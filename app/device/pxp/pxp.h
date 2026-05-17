#ifndef __PXP_H_
#define __PXP_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pxp_user.h"

typedef struct
{
    struct pxp_chan_handle chan_handle;
    int fd;
}Pxp_DevType;

extern int pxp_init(Pxp_DevType* pDev, const char* file_name);
extern int pxp_yuyv_to_rgb565(Pxp_DevType* dev,
        uint32_t lcd_phy,
        uint16_t lcd_w,
        uint16_t lcd_h,
        uint32_t cam_phy,
        uint16_t cam_w,
        uint16_t cam_h
);
extern void pxp_release(Pxp_DevType* pDev);

#endif
