#include <signal.h>
#include "lcd.h"
#include "camera.h"
#include "pxp.h"

static V4l2_DevType* camera_dev_ptr = NULL;

void sigint_handler(int signo) {
    camera_dev_ptr->is_running = 0;
    printf("exit: signal SIGINT\n");
}

int main(int argc, char** argv)
{
    V4l2_DevType cam;
    Fb_DevType fb;
    const char* video_dev = "/dev/video0";
    const char* fb_dev = "/dev/fb0";

    if(argc >= 2) {
        video_dev = argv[1];
    }

    if(argc >= 3) {
        fb_dev = argv[2];
    }

    signal(SIGINT, sigint_handler);

    memset(&fb, 0, sizeof(fb));
    if(lcd_init(&fb, fb_dev) < 0) {
        perror("lcd failed\n");
        return -1;
    }

    memset(&cam, 0, sizeof(cam));
    camera_dev_ptr = &cam;
    camera_dev_ptr->is_running = 1;
    if(camera_init(&cam, video_dev) < 0) {
        perror("init v4l2 failed\n");
        return -1;
    }

    camera_register_callback(&cam, lcd_live_preview);
    camera_capture(&cam, fb.base_addr, fb.xres);

    lcd_release(&fb);
    camera_release(&cam);

    return 0;
}
