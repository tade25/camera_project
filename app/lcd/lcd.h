#ifndef __FB_H_
#define __FB_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LCD_WIDTH           1024
#define LCD_HEIGHT          600

typedef struct {
    int fd;
    uint32_t phy_addr;
    uint16_t xres;
    uint16_t yres; 
}Fb_DevType;

extern int lcd_init(Fb_DevType* dev, const char* file_name);
extern void lcd_release(Fb_DevType* dev);

#endif
