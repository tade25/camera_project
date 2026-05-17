#ifndef __DISPLAY_SERVICE_H_
#define __DISPLAY_SERVICE_H_

#include <pthread.h>
#include "lcd.h"
#include "pxp.h"
#include "frame_pipeline.h"

typedef struct {
    Lcd_DevType* lcd_dev;
    Pxp_DevType* pxp_dev;
    pthread_t tid;
    uint8_t stop;
    Ring_Buffer_t* rb;
}display_service_t;

extern int display_service_start(display_service_t* svc);
extern void display_service_stop(display_service_t* svc);

#endif
