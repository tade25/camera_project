#include "lcd.h"
#include <linux/fb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>

static void lcd_clear(void* base_addr, uint16_t xres, uint16_t yres, uint8_t bbp)
{
    uint8_t* base_addr_ptr = (uint8_t*)base_addr;

    if(NULL == base_addr)   return;

    memset(base_addr_ptr, 0xFF, xres * yres * bbp / 8);
}

int lcd_init(Fb_DevType* dev, const char* file_name)
{
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;

    if((NULL == dev) || (NULL == file_name))
        return;

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
    dev->bbp = var.bits_per_pixel;
    dev->len = fix.smem_len;
    dev->phy_addr = fix.smem_start;

    // dev->base_addr = mmap(NULL, fix.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, 0);
    // if(MAP_FAILED == dev->base_addr) {
    //     perror("fb mmap failed");
    //     goto err_close;
    // }

    // lcd_clear(dev->base_addr, dev->xres, dev->yres, dev->bbp);

    return 0;

err_close:
    close(dev->fd);
err_open:
    return -1;
}

void lcd_live_preview(void* fb_base_addr, uint16_t xres,
    void* cam_yuv_buf,
    uint16_t cam_width,
    uint16_t cam_height
)
{
    uint16_t* fb_ptr = (uint16_t*)fb_base_addr;
    uint16_t* line_fb;
    uint8_t* yuv_ptr = (uint8_t*)cam_yuv_buf;
    uint8_t* line_yuv;
    uint16_t color0, color1;
    int y0, Cb, y1, Cr;
    int r0, g0, b0, r1, g1, b1;
    int i, j;

    if((NULL == fb_base_addr) || (NULL == cam_yuv_buf))
        return;

    for(i = 0;i < cam_height;i++) {
        line_yuv = yuv_ptr + 2 * cam_width * i; // yuv下一行的起始地址
        line_fb = fb_ptr + xres * i;            // fb下一行的起始地址
        for(j = 0;j < cam_width;j += 2) {
            y0 = *line_yuv++;
            Cb = *line_yuv++ - 128;
            y1 = *line_yuv++;
            Cr = *line_yuv++ - 128;

            r0 = y0 + ((1436 * Cr) >> 10);
            g0 = y0 - ((352 * Cb + 731 * Cr) >> 10);
            b0 = y0 + ((1815 * Cb) >> 10);

            r1 = y1 + ((1436 * Cr) >> 10);
            g1 = y1 - ((352 * Cb + 731 * Cr) >> 10);
            b1 = y1 + ((1815 * Cb) >> 10);

            if(r0 < 0) r0 = 0;
            if(r0 > 255) r0 = 255;

            if(g0 < 0) g0 = 0;
            if(g0 > 255) g0 = 255;

            if(b0 < 0) b0 = 0;
            if(b0 > 255) b0 = 255;

            if(r1 < 0) r1 = 0;
            if(r1 > 255) r1 = 255;

            if(g1 < 0) g1 = 0;
            if(g1 > 255) g1 = 255;

            if(b1 < 0) b1 = 0;
            if(b1 > 255) b1 = 255;

            color0 = ((uint16_t)(r0 >> 3) << 11)
                    | ((uint16_t)(g0 >> 2) << 5)
                    | ((uint16_t) b0 >> 3);

            color1 = ((uint16_t)(r1 >> 3) << 11)
                    | ((uint16_t)(g1 >> 2) << 5)
                    | ((uint16_t) b1 >> 3);
            line_fb[j] = color0;
            line_fb[j + 1] = color1;
        }
    }
}

void lcd_release(Fb_DevType* dev)
{
    if(NULL == dev) return;

    // munmap(dev->base_addr, dev->len);
    close(dev->fd);
}
