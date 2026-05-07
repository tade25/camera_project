#ifndef __CAMERA_H_
#define __CAMERA_H_

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define REQ_BUF_COUNT           32
#define CAMERA_WIDTH            1280
#define CAMERA_HEIHET           720

struct buffer {
    void* start;
    size_t length;
    uint32_t phy_addr;
};

typedef int (*fmt_cvrt_t)(void*, uint32_t, uint32_t, int, int);

typedef struct {
    struct buffer buffers[REQ_BUF_COUNT];
    fmt_cvrt_t fmt_cvrt_cbk;
    int fd;
    int buf_cnt;
    volatile int is_running;
    uint16_t width;
    uint16_t height;
}V4l2_DevType;

extern void camera_register_callback(V4l2_DevType* dev, fmt_cvrt_t cbk);
extern int camera_init(V4l2_DevType* dev, const char* file_name);
extern void camera_capture(V4l2_DevType* dev, uint32_t fb_buf, void* pxp_dev);
extern void camera_release(V4l2_DevType* dev);

#endif
