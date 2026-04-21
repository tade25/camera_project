#ifndef __FB_H_
#define __FB_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LCD_WIDTH           1024
#define LCD_HEIGHT          600

typedef struct {
    int fd;
    void* base_addr;
    uint32_t len;
    uint16_t xres;
    uint16_t yres; 
    uint8_t bbp;
}Fb_DevType;

extern int lcd_init(Fb_DevType* dev, const char* file_name);
extern void lcd_live_preview(void* fb_base_addr, uint16_t xres,
    void* cam_yuv_buf,
    uint16_t cam_width,
    uint16_t cam_height
);
extern void lcd_release(Fb_DevType* dev);

#endif
