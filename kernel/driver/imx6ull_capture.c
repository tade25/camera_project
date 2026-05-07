#include <linux/module.h>
#include <linux/platform_device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-subdev.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/of_graph.h>
#include <linux/of.h>

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

struct imx6ull_csi_frame_buf {
	struct vb2_buffer vb;
	struct list_head list;
};

struct imx6ull_csi_dev {
    void __iomem *csi_base;
    struct imx6ull_csi_frame_buf *active_fb1;
    struct imx6ull_csi_frame_buf *active_fb2;
    struct list_head queued_bufs;
    spinlock_t queue_lock;

    struct vb2_queue vb2_q;
    struct mutex vb_queue_lock;
    struct mutex v4l2_lock;
    struct v4l2_pix_format pix;

    struct video_device *vdev;
    struct v4l2_device v4l2_dev;
    struct v4l2_subdev *sensor_sd;

    struct device *dev;
    struct clk *axi;
    struct clk *mclk;
    struct clk *dcic;
    struct vb2_alloc_ctx *alloc_ctx;

    struct v4l2_async_notifier subdev_notifier;
    struct v4l2_async_subdev asd;
    struct v4l2_async_subdev *async_subdevs[2];
};

static void csi_enable_int(struct imx6ull_csi_reg* csi_base, int arg)
{
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

static void csi_dmareq_rff_enable(struct imx6ull_csi_reg* csi_base)
{
	u32 cr3;
	u32 cr2;

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

static void csi_dmareq_rff_disable(struct imx6ull_csi_reg* csi_base)
{
	u32 cr3;

	cr3 = csi_base->CSI_CSICR3;

	cr3 &= ~(1 << 12);
	cr3 &= ~(1 << 7);

	csi_base->CSI_CSICR3 = cr3;
}

static void csi_enable(struct imx6ull_csi_reg* csi_base, int arg)
{
	u32 cr;

	cr = csi_base->CSI_CSICR18;

	if (arg == 1)
		cr |= (1 << 31);
	else
		cr &= ~(1 << 31);
	
	csi_base->CSI_CSICR18 = cr;
}

void imx6ull_csi_init(struct imx6ull_csi_reg* csi_base)
{
	csi_base->CSI_CSICR3 = (1 << 15);

	/* hw reset */
	csi_base->CSI_CSICR1 = CSICR1_RESET_VAL;
	csi_base->CSI_CSICR2 = CSICR2_RESET_VAL;
	csi_base->CSI_CSICR3 = CSICR3_RESET_VAL;

	/* init */
	csi_base->CSI_CSICR1 |= (1 << 17) | (1 << 12) | (1 << 11) | (1 << 9) | (1 << 8) | (1 << 4) | (1 << 1);

	csi_base->CSI_CSIFBUF_PARA = 0;
	csi_base->CSI_CSIIMAG_PARA = (1280 << 16) | 480;;

	csi_base->CSI_CSICR3 = (1 << 14);
}

static void csisw_reset(struct imx6ull_csi_reg* csi_base)
{
	u32 cr1, cr3, cr18, isr;

	cr18 = csi_base->CSI_CSICR18;
	cr18 &= ~(1 << 31);
	csi_base->CSI_CSICR18 = cr18;

	cr1 = csi_base->CSI_CSICR1;
	csi_base->CSI_CSICR1 = cr1 & ~(1 << 8);
	cr1 = csi_base->CSI_CSICR1;
	csi_base->CSI_CSICR1 = cr1 | (1 << 5); // BIT_CLR_RXFIFO

	cr3 = csi_base->CSI_CSICR3;
	cr3 |= (1 << 14) | (1 << 15);
	csi_base->CSI_CSICR3 = cr3;
	msleep(2);

	cr1 = csi_base->CSI_CSICR1;
	csi_base->CSI_CSICR1 = cr1 | (1 << 8);

	isr = csi_base->CSI_CSISR;
	csi_base->CSI_CSISR = isr;

	cr18 |= (1 << 31);
	csi_base->CSI_CSICR18 = cr18;
}

static void csi_start(struct imx6ull_csi_reg* csi_base)
{
	u32 status, cr3;
	int timeout, timeout2;
	unsigned long flags;

	csisw_reset(csi_base);

	local_irq_save(flags);

	for (timeout = 10000000; timeout > 0; timeout--) {
        status = csi_base->CSI_CSISR;
        if (status & (1 << 16)) {
            cr3 = csi_base->CSI_CSICR3;
            csi_base->CSI_CSICR3 = cr3 | (1 << 14);
            
            for (timeout2 = 1000000; timeout2 > 0; timeout2--) {
                if (!(csi_base->CSI_CSICR3 & (1 << 14)))
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

	csi_dmareq_rff_enable(csi_base);
	csi_enable_int(csi_base, 1);
	csi_enable(csi_base, 1);

	local_irq_restore(flags);
}

static struct imx6ull_csi_frame_buf* virtual_get_next_fill_buf(struct imx6ull_csi_dev* csi_dev)
{
	struct imx6ull_csi_frame_buf* buf = NULL;

	if(!list_empty(&csi_dev->queued_bufs)) {
		buf = list_entry(csi_dev->queued_bufs.next, struct imx6ull_csi_frame_buf, list);
		list_del(&buf->list);
	}

	return buf;
}

static irqreturn_t csi_irq_handler(int irq, void *para)
{
	struct imx6ull_csi_dev* csi_dev = (struct imx6ull_csi_dev* )para;
    struct imx6ull_csi_reg *csi_base = (struct imx6ull_csi_reg *)csi_dev->csi_base;
    struct imx6ull_csi_frame_buf *done_buf = NULL, *next = NULL;
    u32 status;
    dma_addr_t dma_addr;
    unsigned long flags;

    spin_lock_irqsave(&csi_dev->queue_lock, flags);

    status = csi_base->CSI_CSISR;
    csi_base->CSI_CSISR = 0xffffffff;

	if(status & (1 << 24)) {
		printk("rxfifo overrun\n");
	}

	if(status & (1 << 7)) {
		printk("hresponse Error\n");
	}

	if((status & (1 << 19)) && (status & (1 << 20))) {
		printk("skip two frames\n");
		goto unlock;
	}else if (status & (1 << 19)) {
        done_buf = csi_dev->active_fb1;
        csi_dev->active_fb1 = NULL;

        next = virtual_get_next_fill_buf(csi_dev);
        if (next) {
			next->vb.state = VB2_BUF_STATE_ACTIVE;
            dma_addr = vb2_dma_contig_plane_dma_addr(&next->vb, 0);
            csi_base->CSI_CSIDMASA_FB1 = dma_addr;
            csi_dev->active_fb1 = next;
        }
    }else if (status & (1 << 20)) {
        done_buf = csi_dev->active_fb2;
        csi_dev->active_fb2 = NULL;

        next = virtual_get_next_fill_buf(csi_dev);
        if (next) {
			next->vb.state = VB2_BUF_STATE_ACTIVE;
            dma_addr = vb2_dma_contig_plane_dma_addr(&next->vb, 0);
            csi_base->CSI_CSIDMASA_FB2 = dma_addr;
            csi_dev->active_fb2 = next;
        }
    }

    if (done_buf) {
        vb2_set_plane_payload(&done_buf->vb, 0, csi_dev->pix.sizeimage);
        vb2_buffer_done(&done_buf->vb, VB2_BUF_STATE_DONE);
    }

unlock:
    spin_unlock_irqrestore(&csi_dev->queue_lock, flags);
    return IRQ_HANDLED;
}

static int imx6ull_queue_setup(struct vb2_queue *q, const struct v4l2_format *fmt,
        unsigned int *num_buffers, unsigned int *num_planes,
        unsigned int sizes[], void *alloc_ctxs[])
{
	struct imx6ull_csi_dev *csi_dev = vb2_get_drv_priv(q);

    if((q->num_buffers + *num_buffers) < 8)
        *num_buffers = 8 - q->num_buffers;

    *num_planes = 1;
    sizes[0] = PAGE_ALIGN(csi_dev->pix.sizeimage);

	alloc_ctxs[0] = csi_dev->alloc_ctx;

    return 0;
}

static void imx6ull_buf_queue(struct vb2_buffer *vb)
{
	struct imx6ull_csi_dev *csi_dev = vb2_get_drv_priv(vb->vb2_queue);
	struct imx6ull_csi_frame_buf *buf = container_of(vb, struct imx6ull_csi_frame_buf, vb);
	unsigned long flags;

	spin_lock_irqsave(&csi_dev->queue_lock, flags);
	list_add_tail(&buf->list, &csi_dev->queued_bufs);
	spin_unlock_irqrestore(&csi_dev->queue_lock, flags);
}

static int imx6ull_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct imx6ull_csi_dev *csi_dev = vb2_get_drv_priv(q);
    struct imx6ull_csi_reg* csi_base = (struct imx6ull_csi_reg*)csi_dev->csi_base;
    struct imx6ull_csi_frame_buf *buf;
    dma_addr_t dma_addr;
    unsigned long flags;

    if (count < 2)
        return -ENOBUFS;

    spin_lock_irqsave(&csi_dev->queue_lock, flags);

    buf = virtual_get_next_fill_buf(csi_dev);
    if (!buf) {
        spin_unlock_irqrestore(&csi_dev->queue_lock, flags);
        return -ENOMEM;
    }
    buf->vb.state = VB2_BUF_STATE_ACTIVE;
    dma_addr = vb2_dma_contig_plane_dma_addr(&buf->vb, 0);
    csi_base->CSI_CSIDMASA_FB1 = dma_addr;
    csi_dev->active_fb1 = buf;

    buf = virtual_get_next_fill_buf(csi_dev);
    if (!buf) {
        spin_unlock_irqrestore(&csi_dev->queue_lock, flags);
        return -ENOMEM;
    }
    buf->vb.state = VB2_BUF_STATE_ACTIVE;
    dma_addr = vb2_dma_contig_plane_dma_addr(&buf->vb, 0);
    csi_base->CSI_CSIDMASA_FB2 = dma_addr;
    csi_dev->active_fb2 = buf;

    spin_unlock_irqrestore(&csi_dev->queue_lock, flags);

    csi_start(csi_base);

    printk("start streaming\n");

    return 0;
}

static void imx6ull_stop_streaming(struct vb2_queue *q)
{
	struct imx6ull_csi_dev *csi_dev = vb2_get_drv_priv(q);
	struct imx6ull_csi_frame_buf *buf;
	unsigned long flags;

    /* 停止硬件传输 */
	csi_dmareq_rff_disable(csi_dev->csi_base);
	csi_enable_int(csi_dev->csi_base, 0);
	csi_enable(csi_dev->csi_base, 0);

	msleep(10);

	spin_lock_irqsave(&csi_dev->queue_lock, flags);

	if(VB2_BUF_STATE_ACTIVE == csi_dev->active_fb1->vb.state) {
		vb2_buffer_done(&csi_dev->active_fb1->vb, VB2_BUF_STATE_ERROR);
	}

	if(VB2_BUF_STATE_ACTIVE == csi_dev->active_fb2->vb.state) {
		vb2_buffer_done(&csi_dev->active_fb2->vb, VB2_BUF_STATE_ERROR);
	}

	while (!list_empty(&csi_dev->queued_bufs)) {
		buf = list_entry(csi_dev->queued_bufs.next,
		struct imx6ull_csi_frame_buf, list);
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
	}

	spin_unlock_irqrestore(&csi_dev->queue_lock, flags);
}

static int imx6ull_videobuf_prepare(struct vb2_buffer *vb)
{
	struct imx6ull_csi_dev *csi_dev = vb2_get_drv_priv(vb->vb2_queue);

	vb2_set_plane_payload(vb, 0, csi_dev->pix.sizeimage);
	return 0;
}

static struct vb2_ops imx6ull_vb2_ops = {
	.queue_setup            = imx6ull_queue_setup,
	.buf_prepare            = imx6ull_videobuf_prepare,
	.buf_queue              = imx6ull_buf_queue,
	.start_streaming        = imx6ull_start_streaming,
	.stop_streaming         = imx6ull_stop_streaming,
	.wait_prepare           = vb2_ops_wait_prepare,
	.wait_finish            = vb2_ops_wait_finish,
};

static int imx6ull_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
    strlcpy(cap->driver, "my_virtual_videio", sizeof(cap->driver));
    strlcpy(cap->card, "no_card", sizeof(cap->card));
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

    return 0;
}

static int imx6ull_enum_fmt_vid_cap(struct file *file, void *fh,
        struct v4l2_fmtdesc *f)
{
	struct imx6ull_csi_dev *csi_dev = video_drvdata(file);
	struct v4l2_subdev* sd = csi_dev->sensor_sd;
	u32 code;

    if(f->index > 0)
        return -EINVAL;

	v4l2_subdev_call(sd, video, enum_mbus_fmt, f->index, &code);

	if(MEDIA_BUS_FMT_YUYV8_2X8 == code) {
		f->pixelformat = V4L2_PIX_FMT_YUYV;
		strlcpy(f->description, "YUVV", sizeof(f->description));
	}

    return 0;
}

static int imx6ull_vidioc_g_fmt_vid_cap(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct imx6ull_csi_dev *csi_dev = video_drvdata(file);
	struct v4l2_pix_format* pix = &f->fmt.pix;

    if(V4L2_BUF_TYPE_VIDEO_CAPTURE != f->type)
        return -EINVAL;

	*pix = csi_dev->pix;

	return 0;
}

static int imx6ull_s_fmt_vid_cap(struct file *file, void *fh,
        struct v4l2_format *f)
{
	struct imx6ull_csi_dev *csi_dev = video_drvdata(file);
	struct imx6ull_csi_reg* csi_base = (struct imx6ull_csi_reg*)csi_dev->csi_base;
    struct v4l2_pix_format* pix = &f->fmt.pix;
	struct v4l2_subdev* sd = csi_dev->sensor_sd;
	struct v4l2_mbus_framefmt fmt;
	int ret;

    if(V4L2_BUF_TYPE_VIDEO_CAPTURE != f->type)
        return -EINVAL;
	
	fmt.width = pix->width;
	fmt.height = pix->height;
	ret = v4l2_subdev_call(sd, video, s_mbus_fmt, &fmt);
	if(ret < 0)
		return ret;
	
	pix->width = fmt.width;
	pix->height = fmt.height;
	pix->field = V4L2_FIELD_NONE;
	pix->bytesperline = 2 * fmt.width;
	pix->sizeimage = 2 * fmt.width * fmt.height;

	csi_dev->pix = *pix;

	csi_base->CSI_CSIIMAG_PARA = (2 * fmt.width << 16) | fmt.height;;
	csi_base->CSI_CSICR3 |= (1 << 14);

    return 0;
}

static int imx6ull_vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct imx6ull_csi_dev *csi_dev = video_drvdata(file);
	int ret;

	WARN_ON(priv != file->private_data);

	ret = vb2_querybuf(&csi_dev->vb2_q, p);

	if (!ret) {
		/* return physical address */
		struct vb2_buffer *vb = csi_dev->vb2_q.bufs[p->index];

		if (p->flags & V4L2_BUF_FLAG_MAPPED)
			p->m.offset = vb2_dma_contig_plane_dma_addr(vb, 0);
	}
	return ret;
}

static int imx6ull_vidioc_g_parm(struct file *file, void *fh,
		struct v4l2_streamparm *a)
{
	struct imx6ull_csi_dev *csi_dev = video_drvdata(file);
	struct v4l2_subdev* sd = csi_dev->sensor_sd;
	struct v4l2_streamparm streamparm;
	int ret;

	ret = v4l2_subdev_call(sd, video, g_parm, &streamparm);
	if(ret < 0)
		return ret;

	*a = streamparm;

	return 0;
}

static int imx6ull_vidioc_s_parm(struct file *file, void *fh,
		struct v4l2_streamparm *a)
{
	struct imx6ull_csi_dev *csi_dev = video_drvdata(file);
	struct v4l2_subdev* sd = csi_dev->sensor_sd;
	struct v4l2_streamparm streamparm;
	int ret;

	ret = v4l2_subdev_call(sd, video, s_parm, &streamparm);
	if(ret < 0)
		return ret;

	*a = streamparm;

	return 0;
}

static int imx6ull_enum_framesizes(struct file *file, void *fh,
        struct v4l2_frmsizeenum *fsize)
{
	struct imx6ull_csi_dev *csi_dev = video_drvdata(file);
	struct v4l2_subdev* sd = csi_dev->sensor_sd;
	struct v4l2_subdev_frame_size_enum frame_size;
	int ret;

    if(V4L2_PIX_FMT_YUYV !=  fsize->pixel_format)
        return -EINVAL;

	frame_size.index = fsize->index;
	ret = v4l2_subdev_call(sd, pad, enum_frame_size, NULL, &frame_size);
	if(ret < 0)
		return ret;

	if((frame_size.min_width == frame_size.max_width) &&\
		(frame_size.min_height == frame_size.max_height)) {
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frame_size.min_width;
		fsize->discrete.height = frame_size.min_height;
	}

    return 0;
}

static int imx6ull_enum_frameintervals(struct file *file, void *priv,
		struct v4l2_frmivalenum *interval)
{
	struct imx6ull_csi_dev *csi_dev = video_drvdata(file);
	struct v4l2_subdev* sd = csi_dev->sensor_sd;
	struct v4l2_subdev_frame_interval_enum frame_interval;
	int ret;

	if(V4L2_PIX_FMT_YUYV !=  interval->pixel_format)
		return -EINVAL;

	frame_interval.index = interval->index;
	frame_interval.width = interval->width;
	frame_interval.height = interval->height;
	ret = v4l2_subdev_call(sd, pad, enum_frame_interval, NULL, &frame_interval);
	if(ret < 0)
		return ret;

	interval->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	interval->discrete.numerator = frame_interval.interval.numerator;
	interval->discrete.denominator = frame_interval.interval.denominator;

	return 0;
}

static const struct v4l2_ioctl_ops imx6ull_ioctl_ops = {
	.vidioc_querycap          	= imx6ull_querycap,
	.vidioc_enum_fmt_vid_cap  	= imx6ull_enum_fmt_vid_cap,

	.vidioc_g_fmt_vid_cap	  	= imx6ull_vidioc_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap     	= imx6ull_s_fmt_vid_cap,

	.vidioc_streamon          	= vb2_ioctl_streamon,
	.vidioc_streamoff         	= vb2_ioctl_streamoff,

	.vidioc_reqbufs           	= vb2_ioctl_reqbufs,
	.vidioc_create_bufs       	= vb2_ioctl_create_bufs,
	.vidioc_prepare_buf       	= vb2_ioctl_prepare_buf,
	.vidioc_querybuf          	= imx6ull_vidioc_querybuf,
	.vidioc_qbuf              	= vb2_ioctl_qbuf,
	.vidioc_dqbuf             	= vb2_ioctl_dqbuf,
	.vidioc_expbuf			  	= vb2_ioctl_expbuf,

	.vidioc_g_parm				= imx6ull_vidioc_g_parm,
	.vidioc_s_parm				= imx6ull_vidioc_s_parm,

	.vidioc_enum_framesizes   	= imx6ull_enum_framesizes,
	.vidioc_enum_frameintervals = imx6ull_enum_frameintervals,
};

static const struct v4l2_file_operations imx6ull_fops = {
	.owner                    = THIS_MODULE,
	.open                     = v4l2_fh_open,
	.release                  = vb2_fop_release,
	.read                     = vb2_fop_read,
	.poll                     = vb2_fop_poll,
	.mmap                     = vb2_fop_mmap,
	.unlocked_ioctl           = video_ioctl2,
};

static int imx6ull_csi_async_bound(struct v4l2_async_notifier *notifier,
		struct v4l2_subdev *subdev,
		struct v4l2_async_subdev *asd)
{
	struct imx6ull_csi_dev *csi_dev = container_of(notifier, struct imx6ull_csi_dev, subdev_notifier);

	/* 都是ov5640: ov5640@3c这个节点 */
	if (csi_dev->asd.match.of.node == subdev->dev->of_node)
		csi_dev->sensor_sd = subdev;

	if (subdev == NULL)
		return -EINVAL;
	
	return 0;
}

static int imx6ull_csi_register_subdev(struct imx6ull_csi_dev* csi_dev)
{
	struct device_node* endpoint;
	struct device_node* remote;
	int ret;

	endpoint = of_graph_get_next_endpoint(csi_dev->dev->of_node, NULL);
	if(!endpoint) {
		dev_info(csi_dev->dev, "failed to get endpoint\n");
		return -EINVAL;
	}

	remote = of_graph_get_remote_port_parent(endpoint);
	if(!remote) {
		dev_info(csi_dev->dev, "failed to get remote\n");
		of_node_put(remote);
		return -EINVAL;
	}

	csi_dev->asd.match_type = V4L2_ASYNC_MATCH_OF;
    csi_dev->asd.match.of.node = remote;
    csi_dev->async_subdevs[0] = &csi_dev->asd;

	of_node_put(endpoint);
	of_node_put(remote);

    csi_dev->subdev_notifier.subdevs = csi_dev->async_subdevs;
    csi_dev->subdev_notifier.num_subdevs = 1;
    csi_dev->subdev_notifier.bound = imx6ull_csi_async_bound;

	ret = v4l2_async_notifier_register(&csi_dev->v4l2_dev, &csi_dev->subdev_notifier);

	return ret;
}

int my_imx6ull_csi_probe(struct platform_device  * pdev)
{
	struct imx6ull_csi_dev* csi_dev;
	struct resource* res;
	struct clk * axi;
	struct clk * mclk;
	struct clk * dcic;
	int irq;
	int ret;

	csi_dev = devm_kzalloc(&pdev->dev, sizeof(*csi_dev), GFP_KERNEL);
	if(!csi_dev) {
		dev_err(&pdev->dev, "failed to alloc device private data\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, (void*)csi_dev);

	csi_dev->pix.width = 640;
	csi_dev->pix.height = 480;
	csi_dev->pix.pixelformat = V4L2_PIX_FMT_YUYV;
	csi_dev->pix.field = V4L2_FIELD_NONE;
	csi_dev->pix.bytesperline = 640 * 2;
	csi_dev->pix.sizeimage = 640 * 480 * 2;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	csi_dev->csi_base = devm_ioremap_resource(&pdev->dev, res);
	if(IS_ERR(csi_dev->csi_base))
		return PTR_ERR(csi_dev->csi_base);

	irq = platform_get_irq(pdev, 0);
	if(irq < 0)
		return irq;

	ret = devm_request_irq(&pdev->dev, irq, csi_irq_handler, 0, "csi", (void *)csi_dev);
	if(ret < 0) {
		dev_err(&pdev->dev, "IRQ %d request failed\n", irq);
		return ret;
	}

	axi = devm_clk_get(&pdev->dev, "disp-axi");
	if(IS_ERR(axi))
		return PTR_ERR(axi);

	mclk = devm_clk_get(&pdev->dev, "csi_mclk");
	if(IS_ERR(mclk))
		return PTR_ERR(mclk);
	
	dcic = devm_clk_get(&pdev->dev, "disp_dcic");
	if(IS_ERR(dcic))
		return PTR_ERR(dcic);

	ret = clk_prepare_enable(axi);
	if(ret < 0)
		return ret;
	
	ret = clk_prepare_enable(mclk);
	if(ret < 0)
		goto err_prepare_axi;

	ret = clk_prepare_enable(dcic);
	if(ret < 0)
		goto err_prepare_mclk;
	
	imx6ull_csi_init(csi_dev->csi_base);

	csi_dev->dev = &pdev->dev;
	csi_dev->alloc_ctx = vb2_dma_contig_init_ctx(csi_dev->dev);
	if(IS_ERR(csi_dev->alloc_ctx))	{
		dev_err(&pdev->dev, "init contig dma failed\n");
		ret = PTR_ERR(csi_dev->alloc_ctx);
		goto err_dma_contig_init;	
	}

    csi_dev->vdev = video_device_alloc();
	if(!csi_dev->vdev) {
		dev_err(&pdev->dev, "alloc video device failed\n");
		ret = -ENOMEM;
		goto err_alloc_video_device;
	}

	video_set_drvdata(csi_dev->vdev, (void*)csi_dev);

	mutex_init(&csi_dev->v4l2_lock);
	mutex_init(&csi_dev->vb_queue_lock);
	INIT_LIST_HEAD(&csi_dev->queued_bufs);
	spin_lock_init(&csi_dev->queue_lock);

    /* Init videobuf2 queue structure */
	csi_dev->vb2_q.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	csi_dev->vb2_q.io_modes = VB2_MMAP | VB2_DMABUF;
	csi_dev->vb2_q.drv_priv = csi_dev;
	csi_dev->vb2_q.buf_struct_size = sizeof(struct imx6ull_csi_frame_buf);
	csi_dev->vb2_q.ops = &imx6ull_vb2_ops;
	csi_dev->vb2_q.mem_ops = &vb2_dma_contig_memops;
	csi_dev->vb2_q.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	csi_dev->vb2_q.lock = &csi_dev->vb_queue_lock;
	csi_dev->vb2_q.min_buffers_needed = 2;

	ret = vb2_queue_init(&csi_dev->vb2_q);
	if (ret) {
        dev_err(&pdev->dev, "failed to init queue\n");
        goto err_vb2;
	}

	csi_dev->vdev->queue = &csi_dev->vb2_q;

    /* Register the v4l2_device structure */
    strlcpy(csi_dev->v4l2_dev.name, "imx6ull_dev", sizeof(csi_dev->v4l2_dev.name));
    ret = v4l2_device_register(&pdev->dev, &csi_dev->v4l2_dev);
    if(ret < 0)
        goto err_v4l2_device;

    csi_dev->vdev->v4l2_dev = &csi_dev->v4l2_dev;
	csi_dev->vdev->lock = &csi_dev->v4l2_lock;

    /* Register video_device structure */
    csi_dev->vdev->release = video_device_release_empty;
    csi_dev->vdev->fops = &imx6ull_fops;
    csi_dev->vdev->ioctl_ops = &imx6ull_ioctl_ops;
    ret = video_register_device(csi_dev->vdev, VFL_TYPE_GRABBER, -1);
    if(ret < 0)
       	goto err_video_device;

	ret = imx6ull_csi_register_subdev(csi_dev);
	if(ret < 0)
        goto err_subdev;

	dev_info(&pdev->dev, "IMX6ULL CSI controller probe\n");

	return 0;

err_subdev:
	video_unregister_device(csi_dev->vdev);
err_video_device:
	v4l2_device_unregister(&csi_dev->v4l2_dev);
err_v4l2_device:
	vb2_queue_release(&csi_dev->vb2_q);
err_vb2:
	video_device_release(csi_dev->vdev);
err_alloc_video_device:
	vb2_dma_contig_cleanup_ctx(csi_dev->alloc_ctx);
err_dma_contig_init:
	clk_disable_unprepare(dcic);
err_prepare_mclk:
	clk_disable_unprepare(mclk);
err_prepare_axi:
	clk_disable_unprepare(axi);

	return ret;
}

int my_imx6ull_csi_remove(struct platform_device *pdev)
{
	struct imx6ull_csi_dev* csi_dev = platform_get_drvdata(pdev);

	v4l2_async_notifier_unregister(&csi_dev->subdev_notifier);
    video_unregister_device(csi_dev->vdev);
    v4l2_device_unregister(&csi_dev->v4l2_dev);
	vb2_queue_release(&csi_dev->vb2_q);
	video_device_release(csi_dev->vdev);
	vb2_dma_contig_cleanup_ctx(csi_dev->alloc_ctx);

	clk_disable_unprepare(csi_dev->dcic);
	clk_disable_unprepare(csi_dev->mclk);
	clk_disable_unprepare(csi_dev->axi);

	dev_info(&pdev->dev, "IMX6ULL CSI controller remove\n");

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
