#include <unistd.h>
#include "display_service.h"

void* display_thread(void* argv)
{
    display_service_t* svc = (display_service_t*)argv;
    Frame_t* frame;

    while(!svc->stop) {
        frame = frame_pipeline_get(svc->rb);
        if(NULL == frame) {
            break;
        }

        pxp_yuyv_to_rgb565(svc->pxp_dev,
                svc->lcd_dev->phy_addr,
                svc->lcd_dev->xres,
                svc->lcd_dev->yres,
                frame->phy_addr,
                frame->width,
                frame->height
        );
    }

    return NULL;
}

int display_service_start(display_service_t* svc)
{
    int ret;

    svc->stop = 0;

    if(lcd_init(svc->lcd_dev, "/dev/fb0") < 0) {
        perror("lcd failed\n");
        return -1;
    }

    if(pxp_init(svc->pxp_dev, "/dev/pxp_device") < 0) {
        perror("init pxp failed\n");
        lcd_release(svc->lcd_dev);
        return -1;
    }    

    ret = pthread_create(&svc->tid, NULL, display_thread, (void*)svc);
    if(ret != 0) {
        fprintf(stderr, "create display thread failed: %s\n", strerror(ret));
        lcd_release(svc->lcd_dev);
        pxp_release(svc->pxp_dev);
        return -1;
    }

    return 0;
}

void display_service_stop(display_service_t* svc)
{
    svc->stop = 1;

    pthread_mutex_lock(&svc->rb->mutex);
    svc->rb->stop = 1;
    pthread_cond_broadcast(&svc->rb->not_empty);
    pthread_mutex_unlock(&svc->rb->mutex);

    pthread_join(svc->tid, NULL);

    lcd_release(svc->lcd_dev);
    pxp_release(svc->pxp_dev);
}
