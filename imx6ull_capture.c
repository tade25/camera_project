#include <linux/module.h>
#include <linux/platform_device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#define CSICR1_RESET_VAL			0x40000800
#define CSICR2_RESET_VAL			0x0
#define CSICR3_RESET_VAL			0x0

struct imx6ull_csi_reg {
	u32 CSI_CSICR1;
	u32 CSI_CSICR2;
	u32 CSI_CSICR3;
	u32 CSI_CSISTATFIFO;
	u32 CSI_CSIRFIFO;
	u32 CSI_CSIRXCNT;
	u32 CSI_CSISR;
	u32 RESERVED_0x1C;
	u32 CSI_CSIDMASA_STATFIFO;
	u32 CSI_CSIDMATS_STATFIFO;
	u32 CSI_CSIDMASA_FB1;
	u32 CSI_CSIDMASA_FB2;
	u32 CSI_CSIFBUF_PARA;
	u32 CSI_CSIIMAG_PARA;
	u32 RESERVED_0x38;
	u32 RESERVED_0x3C;
	u32 RESERVED_0x40;
	u32 RESERVED_0x44;
	u32 CSI_CSICR18;
	u32 CSI_CSICR19;
};

struct virtual_frame_buf {
	struct vb2_buffer vb;
	struct list_head list;
};

struct virtual_cam {
	struct device* dev;
    struct video_device* vdev;
    struct v4l2_device v4l2_dev;
    struct vb2_queue vb2_q;
    struct list_head queued_bufs;
    struct timer_list stream_timer;
	struct mutex v4l2_lock;
	struct mutex vb_queue_lock;
	spinlock_t queue_lock;
	void __iomem *csi_base;
	int irq_num;
    struct virtual_frame_buf *active_fb1;
    struct virtual_frame_buf *active_fb2;
	struct vb2_alloc_ctx* alloc_ctx;
	struct v4l2_pix_format pix;
};

static struct virtual_cam* cam;

static void csi_enable_int(void __iomem * base, int arg)
{
	struct imx6ull_csi_reg* csi_base = (struct imx6ull_csi_reg*)base;
	u32 cr1;

	cr1 = csi_base->CSI_CSICR1;

	if (arg == 1) {
		cr1 |= (1 << 16);
		cr1 |= (1 << 24);
		cr1 |= (1 << 19);
		cr1 |= (1 << 20);
	} else {
		cr1 &= ~((1 << 16) | (1 << 24) | (1 << 19) | (1 << 20));
	}
	csi_base->CSI_CSICR1 = cr1;
}

static void csi_dmareq_rff_enable(void __iomem * base)
{
	u32 cr3;
	u32 cr2;
	struct imx6ull_csi_reg* csi_base = (struct imx6ull_csi_reg*)base;

	cr3 = csi_base->CSI_CSICR3;
	cr2 = csi_base->CSI_CSICR2;

	/* Burst Type of DMA Transfer from RxFIFO. INCR16 */
	cr2 |= 0xC0000000;

	cr3 |= (1 << 12);
	cr3 |= (1 << 7);
	cr3 &= ~(7 << 4);
	cr3 |= 0x2 << 4;

	csi_base->CSI_CSICR3 = cr3;
	csi_base->CSI_CSICR2 = cr2;
}

static void csi_dmareq_rff_disable(void __iomem * base)
{
	struct imx6ull_csi_reg* csi_base = (struct imx6ull_csi_reg*)base;
	u32 cr3;

	cr3 = csi_base->CSI_CSICR3;

	cr3 &= ~(1 << 12);
	cr3 &= ~(1 << 7);

	csi_base->CSI_CSICR3 = cr3;
}

static void csi_enable(void __iomem * base, int arg)
{
	u32 cr;
	struct imx6ull_csi_reg* csi_base = (struct imx6ull_csi_reg*)base;

	cr = csi_base->CSI_CSICR18;

	if (arg == 1)
		cr |= (1 << 31);
	else
		cr &= ~(1 << 31);
	
	csi_base->CSI_CSICR18 = cr;
}

void imx6ull_csi_init(void __iomem * base)
{
	u32 image_para;

	struct imx6ull_csi_reg* csi_base = (struct imx6ull_csi_reg*)base;

	csi_base->CSI_CSICR3 = (1 << 15);

	/* hw reset */
	csi_base->CSI_CSICR1 = CSICR1_RESET_VAL;
	csi_base->CSI_CSICR2 = CSICR2_RESET_VAL;
	csi_base->CSI_CSICR3 = CSICR3_RESET_VAL;

	/* init */
	csi_base->CSI_CSICR1 |= (1 << 17) | (1 << 12) | (1 << 11) | (1 << 9) | (1 << 8) | (1 << 4) | (1 << 1);

	image_para = (1280 << 16) | 480;
	csi_base->CSI_CSIIMAG_PARA = image_para;

	// csi_base->CSI_CSICR3 = (1 << 14);
}

static void csisw_reset(void __iomem *base)
{
	struct imx6ull_csi_reg* csi_base = (struct imx6ull_csi_reg*)base;
	u32 cr1, cr3, cr18, isr;

	// 原厂软件复位流程，一字不差
	cr18 = csi_base->CSI_CSICR18;
	cr18 &= ~(1 << 31); // 关闭CSI
	csi_base->CSI_CSICR18 = cr18;

	// 清RX FIFO
	cr1 = csi_base->CSI_CSICR1;
	csi_base->CSI_CSICR1 = cr1 & ~(1 << 8);
	cr1 = csi_base->CSI_CSICR1;
	csi_base->CSI_CSICR1 = cr1 | (1 << 5); // BIT_CLR_RXFIFO

	// DMA重刷
	cr3 = csi_base->CSI_CSICR3;
	cr3 |= (1 << 14) | (1 << 15);
	csi_base->CSI_CSICR3 = cr3;
	msleep(2);

	cr1 = csi_base->CSI_CSICR1;
	csi_base->CSI_CSICR1 = cr1 | (1 << 8);

	// 清中断
	isr = csi_base->CSI_CSISR;
	csi_base->CSI_CSISR = isr;

	// 重新开启CSI
	cr18 |= (1 << 31);
	csi_base->CSI_CSICR18 = cr18;
}

void csi_start(void)
{
	struct imx6ull_csi_reg* csi_base = (struct imx6ull_csi_reg*)cam->csi_base;
	u32 status, cr3;
	int timeout, timeout2;
	unsigned long flags;

	csisw_reset(cam->csi_base);

	local_irq_save(flags);

	for (timeout = 10000000; timeout > 0; timeout--) {
        status = csi_base->CSI_CSISR;
        if (status & (1 << 16)) {  // BIT_SOF_INT
            cr3 = csi_base->CSI_CSICR3;
            csi_base->CSI_CSICR3 = cr3 | (1 << 14);  // BIT_DMA_REFLASH_RFF
            
            // 必须等待DMA刷新完成！
            for (timeout2 = 1000000; timeout2 > 0; timeout2--) {
                if (!(csi_base->CSI_CSICR3 & (1 << 14)))  // 等待BIT_DMA_REFLASH_RFF清零
                    break;
                cpu_relax();
            }
            if (timeout2 <= 0) {
                printk("ERROR: DMA reflash timeout\n");
                local_irq_restore(flags);
                return;
            }
            
            break;
        }
        cpu_relax();
    }
    
    if (timeout <= 0) {
        printk("ERROR: Wait SOF timeout\n");
        local_irq_restore(flags);
        return;
    }

	csi_dmareq_rff_enable(cam->csi_base);
	csi_enable_int(cam->csi_base, 1);
	csi_enable(cam->csi_base, 1);

	local_irq_restore(flags);
}

static struct virtual_frame_buf* virtual_get_next_fill_buf(void)
{
	struct virtual_frame_buf* buf = NULL;

	if(!list_empty(&cam->queued_bufs)) {
		buf = list_entry(cam->queued_bufs.next, struct virtual_frame_buf, list);
		list_del(&buf->list);
	}

	return buf;
}

static irqreturn_t csi_irq_handler(int irq, void *para)
{
	struct virtual_cam* cam = (struct virtual_cam* )para;
    struct imx6ull_csi_reg *csi_base = (struct imx6ull_csi_reg *)cam->csi_base;
    struct virtual_frame_buf *done_buf = NULL, *next = NULL;
    u32 status;
    dma_addr_t dma_addr;
    unsigned long flags;

    spin_lock_irqsave(&cam->queue_lock, flags);

    status = csi_base->CSI_CSISR;
    csi_base->CSI_CSISR = 0xffffffff;

	if((status & (1 << 19)) && (status & (1 << 20))) {
		printk("skip two frames\n");
		goto unlock;
	}else if (status & (1 << 19)) {
        done_buf = cam->active_fb1;
        cam->active_fb1 = NULL;

        next = virtual_get_next_fill_buf();
        if (next) {
			next->vb.state = VB2_BUF_STATE_ACTIVE;
            dma_addr = vb2_dma_contig_plane_dma_addr(&next->vb, 0);
            csi_base->CSI_CSIDMASA_FB1 = dma_addr;
            cam->active_fb1 = next;
        }
    }else if (status & (1 << 20)) {
        done_buf = cam->active_fb2;
        cam->active_fb2 = NULL;

        next = virtual_get_next_fill_buf();
        if (next) {
			next->vb.state = VB2_BUF_STATE_ACTIVE;
            dma_addr = vb2_dma_contig_plane_dma_addr(&next->vb, 0);
            csi_base->CSI_CSIDMASA_FB2 = dma_addr;
            cam->active_fb2 = next;
        }
    }

    if (done_buf) {
        vb2_set_plane_payload(&done_buf->vb, 0, 640 * 480 * 2);
        vb2_buffer_done(&done_buf->vb, VB2_BUF_STATE_DONE);
    }

unlock:
    spin_unlock_irqrestore(&cam->queue_lock, flags);
    return IRQ_HANDLED;
}

static int virtual_queue_setup(struct vb2_queue *q, const struct v4l2_format *fmt,
        unsigned int *num_buffers, unsigned int *num_planes,
        unsigned int sizes[], void *alloc_ctxs[])
{
    /* 假装至少需要8个buffer, 每个buffer只有一个plane */
    if((q->num_buffers + *num_buffers) < 8)
        *num_buffers = 8 - q->num_buffers;

    *num_planes = 1;
    sizes[0] = PAGE_ALIGN(640*480*2);

	alloc_ctxs[0] = cam->alloc_ctx;

    return 0;
}

static void virtual_buf_queue(struct vb2_buffer *vb)
{
	struct virtual_frame_buf *buf = container_of(vb, struct virtual_frame_buf, vb);
	unsigned long flags;

	spin_lock_irqsave(&cam->queue_lock, flags);
	list_add_tail(&buf->list, &cam->queued_bufs);
	spin_unlock_irqrestore(&cam->queue_lock, flags);
}

static int virtual_start_streaming(struct vb2_queue *q, unsigned int count)
{
    struct imx6ull_csi_reg* csi_base = (struct imx6ull_csi_reg*)cam->csi_base;
    struct virtual_frame_buf *buf;
    dma_addr_t dma_addr;
    unsigned long flags;

    if (count < 2)
        return -ENOBUFS;

    spin_lock_irqsave(&cam->queue_lock, flags);

    buf = virtual_get_next_fill_buf();
    if (!buf) {
        spin_unlock_irqrestore(&cam->queue_lock, flags);
        return -ENOMEM;
    }
    buf->vb.state = VB2_BUF_STATE_ACTIVE;
    dma_addr = vb2_dma_contig_plane_dma_addr(&buf->vb, 0);
    csi_base->CSI_CSIDMASA_FB1 = dma_addr;
    cam->active_fb1 = buf;

    buf = virtual_get_next_fill_buf();
    if (!buf) {
        spin_unlock_irqrestore(&cam->queue_lock, flags);
        return -ENOMEM;
    }
    buf->vb.state = VB2_BUF_STATE_ACTIVE;
    dma_addr = vb2_dma_contig_plane_dma_addr(&buf->vb, 0);
    csi_base->CSI_CSIDMASA_FB2 = dma_addr;
    cam->active_fb2 = buf;

    spin_unlock_irqrestore(&cam->queue_lock, flags);

    csi_start();

    printk("start streaming\n");

    return 0;
}

static void virtual_stop_streaming(struct vb2_queue *q)
{
	struct virtual_frame_buf *buf;

    /* 停止硬件传输 */
	csi_dmareq_rff_disable(cam->csi_base);
	csi_enable_int(cam->csi_base, 0);
	csi_enable(cam->csi_base, 0);

	while (!list_empty(&cam->queued_bufs)) {
		buf = list_entry(cam->queued_bufs.next,
		struct virtual_frame_buf, list);
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
	}

	cam->active_fb1 = NULL;
    cam->active_fb2 = NULL;
}

static int virtual_videobuf_prepare(struct vb2_buffer *vb)
{
	struct virtual_cam *csi_dev = vb2_get_drv_priv(vb->vb2_queue);

	vb2_set_plane_payload(vb, 0, csi_dev->pix.sizeimage);
	return 0;
}

static struct vb2_ops virtual_vb2_ops = {
	.queue_setup            = virtual_queue_setup,
	.buf_prepare            = virtual_videobuf_prepare,
	.buf_queue              = virtual_buf_queue,
	.start_streaming        = virtual_start_streaming,
	.stop_streaming         = virtual_stop_streaming,
	.wait_prepare           = vb2_ops_wait_prepare,
	.wait_finish            = vb2_ops_wait_finish,
};

static int virtual_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
    strlcpy(cap->driver, "my_virtual_videio", sizeof(cap->driver));
    strlcpy(cap->card, "no_card", sizeof(cap->card));
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

    return 0;
}

static int virtual_enum_fmt_vid_cap(struct file *file, void *fh,
        struct v4l2_fmtdesc *f)
{
    if(f->index > 0)
        return -EINVAL;

    f->pixelformat = V4L2_PIX_FMT_YUYV;
    strlcpy(f->description, "YUV", sizeof(f->description));

    return 0;
}

static int virtual_vidioc_g_fmt_vid_cap(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct v4l2_pix_format* pix = &f->fmt.pix;

    if(V4L2_BUF_TYPE_VIDEO_CAPTURE != f->type)
        return -EINVAL;

    pix->width = 640;
    pix->height = 480;
    pix->pixelformat = V4L2_PIX_FMT_YUYV;
    pix->field = V4L2_FIELD_NONE;
	pix->bytesperline = 640 * 2;
	pix->sizeimage = 640 * 480 * 2;

	return 0;
}

static int virtual_s_fmt_vid_cap(struct file *file, void *fh,
        struct v4l2_format *f)
{
    struct v4l2_pix_format* pix = &f->fmt.pix;

    if(V4L2_BUF_TYPE_VIDEO_CAPTURE != f->type)
        return -EINVAL;

    pix->width = 640;
    pix->height = 480;
    pix->pixelformat = V4L2_PIX_FMT_YUYV;
    pix->field = V4L2_FIELD_NONE;
    pix->bytesperline = 640 * 2;
    pix->sizeimage = 640 * 480 * 2;
	cam->pix = *pix;

    return 0;
}

static int virtual_enum_framesizes(struct file *file, void *fh,
        struct v4l2_frmsizeenum *fsize)
{
    if(fsize->index > 0)
        return -EINVAL;

    fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
    fsize->discrete.width = 640;
    fsize->discrete.height = 480;

    return 0;
}

static const struct v4l2_ioctl_ops virtual_ioctl_ops = {
	.vidioc_querycap          = virtual_querycap,

	.vidioc_enum_fmt_vid_cap  = virtual_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap	  = virtual_vidioc_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap     = virtual_s_fmt_vid_cap,
    .vidioc_enum_framesizes   = virtual_enum_framesizes,

	.vidioc_reqbufs           = vb2_ioctl_reqbufs,
	.vidioc_create_bufs       = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf       = vb2_ioctl_prepare_buf,
	.vidioc_querybuf          = vb2_ioctl_querybuf,
	.vidioc_qbuf              = vb2_ioctl_qbuf,
	.vidioc_dqbuf             = vb2_ioctl_dqbuf,
	.vidioc_expbuf			  = vb2_ioctl_expbuf,

	.vidioc_streamon          = vb2_ioctl_streamon,
	.vidioc_streamoff         = vb2_ioctl_streamoff,
};

static const struct v4l2_file_operations virtual_fops = {
	.owner                    = THIS_MODULE,
	.open                     = v4l2_fh_open,
	.release                  = vb2_fop_release,
	.read                     = vb2_fop_read,
	.poll                     = vb2_fop_poll,
	.mmap                     = vb2_fop_mmap,
	.unlocked_ioctl           = video_ioctl2,
};

int my_imx6ull_csi_probe(struct platform_device  * pdev)
{
    int ret;
	struct resource * res;
	struct clk * axi;
	struct clk * mclk;
	struct clk * dcic;

	cam = kzalloc(sizeof(*cam), GFP_KERNEL);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	cam->csi_base = devm_ioremap_resource(&pdev->dev, res);
	cam->irq_num = platform_get_irq(pdev, 0);

	printk("irq_num: %d\n", cam->irq_num);

	axi = devm_clk_get(&pdev->dev, "disp-axi");
	mclk = devm_clk_get(&pdev->dev, "csi_mclk");
	dcic = devm_clk_get(&pdev->dev, "disp_dcic");

	clk_prepare_enable(axi);
	clk_prepare_enable(mclk);
	clk_prepare_enable(dcic);

	ret = devm_request_irq(&pdev->dev, cam->irq_num, csi_irq_handler, 0, "csi", (void *)cam);
	if(ret < 0) {
		printk("failed to request irq\n");
		return -1;
	}

	imx6ull_csi_init(cam->csi_base);

	cam->dev = &pdev->dev;
	cam->alloc_ctx = vb2_dma_contig_init_ctx(cam->dev);;

    cam->vdev = video_device_alloc();

	mutex_init(&cam->v4l2_lock);
	mutex_init(&cam->vb_queue_lock);
	INIT_LIST_HEAD(&cam->queued_bufs);
	spin_lock_init(&cam->queue_lock);

    /* Init videobuf2 queue structure */
	cam->vb2_q.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	cam->vb2_q.io_modes = VB2_MMAP | VB2_DMABUF;
	cam->vb2_q.drv_priv = cam;
	cam->vb2_q.buf_struct_size = sizeof(struct virtual_frame_buf);
	cam->vb2_q.ops = &virtual_vb2_ops;
	cam->vb2_q.mem_ops = &vb2_dma_contig_memops;
	cam->vb2_q.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	cam->vb2_q.lock = &cam->vb_queue_lock;
	cam->vb2_q.min_buffers_needed = 2;

	ret = vb2_queue_init(&cam->vb2_q);
	if (ret) {
        printk("failed to init queue\n");
        return -1;
	}

	cam->vdev->queue = &cam->vb2_q;

    /* Register the v4l2_device structure */
    strlcpy(cam->v4l2_dev.name, "virtual_dev", sizeof(cam->v4l2_dev.name));
    ret = v4l2_device_register(&pdev->dev, &cam->v4l2_dev);
    if(ret < 0) {
        printk("falied to register v4l2 device\n");
        return -1;
    }

    cam->vdev->v4l2_dev = &cam->v4l2_dev;
	cam->vdev->lock = &cam->v4l2_lock;

    /* Register video_device structure */
    cam->vdev->release = video_device_release_empty;
    cam->vdev->fops = &virtual_fops;
    cam->vdev->ioctl_ops = &virtual_ioctl_ops;
    ret = video_register_device(cam->vdev, VFL_TYPE_GRABBER, -1);
    if(ret < 0) {
        printk("falied to register video device\n");
        return -1;
    }

	printk("imx6ull csi probe\n");

	return 0;
}

int my_imx6ull_csi_remove(struct platform_device *pdev)
{
    video_unregister_device(cam->vdev);
    v4l2_device_unregister(&cam->v4l2_dev);
	if (cam->alloc_ctx)
		vb2_dma_contig_cleanup_ctx(cam->alloc_ctx);
	kfree(cam);
	return 0;
}

static const struct of_device_id my_imx6ull_csi_of_match[] = {
	{.compatible = "myboard,imx6ul-csi",},
	{},
};

static struct platform_driver myboard_csi_driver = {
	.probe = my_imx6ull_csi_probe,
	.remove = my_imx6ull_csi_remove,
	.driver = {
		.name = "csi",
		.of_match_table = my_imx6ull_csi_of_match,
	},
};

module_platform_driver(myboard_csi_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("My CSI Driver");
