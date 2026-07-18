#include <zephyr/kernel.h>

#include "colibri-sdk/colibri.h"
#include "colibri/events.h"
#include "colibri/swtimer.h"

typedef struct
{
    struct k_timer __timer;
    uint16_t expired_count;
    uint16_t generation;
    swtimer_callback cb;
    swtimer_callback threaded_cb;
    void *arg;
    bool one_shot;
} swtimer;

#define TIM_POOL_SIZE (8)

#if TIM_POOL_SIZE > 32
#  error "Fix the bitmask dummy!!!!"
#elif TIM_POOL_SIZE == 32
atomic_t tim_bitmask = ATOMIC_INIT(LONG_MAX);
#else
atomic_t tim_bitmask = ATOMIC_INIT((1 << TIM_POOL_SIZE) - 1);
#endif

swtimer timpool[TIM_POOL_SIZE];

#define SWTIMER_GEN_IS_VALID(handle)({\
( handle.idx < TIM_POOL_SIZE && timpool[handle.idx].generation == handle.generation);\
})

#define SWTIMER_PTR(handle)({\
(&timpool[handle.idx]);\
})

static void timpool_free(swtimer *timer)
{
    uint32_t pos = timer - timpool;
    uint16_t gen = ++timer->generation;
    *timer = (swtimer){0};
    timer->generation = gen;
    atomic_set_bit(&tim_bitmask,pos);
}

static int timpool_alloc(swtimer **writeback)
{
    uint32_t pos = 0;
    do{
        uint32_t bmask = atomic_get(&tim_bitmask);
        
        // for some reason returns 0 for uint32_t (0) input, and BIT + 1 for non (0)
        pos = find_lsb_set(bmask); 
        
        if(!pos){
            return -ENOMEM;
        }
        
        pos--;
        
        //Probably overkill cause its probably never gonna be called concurrently
    }while(!atomic_test_and_clear_bit(&tim_bitmask,pos));
    
    *writeback = &timpool[pos];
    
    return pos;
}

static void swtimer_expiry(struct k_timer *timer)
{
    swtimer *upcast = CONTAINER_OF(timer,swtimer,__timer);
    
    uint16_t gen = upcast->generation;
    
    upcast->expired_count++;
    
    if (upcast->cb)
        upcast->cb(upcast->expired_count,upcast->arg);
    
    //Timer was deleted in the callback
    if (upcast->generation != gen)
        return;
    
    //megahack to just showcase the idea
    //TODO: an actual clean way to interract with the runner thread
    if (upcast->threaded_cb) {
        swtimer_handle handle = {
            .idx = upcast - timpool,
            .generation = upcast->generation,
        };
        int64_t hackval = (int64_t)upcast->expired_count << 32 | handle.gen_index;
        
        events_publish_isr(SWTIMER_HACK_EVENT,hackval);
    } else if (upcast->one_shot) { //Else free in the threaded cb
        timpool_free(upcast);
    }
    
}

swtimer_handle swtimer_create(swtimer_callback cb,swtimer_callback threaded_cb, void *arg)
{
    
    swtimer *allocated = 0;
    int ret = timpool_alloc(&allocated);
    
    if(ret < 0)
        return (swtimer_handle) {.gen_index = SWTIMER_HANDLE_INVALID};
    
    
    swtimer_handle handle = {.idx = ret, .generation = allocated->generation};
    
    allocated->cb = cb;
    allocated->threaded_cb = threaded_cb;
    allocated->arg = arg;
    
    k_timer_init(&allocated->__timer,swtimer_expiry,NULL);
    return handle;
}

//TODO: gate the supported number of milis?
int swtimer_start(swtimer_handle handle,uint32_t milis,bool one_shot)
{
    if (!SWTIMER_HANDLE_IS_VALID(handle) || !SWTIMER_GEN_IS_VALID(handle)) {
        return -EINVAL;
    }
    
    swtimer *timer = SWTIMER_PTR(handle);
    timer->one_shot = one_shot;
    
    k_timeout_t period = one_shot == false ? K_MSEC(milis) : K_NO_WAIT;
    
    k_timer_start(&timer->__timer,K_MSEC(milis),period);
    return 0;
}

int swtimer_get_remaining_ms(swtimer_handle handle)
{
    if (!SWTIMER_HANDLE_IS_VALID(handle) || !SWTIMER_GEN_IS_VALID(handle)) {
        return -EINVAL;
    }
    
    swtimer *timer = SWTIMER_PTR(handle);
    
    return k_timer_remaining_get(&timer->__timer);
    
}

//we can just read k_timer_status_get?
int swtimer_get_expired_count(swtimer_handle handle)
{
    if (!SWTIMER_HANDLE_IS_VALID(handle) || !SWTIMER_GEN_IS_VALID(handle)) {
        return -EINVAL;
    }
    
    swtimer *timer = SWTIMER_PTR(handle);
    
    return timer->expired_count;
}

int swtimer_stop(swtimer_handle handle)
{
    if (!SWTIMER_HANDLE_IS_VALID(handle) || !SWTIMER_GEN_IS_VALID(handle)) {
        return -EINVAL;
    }
    
    swtimer *timer = SWTIMER_PTR(handle);
    
    k_timer_stop(&timer->__timer);
    
    return 0;
}

int swtimer_destroy(swtimer_handle handle)
{
    if (!SWTIMER_HANDLE_IS_VALID(handle) || !SWTIMER_GEN_IS_VALID(handle)) {
        return -EINVAL;
    }
    
    swtimer *timer = SWTIMER_PTR(handle);
    
    k_timer_stop(&timer->__timer);
    
    timpool_free(timer);
    
    return 0;
}

int swtimer_run_callback_threaded(swtimer_handle handle,uint16_t expired_count)
{
    if (!SWTIMER_HANDLE_IS_VALID(handle) || !SWTIMER_GEN_IS_VALID(handle)) {
        //timer was deleted before the thread was able to run the cb
        return -EINVAL;
    }
    
    swtimer *timer = SWTIMER_PTR(handle);
    
    //count can be pretty stale if the event thread underruns, but at that point 
    //we have bigger problems
    
    timer->threaded_cb(expired_count,timer->arg);
    
    //Callback can invalidate the ptr right under us so we need to recheck
    if (SWTIMER_GEN_IS_VALID(handle) && timer->one_shot) 
        timpool_free(timer);
    
    return 0;
}
