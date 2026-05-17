#ifndef __RING_BUFFER_H_
#define __RING_BUFFER_H_

#include <stdint.h>
#include <string.h>
#include <pthread.h>

#define RING_BUFFER_NUMBER          16

typedef struct {
    void* buffers[RING_BUFFER_NUMBER];
    uint8_t write;
    uint8_t read;
    uint8_t stop;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
}Ring_Buffer_t;

static inline uint8_t ring_buffer_is_full(Ring_Buffer_t* rb)
{
    return (rb->read == ((rb->write + 1) % RING_BUFFER_NUMBER)) ? 1 : 0;
}

static inline uint8_t ring_buffer_is_empty(Ring_Buffer_t* rb)
{
    return (rb->read == rb->write) ? 1 : 0;
}

extern void ring_buffer_init(Ring_Buffer_t* rb);
extern void ring_buffer_destroy(Ring_Buffer_t* rb);
extern int ring_buffer_set(Ring_Buffer_t* rb, void* buffer);
extern void* ring_buffer_get(Ring_Buffer_t* rb);

#endif
