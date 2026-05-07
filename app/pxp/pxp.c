#include "pxp.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "pxp_user.h"

int pxp_init(Pxp_DevType* pDev, uint16_t cam_w, uint16_t cam_h)
{
    pDev->fd = open("/dev/pxp_device", O_RDWR);
    if (pDev->fd < 0) {
        perror("open /dev/pxp_device failed");
        goto err_open;
    }

    memset(&pDev->chan_handle, 0, sizeof(pDev->chan_handle));
    if (ioctl(pDev->fd, PXP_IOC_GET_CHAN, &pDev->chan_handle) < 0) {
        perror("PXP_IOC_GET_CHAN failed");
        goto err_close;
    }

    return 0;
err_close:
    close(pDev->fd);
err_open:
    return -1;
}

int pxp_yuyv_to_rgb565(void* dev, uint32_t cam_buf, uint32_t fb_buf, int cam_w, int cam_h)
{
    Pxp_DevType* p_dev = (Pxp_DevType*)dev;
    struct pxp_config_data config_data;
    struct pxp_mem_flush mem_flush;
    int ret;

    if (!p_dev || !cam_buf || !fb_buf) {
        perror("pxp param error");
        return -1;
    }

    memset(&config_data, 0, sizeof(config_data));
    config_data.handle = p_dev->chan_handle.handle;
    config_data.layer_nr = 1;

    config_data.proc_data.engine_enable = 0;
    config_data.proc_data.yuv = 1;
    config_data.proc_data.hflip = 0;
    config_data.proc_data.vflip = 0;

    config_data.proc_data.srect.left = 0;
    config_data.proc_data.srect.top = 0;
    config_data.proc_data.srect.width = cam_w;
    config_data.proc_data.srect.height = cam_h;

    config_data.proc_data.drect.left = 0;
    config_data.proc_data.drect.top = 0;
    config_data.proc_data.drect.width = 1024;
    config_data.proc_data.drect.height = 600;

    config_data.s0_param.paddr = cam_buf;
    config_data.s0_param.width = cam_w;
    config_data.s0_param.height = cam_h;
    config_data.s0_param.stride = cam_w * 2;
    config_data.s0_param.pixel_fmt = PXP_PIX_FMT_YUYV;

    config_data.out_param.paddr = fb_buf;
    config_data.out_param.width = 1024;
    config_data.out_param.height = 600;
    config_data.out_param.stride = 1024 * 2;
    config_data.out_param.pixel_fmt = PXP_PIX_FMT_RGB565;

    ret = ioctl(p_dev->fd, PXP_IOC_CONFIG_CHAN, &config_data);
    if (ret < 0) {
        perror("PXP_IOC_CONFIG_CHAN failed");
        return -1;
    }

    ret = ioctl(p_dev->fd, PXP_IOC_START_CHAN, &p_dev->chan_handle);
    if (ret < 0) {
        perror("PXP_IOC_START_CHAN falied");
        return -1;
    }

    ret = ioctl(p_dev->fd, PXP_IOC_WAIT4CMPLT, &p_dev->chan_handle);
    if (ret < 0) {
        perror("PXP_IOC_WAIT4CMPLT failed");
        return -1;
    }

    return 0;
}

void pxp_release(Pxp_DevType* pDev)
{
    ioctl(pDev->fd, PXP_IOC_PUT_CHAN, &pDev->chan_handle);
    close(pDev->fd);
}
