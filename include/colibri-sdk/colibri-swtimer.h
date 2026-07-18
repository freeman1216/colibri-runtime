/* date = July 16th 2026 10:58 pm */

#ifndef COLIBRI_SWTIMER_H
#define COLIBRI_SWTIMER_H

#include "colibri-sdk/colibri.h"

typedef void (*swtimer_callback)(uint16_t expired_count,void *arg);

//Treated opaque by the user code

typedef union
{
    uint32_t gen_index;
    struct 
    {
        uint16_t idx;
        uint16_t generation;
    };
}swtimer_handle;


#define SWTIMER_HANDLE_INVALID (UINT32_MAX)
#define SWTIMER_HANDLE_IS_VALID(handle) ({(handle.gen_index != SWTIMER_HANDLE_INVALID);})

//To quickly hack running from event thread
#define SWTIMER_HACK_EVENT_VAL (0xDEADBABE)
#define SWTIMER_HACK_EVENT ((event_t)SWTIMER_HACK_EVENT_VAL)

swtimer_handle swtimer_create(swtimer_callback cb,swtimer_callback threaded_cb,void *arg);
int swtimer_start(swtimer_handle handle, uint32_t milis,bool one_shot);
int swtimer_get_remaining_ms(swtimer_handle handle);
int swtimer_get_expired_count(swtimer_handle handle);
int swtimer_stop(swtimer_handle handle);
int swtimer_destroy(swtimer_handle handle);

#endif //COLIBRI_SWTIMER_H 
