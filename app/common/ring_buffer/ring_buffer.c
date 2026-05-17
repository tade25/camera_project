#include "ring_buffer.h"

void ring_buffer_init(Ring_Buffer_t* rb)
{
    memset(rb, 0, sizeof(*rb));
    pthread_mutex_init(&rb->mutex, NULL);
    pthread_cond_init(&rb->not_empty, NULL);
}

void ring_buffer_destroy(Ring_Buffer_t* rb)
{
    pthread_mutex_destroy(&rb->mutex);
    pthread_cond_destroy(&rb->not_empty);
    memset(rb, 0, sizeof(*rb));
}

int ring_buffer_set(Ring_Buffer_t* rb, void* buffer)
{
    pthread_mutex_lock(&rb->mutex);

    if(ring_buffer_is_full(rb))
        return -1;

    rb->buffers[rb->write] = buffer;
    rb->write = (rb->write + 1) % RING_BUFFER_NUMBER;
    pthread_cond_signal(&rb->not_empty);

    pthread_mutex_unlock(&rb->mutex);

    return 0;
}

void* ring_buffer_get(Ring_Buffer_t* rb)
{
    void* buffer = NULL;

    pthread_mutex_lock(&rb->mutex);

    while(ring_buffer_is_empty(rb) && !rb->stop) {
        pthread_cond_wait(&rb->not_empty, &rb->mutex);
    }
    
    buffer = rb->buffers[rb->read];
    rb->buffers[rb->read] = NULL;
    rb->read = (rb->read + 1) % RING_BUFFER_NUMBER;

    pthread_mutex_unlock(&rb->mutex);

    return buffer;
}
