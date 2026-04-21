#include "camera.h"
#include <linux/videodev2.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>

static double camera_calc_fps(unsigned int frame_cnt, struct timespec* t_start)
{
    double elapsed;
    struct timespec t_now;

    if(!t_start || (0 == frame_cnt))
        return 0.0;

    clock_gettime(CLOCK_MONOTONIC, &t_now);

    elapsed = (double)(t_now.tv_sec - t_start->tv_sec) +\
            (double)(t_now.tv_nsec - t_start->tv_nsec) / 1e9;

    return (double)frame_cnt / elapsed;
}

void camera_register_callback(V4l2_DevType* dev, frame_callback_t cbk)
{
    dev->lcd_cbk = cbk;
}

int camera_init(V4l2_DevType* dev, const char* file_name)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    enum v4l2_buf_type buf_type;
    int i;

    dev->fd = open(file_name, O_RDWR);
    if(dev->fd < 0) {
        perror("open /dev/videox failed\n");
        goto err_open;
    }

    memset(&cap, 0, sizeof(cap));
    if(ioctl(dev->fd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("query capability failed\n");
        goto err_close;
    }

    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        printf("not a video capture device\n");
        goto err_close;
    }

    if(!(cap.capabilities & V4L2_CAP_STREAMING)) {
        printf("not support streaming\n");
        goto err_close;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = CAMERA_WIDTH;
    fmt.fmt.pix.height = CAMERA_HEIHET;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(dev->fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("set format failed\n");
        goto err_close;
    }

    printf("camera fmt: %c%c%c%c, w=%u, h=%u, bpl=%u, size=%u\n",
            fmt.fmt.pix.pixelformat & 0xff,
            (fmt.fmt.pix.pixelformat >> 8) & 0xff,
            (fmt.fmt.pix.pixelformat >> 16) & 0xff,
            (fmt.fmt.pix.pixelformat >> 24) & 0xff,
            fmt.fmt.pix.width,
            fmt.fmt.pix.height,
            fmt.fmt.pix.bytesperline,
            fmt.fmt.pix.sizeimage);

    if((CAMERA_WIDTH == fmt.fmt.pix.width) &&\
        (CAMERA_HEIHET == fmt.fmt.pix.height) &&\
        (V4L2_PIX_FMT_YUYV == fmt.fmt.pix.pixelformat)) {
        printf("set the format correctly\n");
    } else {
        printf("driver adjusted the requested format\n");
    }

    memset(&req, 0, sizeof(req));
    req.count = REQ_BUF_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if(ioctl(dev->fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("request buffer failed\n");
        goto err_close;
    }

    dev->buf_cnt = req.count;

    for(i = 0;i < req.count;i++) {
        memset(&buf, 0, sizeof(buf));
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if(ioctl(dev->fd, VIDIOC_QUERYBUF, &buf) < 0) {
            printf("query buffer failed\n");
            goto err_munmap;
        }

        dev->buffers[i].length = buf.length;
        dev->buffers[i].start = mmap(NULL, dev->buffers[i].length,\
                PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, buf.m.offset);
        if (MAP_FAILED == dev->buffers[i].start) {
            perror("mmap failed\n");
            goto err_munmap;
        }

        if(ioctl(dev->fd, VIDIOC_QBUF, &buf) < 0) {
            perror("qbuf failed\n");
            goto err_munmap;
        }
    }

    memset(&buf_type, 0, sizeof(buf_type));
    buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(dev->fd, VIDIOC_STREAMON, &buf_type) < 0) {
        perror("stream on failed\n");
        goto err_munmap;
    }

    printf("start to capture\n");

    return 0;

err_munmap:
    for(i = 0;i < req.count;i++) {
        if (dev->buffers[i].start)
            munmap(dev->buffers[i].start, dev->buffers[i].length);
    }
err_close:
    close(dev->fd);
err_open:
    return -1;
}

void camera_capture(V4l2_DevType* dev, void* fb_base_addr, uint16_t lcd_xres)
{
    struct timespec t_start;
    struct v4l2_buffer buf;
    unsigned int frame_cnt = 0;
    double fps;

    clock_gettime(CLOCK_MONOTONIC, &t_start);
    while(dev->is_running) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if(ioctl(dev->fd, VIDIOC_DQBUF, &buf) < 0) {
            perror("failed to dqbuf");
            break;
        }

        if(dev->lcd_cbk)
            dev->lcd_cbk(fb_base_addr, lcd_xres, dev->buffers[buf.index].start, 640, 480);

        if(ioctl(dev->fd, VIDIOC_QBUF, &buf) < 0) {
            perror("qbuf failed\n");
            break;
        }

        frame_cnt++;
        if(0 == (frame_cnt % 60)) {
            fps = camera_calc_fps(frame_cnt, &t_start);
            printf("[debug] average FPS=%.2f\n", fps);
        }
    }
}

void camera_release(V4l2_DevType* dev)
{
    enum v4l2_buf_type buf_type;
    int i;

    memset(&buf_type, 0, sizeof(buf_type));
    buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(dev->fd, VIDIOC_STREAMOFF, &buf_type) < 0) {
        perror("stream off failed\n");
    }

    printf("end of capture\n");

    for(i = 0;i < dev->buf_cnt;i++) {
        if (dev->buffers[i].start)
            munmap(dev->buffers[i].start, dev->buffers[i].length);
    }

    close(dev->fd);
}
