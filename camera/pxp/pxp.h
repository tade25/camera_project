#ifndef __PXP_H_
#define __PXP_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pxp_user.h"

typedef struct
{
    struct pxp_chan_handle chan_handle;
    struct pxp_mem_desc mem_desc;
    int fd;
    uint32_t input_size;
    uint32_t output_size;
}Pxp_DevType;

extern int pxp_init(Pxp_DevType* pDev, uint16_t cam_w, uint16_t cam_h);
extern void pxp_deinit(Pxp_DevType* pDev);

#endif
