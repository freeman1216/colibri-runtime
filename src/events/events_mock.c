#include "zephyr/kernel.h"
#include "colibri-sdk/colibri-events.h"
#include <zephyr/linker/section_tags.h>
#include "colibri/swtimer.h"
#include "colibri/luaInterface.h"

#define EVENT_STACK_SIZE   (32 * 1024)   /* 32 KiB, placed in CCM */
#define EVENT_THREAD_PRIORITY     10

#define EVENTS_MAX_SUBSCRIPTIONS 256
#define EVENTS_QUEUE_SIZE 256

// This is a private struct type for this C file only. Not part of any public API.
typedef struct
{
    event_t event;
    int64_t value;
} event_value_t;

static event_subscription subscriptions[EVENTS_MAX_SUBSCRIPTIONS];

static const uint32_t ALL_EVENTS = create_user_event(COLIBRI_EVENT_TYPE_ALL, 0);

K_MSGQ_DEFINE(event_msgq, sizeof(event_value_t), EVENTS_QUEUE_SIZE, 4);

int64_t events_get(event_t event)
{
    for (int i = 0; i < EVENTS_MAX_SUBSCRIPTIONS; i++)
    {
        event_subscription subscription = subscriptions[i];
        if (subscription.event_type.type == event.type)
        {
            return subscription.latest_value;
        }
    }
    return 0;
}

void events_publish(event_t event, int64_t value)
{
    event_value_t ev;
    ev.event = event;
    ev.value = value;
    k_timeout_t wait = event.io ? K_MSEC(50) : K_NO_WAIT;
    int result = k_msgq_put(&event_msgq, &ev, wait);
    if (result != 0)
    {
        printk("Event dropped: [%d:%d} %d (%d) -> %lld. Queue size exceeded.\n", event.slot, event.io, event.type,
               event.parameter, value);
    }
    else
    {
        // printk("Event published: [%d:%d] %d(%d) -> %lld.\n", event.slot, event.io, event.type, event.parameter, value);
    }
}

void* events_subscribe(event_t event_type, event_callback callback)
{
    printk("Subscribe: %d\n", event_type.value);
    for (int i = 0; i < EVENTS_MAX_SUBSCRIPTIONS; i++)
    {
        if (subscriptions[i].event_type.type == 0) // Free?
        {
            subscriptions[i].event_type = event_type;
            subscriptions[i].callback = callback;
            return (void*)i; // KISS! Was messing with actual pointers before, but just too dangerous.
        }
    }
    printk("Failed to add subscription, no free slots available\n");
    return (void*)-1;
}

void events_unsubscribe(void* subscription)
{
    size_t index = (size_t)subscription;
    if (index < 0 || index > EVENTS_MAX_SUBSCRIPTIONS)
        return;
    event_subscription* sub = &subscriptions[index];
    sub->callback = NULL;
    sub->event_type.type = COLIBRI_EVENT_TYPE_NONE;
}

static void post(event_value_t* data)
{
    if ( data->event.type < 0 || data->event.type >= COLIBRI_EVENT_TYPE_END_OF_LIST)
        return;
    for (int i = 0; i < EVENTS_MAX_SUBSCRIPTIONS; i++)
    {
        event_subscription subscription = subscriptions[i];
        if (subscription.event_type.value == data->event.value || subscription.event_type.type == ALL_EVENTS)
        {
            if (subscription.callback)
                subscription.callback(data->event, data->value);
            // Setting this AFTER callback, allows us to pick "previous" from within the callback, with event_broker_get().
            subscription.latest_value = data->value;
        }
    }
}

/*
 * Event thread stack in CCM SRAM (0x10000000 on STM32F405).
 *
 *  - __dtcm_noinit_section: skip boot-time zeroing; Zephyr initializes
 *    the active part of the stack itself when the thread starts.
 *  - The macro yields an array that satisfies Zephyr's stack-alignment
 *    requirements for the architecture.
 *
 * Note: K_THREAD_STACK_DEFINE does not accept a section attribute, so we
 * declare the array manually using the same underlying primitives.
 */

static K_KERNEL_STACK_MEMBER(event_thread_stack, EVENT_STACK_SIZE);

static struct k_thread event_thread_data;
static k_tid_t event_thread_tid;


/* ------------------------------------------------------------------------- */
/* Thread entry point: initialize the VM, load the script, then tick forever.*/
/* ------------------------------------------------------------------------- */
static void event_thread_entry(void* p1, void* p2, void* p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    event_value_t data;
    
    lua_initialize();
    
    while (true)
    {
        k_msgq_get(&event_msgq, &data, K_FOREVER);
        
        //TODO: do this in a sane way!!!!!!!!!!
        if (data.event.value == SWTIMER_HACK_EVENT_VAL) {
            swtimer_handle handle = {.gen_index = (uint32_t)(data.value & UINT_MAX)};
            uint16_t expired_count = data.value >> 32;
            swtimer_run_callback_threaded(handle,expired_count);
        }
        else {
            post(&data);
        }
    }
}

/* ISR-safe publish: non-blocking, no logging. Returns 0, or -ENOMSG if full. */
int events_publish_isr(event_t event, int64_t value)
{
    event_value_t ev = {.event = event, .value = value};
    return k_msgq_put(&event_msgq, &ev, K_NO_WAIT);
}

static int create_event_thread(void)
{
    event_thread_tid = k_thread_create(
                                       &event_thread_data,
                                       event_thread_stack,
                                       K_KERNEL_STACK_SIZEOF(event_thread_stack),
                                       event_thread_entry,
                                       NULL, NULL, NULL,
                                       EVENT_THREAD_PRIORITY,
                                       0,
                                       K_NO_WAIT);
    
    if (event_thread_tid == NULL)
    {
        printk("events: failed to create thread\n");
        return -ENOMEM;
    }
    
    k_thread_name_set(event_thread_tid, "events");
    return 0;
}

int events_initialize()
{
    // clear event subscriptions table
    for (int i = 0; i < EVENTS_MAX_SUBSCRIPTIONS; i++)
    {
        subscriptions[i].event_type.value = 0;
        subscriptions[i].callback = NULL;
        subscriptions[i].latest_value = 0;
    }
    return create_event_thread();
}
