#include "lcd.h"
#include <linux/fb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

int lcd_init(Lcd_DevType* dev, const char* file_name)
{
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;

    if((NULL == dev) || (NULL == file_name))
        goto err_open;

    dev->fd = open(file_name, O_RDWR);
    if(dev->fd < 0) {
        perror("open /dev/fbx failed\n");
        goto err_open;
    }

    if(ioctl(dev->fd, FBIOGET_VSCREENINFO, &var)) {
        perror("get var screen info failed\n");
        goto err_close;
    }

    if(ioctl(dev->fd, FBIOGET_FSCREENINFO, &fix)) {
        perror("get fix screen info failed\n");
        goto err_close;
    }

    dev->xres = var.xres;
    dev->yres = var.yres;
    dev->phy_addr = fix.smem_start;

    return 0;

err_close:
    close(dev->fd);
err_open:
    return -1;
}

void lcd_release(Lcd_DevType* dev)
{
    if(NULL == dev) return;

    close(dev->fd);
}
