#include <unistd.h>
#include <time.h>
#include "capture_service.h"

static double capture_calc_fps(unsigned int frame_cnt, struct timespec* t_start)
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

void* capture_thread(void* argv)
{
    capture_service_t* svc = (capture_service_t*)argv;
    Camera_FrameType output_frame;
    Frame_t frame;
    struct timespec t_start;
    double fps;
    unsigned int frame_cnt = 0;
    int ret;

    clock_gettime(CLOCK_MONOTONIC, &t_start);

    while(!svc->stop) {
        (void)camera_capture(svc->cam_dev, &output_frame);

        frame.phy_addr = output_frame.phy_addr;
        frame.width = output_frame.width;
        frame.height = output_frame.height;
        ret = frame_pipeline_set(svc->rb, &frame);
        if(ret < 0) {
            printf("[warning] frame buffer full, drop frame\n");
            camera_queue_buffer(svc->cam_dev, output_frame.index);
            continue;
        }

        camera_queue_buffer(svc->cam_dev, output_frame.index);
        frame_cnt++;
        if(0 == (frame_cnt % 60)) {
            fps = capture_calc_fps(frame_cnt, &t_start);
            printf("[debug] average FPS=%.2f\n", fps);
        }
    }

    return NULL;
}

int capture_service_start(capture_service_t* svc)
{
    int ret;

    svc->stop = 0;

    if(camera_init(svc->cam_dev, "/dev/video0") < 0) {
        perror("init v4l2 failed\n");
        return -1;
    }

    ret = pthread_create(&svc->tid, NULL, capture_thread, (void*)svc);
    if(ret != 0) {
        fprintf(stderr, "create capture thread failed: %s\n", strerror(ret));
        camera_release(svc->cam_dev);
        return -1;
    }

    return 0;
}

void capture_service_stop(capture_service_t* svc)
{
    svc->stop = 1;

    pthread_join(svc->tid, NULL);
    camera_release(svc->cam_dev);
}
