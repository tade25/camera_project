#include "camera.h"
#include <linux/videodev2.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>

#define MAX_RETRY_COUNT             3
#define DQBUF_TIMEOUT_MS            2000

int camera_init(Camera_DevType* dev, const char* file_name)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    struct v4l2_streamparm streamparm;
    enum v4l2_buf_type buf_type;
    uint32_t target_fps;
    uint32_t i;

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
    fmt.fmt.pix.width = CAMERA_WIDTH;
    fmt.fmt.pix.height = CAMERA_HEIGHT;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(dev->fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("set format failed\n");
        goto err_close;
    }

    if((CAMERA_WIDTH == fmt.fmt.pix.width) &&\
        (CAMERA_HEIGHT == fmt.fmt.pix.height)) {
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

    printf("=========== start to capture ===========\n");
    
    memset(&buf_type, 0, sizeof(buf_type));
    buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(dev->fd, VIDIOC_STREAMON, &buf_type) < 0) {
        perror("stream on failed\n");
        goto err_munmap;
    }

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

int camera_capture(Camera_DevType* dev, Camera_FrameType* output_frame)
{
    struct pollfd pfd;
    struct v4l2_buffer buf;
    int ret;
    int retry = 0;

    pfd.fd = dev->fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    do {
        ret = poll(&pfd, 1, DQBUF_TIMEOUT_MS);
        if(ret < 0) {
            if(EINTR == errno)
                goto exit;
            printf("poll failed, errno=%d\n", errno);
            goto exit;
        }else if(0 == ret) {
            retry++;
            printf("capture timeout retry%d\n", retry);
        }
    }while((0 == ret) && (retry < MAX_RETRY_COUNT));

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if(ioctl(dev->fd, VIDIOC_DQBUF, &buf) < 0) {
        perror("failed to dqbuf");
        goto exit;
    }

    output_frame->phy_addr = dev->buffers[buf.index].phy_addr;
    output_frame->width = dev->width;
    output_frame->height = dev->height;
    output_frame->index = buf.index;

    return 0;
exit:
    return -1;
}

void camera_queue_buffer(Camera_DevType* dev, uint32_t index)
{
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.index = index;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if(ioctl(dev->fd, VIDIOC_QBUF, &buf) < 0) {
        perror("qbuf failed\n");
    }
}

void camera_release(Camera_DevType* dev)
{
    enum v4l2_buf_type buf_type;
    int i;

    memset(&buf_type, 0, sizeof(buf_type));
    buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(dev->fd, VIDIOC_STREAMOFF, &buf_type) < 0) {
        perror("stream off failed\n");
    }

    for(i = 0;i < dev->buf_cnt;i++) {
        if (dev->buffers[i].start)
            munmap(dev->buffers[i].start, dev->buffers[i].length);
    }

    close(dev->fd);
}
