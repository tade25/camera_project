#include <signal.h>
#include <unistd.h>
#include "display_service.h"
#include "capture_service.h"
#include "frame_pipeline.h"

static volatile sig_atomic_t g_stop = 0;

static void sigint_handler(int signo)
{
    (void)signo;

    g_stop = 1;
}

int main(int argc, char** argv)
{
    (void)argv;

    Ring_Buffer_t rb;
    display_service_t disp_svc = {0};
    capture_service_t capture_svc = {0};
    Camera_DevType cam = {
        .fd = -1,
    };
    Lcd_DevType lcd = {
        .fd = -1,
    };
    Pxp_DevType pxp = {
        .fd = -1,
    };
    int ret;

    if(argc != 1) {
        fprintf(stderr, "usage: ./capture");
        return -1;
    }

    signal(SIGINT, sigint_handler);

    frame_pipeline_init(&rb);

    disp_svc.lcd_dev = &lcd;
    disp_svc.pxp_dev = &pxp;
    disp_svc.rb = &rb;
    ret = display_service_start(&disp_svc);
    if(ret < 0)
        return -1;

    capture_svc.cam_dev = &cam;
    capture_svc.rb = &rb;
    ret = capture_service_start(&capture_svc);
    if(ret < 0)
        return -1;

    while(!g_stop) {
        pause();
    }

    capture_service_stop(&capture_svc);
    display_service_stop(&disp_svc);
    
    frame_pipeline_destroy(&rb);

    return 0;
}
