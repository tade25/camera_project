#include "frame_pipeline.h"

void frame_pipeline_init(Ring_Buffer_t* rb)
{
    ring_buffer_init(rb);
}

void frame_pipeline_destroy(Ring_Buffer_t* rb)
{
    ring_buffer_destroy(rb);
}

int frame_pipeline_set(Ring_Buffer_t* rb, Frame_t* frame)
{
    return ring_buffer_set(rb, (void*)frame);
}

Frame_t* frame_pipeline_get(Ring_Buffer_t* rb)
{
    return (Frame_t*)ring_buffer_get(rb);
}
