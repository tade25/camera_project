#ifndef __CAMERA_H_
#define __CAMERA_H_

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define REQ_BUF_COUNT           8
#define CAMERA_WIDTH            640
#define CAMERA_HEIHET           480

struct buffer {
    void* start;
    size_t length;
};

typedef void (*frame_callback_t)(void*, uint16_t, void*, uint16_t, uint16_t);

typedef struct {
    struct buffer buffers[REQ_BUF_COUNT];
    frame_callback_t lcd_cbk;
    int fd;
    int buf_cnt;
    volatile int is_running;
}V4l2_DevType;

extern void camera_register_callback(V4l2_DevType* dev, frame_callback_t cbk);
extern int camera_init(V4l2_DevType* dev, const char* file_name);
extern void camera_capture(V4l2_DevType* dev, void* fb_base_addr, uint16_t lcd_xres);
extern void camera_release(V4l2_DevType* dev);

#endif
