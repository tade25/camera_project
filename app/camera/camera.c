#include "camera.h"
#include <linux/videodev2.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <poll.h>
#include <errno.h>

#define MAX_RETRY_COUNT             3
#define DQBUF_TIMEOUT_MS            2000

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

void camera_register_callback(V4l2_DevType* dev, fmt_cvrt_t cbk)
{
    dev->fmt_cvrt_cbk = cbk;
}

int camera_init(V4l2_DevType* dev, const char* file_name)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_frmsizeenum frmsize;
    struct v4l2_frmivalenum frmival;
    struct v4l2_streamparm streamparm;
    enum v4l2_buf_type buf_type;
    int i;
    int fmtdesc_index = 0;
    int frmsize_index = 0;
    int frmival_index = 0;
    int target_fps;

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

    while(1) {
        memset(&fmtdesc, 0, sizeof(fmtdesc));
        fmtdesc.index = fmtdesc_index;
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(ioctl(dev->fd, VIDIOC_ENUM_FMT, &fmtdesc) < 0)
            break;

        while(1) {
            memset(&frmsize, 0, sizeof(frmsize));
            frmsize.index = frmsize_index;
            frmsize.pixel_format = fmtdesc.pixelformat;
            if(ioctl(dev->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) < 0)
                break;
            while(1) {
                memset(&frmival, 0, sizeof(frmival));
                frmival.index = frmival_index;
                frmival.pixel_format = fmtdesc.pixelformat;
                frmival.width = frmsize.discrete.width;
                frmival.height = frmsize.discrete.height;
                if(ioctl(dev->fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) < 0)
                    break;

                printf("supported format: %s %dx%d@%dfps\n", fmtdesc.description, frmsize.discrete.width, frmsize.discrete.height,\
                        frmival.discrete.denominator / frmival.discrete.numerator);
                frmival_index++;
            }
            frmsize_index++;
            frmival_index = 0;
        }
        fmtdesc_index++;
        frmsize_index = 0;
        frmival_index = 0;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.fmt.pix.width = CAMERA_WIDTH;
    fmt.fmt.pix.height = CAMERA_HEIHET;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(dev->fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("set format failed\n");
        goto err_close;
    }

    if((CAMERA_WIDTH == fmt.fmt.pix.width) &&\
        (CAMERA_HEIHET == fmt.fmt.pix.height)) {
        printf("set format successfully\n");
    }else {
        printf("format adjusted to driver, format: %dx%d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
    }

    dev->width = fmt.fmt.pix.width;
    dev->height = fmt.fmt.pix.height;

    memset(&streamparm, 0, sizeof(streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = 15;
    target_fps = streamparm.parm.capture.timeperframe.denominator / streamparm.parm.capture.timeperframe.numerator;
    if(ioctl(dev->fd, VIDIOC_S_PARM, &streamparm) < 0) {
        perror("set parm failed\n");
        goto err_close;
    }

    if(target_fps == streamparm.parm.capture.timeperframe.denominator / streamparm.parm.capture.timeperframe.numerator) {
        printf("set parm successfully\n");
    }else {
        printf("parm adjusted to driver, fps: %d\n", streamparm.parm.capture.timeperframe.denominator / streamparm.parm.capture.timeperframe.numerator);
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

    // 获取物理地址
    for(i = 0;i < req.count;i++) {
        memset(&buf, 0, sizeof(buf));
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if(ioctl(dev->fd, VIDIOC_QUERYBUF, &buf) < 0) {
            printf("query buffer failed\n");
            goto err_munmap;
        }
        dev->buffers[i].phy_addr = buf.m.offset;
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

void camera_capture(V4l2_DevType* dev, uint32_t fb_buf, void* pxp_dev)
{
    struct pollfd pfd;
    struct timespec t_start;
    struct v4l2_buffer buf;
    unsigned int frame_cnt = 0;
    double fps;
    int ret;
    int retry;

    pfd.fd = dev->fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    clock_gettime(CLOCK_MONOTONIC, &t_start);

    while(dev->is_running) {
        retry = 0;

        while(retry < MAX_RETRY_COUNT) {
            ret = poll(&pfd, 1, DQBUF_TIMEOUT_MS);
            if(ret < 0) {
                if(EINTR == errno)
                    goto exit;
                printf("poll failed\n");
                goto exit;
            }else if(0 == ret) {
                retry++;
                printf("capture timeout retry%d\n", retry);
                continue;
            }

            break;
        }

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if(ioctl(dev->fd, VIDIOC_DQBUF, &buf) < 0) {
            perror("failed to dqbuf");
            break;
        }

        if(dev->fmt_cvrt_cbk)
            dev->fmt_cvrt_cbk(pxp_dev, dev->buffers[buf.index].phy_addr, fb_buf, dev->width, dev->height);

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
exit:
    dev->is_running = 0;
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
