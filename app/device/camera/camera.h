#ifndef __CAMERA_H_
#define __CAMERA_H_

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define REQ_BUF_COUNT           16
#define CAMERA_WIDTH            1280
#define CAMERA_HEIHET           720

struct buffer {
    void* start;
    size_t length;
    uint32_t phy_addr;
};

typedef struct {
    struct buffer buffers[REQ_BUF_COUNT];
    int fd;
    int buf_cnt;
    uint16_t width;
    uint16_t height;
}Camera_DevType;

typedef struct {
    uint32_t phy_addr;
    uint16_t width;
    uint16_t height;
    uint32_t index;
}Camera_FrameType;


extern int camera_init(Camera_DevType* dev, const char* file_name);
extern int camera_capture(Camera_DevType* dev, Camera_FrameType* output_frame);
extern void camera_queue_buffer(Camera_DevType* dev, uint32_t index);
extern void camera_release(Camera_DevType* dev);

#endif
