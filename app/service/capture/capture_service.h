#ifndef __CAPTURE_SERVICE_H_
#define __CAPTURE_SERVICE_H_

#include <pthread.h>
#include "camera.h"
#include "frame_pipeline.h"

typedef struct {
    Camera_DevType* cam_dev;
    pthread_t tid;
    uint8_t stop;
    Ring_Buffer_t* rb;
}capture_service_t;

extern int capture_service_start(capture_service_t* svc);
extern void capture_service_stop(capture_service_t* svc);

#endif
