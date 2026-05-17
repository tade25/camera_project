#ifndef __FRAME_PIPELINE_H_
#define __FRAME_PIPELINE_H_

#include <stdint.h>
#include "ring_buffer.h"

typedef struct {
    uint32_t phy_addr;
    uint16_t width;
    uint16_t height;
}Frame_t;

extern void frame_pipeline_init(Ring_Buffer_t* rb);
extern void frame_pipeline_destroy(Ring_Buffer_t* rb);
extern int frame_pipeline_set(Ring_Buffer_t* rb, Frame_t* frame);
extern Frame_t* frame_pipeline_get(Ring_Buffer_t* rb);

#endif
