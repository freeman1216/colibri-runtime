#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
//#include <zephyr/fs/fs.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "colibri-sdk/colibri-events.h"
#include "colibri/luaInterface.h"
#include "colibri-sdk/colibri-swtimer.h"

static lua_State* lua_state;

static volatile uint32_t hit;
static char *mock_prog = 
"do\n" 
" local timer = timer_create(timer_callback)\n"
" local query_hits = 0\n"
" local remaining ms = 0\n"
" function timer_callback(expired_count)\n"
"     remaining_ms = timer_get_remaining_ms(timer)\n"
"     query_hits = timer_get_expired_count(timer)\n"
"     mock_hit(remaining_ms, query_hits)\n"
"     if query_hits >= 4 then \n"
"        timer_destroy(timer)\n"
"        query_hits = 0 \n"
"        timer = timer_create(timer_callback)\n"
"        timer_start(timer,223,1)\n"
"     end\n"
"end\n"
""
"timer_start(timer,322,0)\n"
"end\n";


void lua_tim_callback(uint16_t expired_count, void *arg)
{
    UNUSED(arg); //This should be the lua callback fp
    lua_getglobal(lua_state, "timer_callback");
    
    if (!lua_isfunction(lua_state, -1))
    {
        printk("Error: 'timer_callback' is not a defined Lua function\n");
        lua_pop(lua_state, 1);
        return;
    }
    
    lua_pushinteger(lua_state, expired_count);
    
    if (lua_pcall(lua_state, 1, 0, 0) != LUA_OK)
    {
        printk("Lua Error: %s\n", lua_tostring(lua_state, -1));
        lua_pop(lua_state, 1);
        return;
    }
    
    lua_gc(lua_state, LUA_GCCOLLECT, 0);
    
}

int lua_timer_create(lua_State* L)
{
    int n = lua_gettop(L);
    
    //unused for now
    if (n != 1)
        return luaL_error(L, "timer_create(callback): expected 1 argument, got %d", n);
    
    swtimer_handle handle = swtimer_create(NULL,lua_tim_callback,NULL);
    
    if (!SWTIMER_HANDLE_IS_VALID(handle)) 
        return luaL_error(L, "timer_create(callback) failed. Too many timers");
    
    
    lua_pushinteger(L,handle.gen_index);
    
    return 1;
}

int lua_timer_start(lua_State* L)
{
    int n = lua_gettop(L);
    
    if (n != 3)
        return luaL_error(L, "timer_start(handle,milis,one_shot): expected 3 arguments, got %d", n);
    
    swtimer_handle handle = {
        .gen_index = (uint32_t)luaL_checkinteger(L, 1),
    };
    
    uint32_t milis = (uint32_t)luaL_checkinteger(L, 2);
    bool one_shot = !!luaL_checkinteger(L,3);
    
    int ret = swtimer_start(handle,milis,one_shot);
    
    if (ret < 0)
        return luaL_error(L, "timer_start(handle,milis,one_shot) failed. Invalid timer handle");
    return 0;
}

int lua_timer_get_remaining_ms(lua_State* L)
{
    int n = lua_gettop(L);
    
    if (n != 1)
        return luaL_error(L, "timer_get_remaining_ms(handle): expected 1 argument, got %d", n);
    
    swtimer_handle handle = {
        .gen_index = (uint32_t)luaL_checkinteger(L, 1),
    };
    
    int ret = swtimer_get_remaining_ms(handle);
    
    if (ret < 0)
        return luaL_error(L, "timer_get_remaining_ms(handle) failed. Invalid timer handle");
    
    lua_pushinteger(L,ret);
    
    return 1;
}


int lua_timer_get_expired_count(lua_State* L)
{
    int n = lua_gettop(L);
    
    if (n != 1)
        return luaL_error(L, "timer_get_expired_count(handle): expected 1 argument, got %d", n);
    
    swtimer_handle handle = {
        .gen_index = (uint32_t)luaL_checkinteger(L, 1),
    };
    
    int ret = swtimer_get_expired_count(handle);
    
    if (ret < 0)
        return luaL_error(L, "timer_get_expired_count(handle) failed. Invalid timer handle");
    
    lua_pushinteger(L,ret);
    
    return 1;
}

int lua_timer_stop(lua_State* L)
{
    int n = lua_gettop(L);
    
    if (n != 1)
        return luaL_error(L, "timer_stop(handle): expected 1 argument, got %d", n);
    
    swtimer_handle handle = {
        .gen_index = (uint32_t)luaL_checkinteger(L, 1),
    };
    
    int ret = swtimer_stop(handle);
    
    if (ret < 0)
        return luaL_error(L, "timer_stop(handle) failed. Invalid timer handle");
    
    return 0;
}

int lua_timer_destroy(lua_State* L)
{
    int n = lua_gettop(L);
    
    if (n != 1)
        return luaL_error(L, "timer_destroy(handle): expected 1 argument, got %d", n);
    
    swtimer_handle handle = {
        .gen_index = (uint32_t)luaL_checkinteger(L, 1),
    };
    
    int ret = swtimer_destroy(handle);
    
    if (ret < 0)
        return luaL_error(L, "timer_destroy(handle) failed. Invalid timer handle");
    
    return 0;
}

int lua_mock_hit(lua_State *L)
{
    hit++;
    return 0;
}


void lua_event(event_t event, int64_t value)
{
    lua_getglobal(lua_state, "event");
    
    if (!lua_isfunction(lua_state, -1))
    {
        printk("Error: 'event' is not a defined Lua function\n");
        lua_pop(lua_state, 1);
        return;
    }
    
    lua_pushinteger(lua_state, event.type);
    lua_pushinteger(lua_state, event.parameter);
    lua_pushinteger(lua_state, value);
    
    if (lua_pcall(lua_state, 3, 0, 0) != LUA_OK)
    {
        printk("Lua Error: %s\n", lua_tostring(lua_state, -1));
        lua_pop(lua_state, 1);
        return;
    }
    
    lua_gc(lua_state, LUA_GCCOLLECT, 0);
}

int lua_publish(lua_State* L)
{
    int32_t event = (int32_t)luaL_checkinteger(L, 1); // event_type
    int32_t parameter = (int32_t)luaL_checkinteger(L, 2); // event parameter
    int64_t value = luaL_checkinteger(L, 3);
    event_t ev;
    ev.value = create_user_event(event, parameter);
    events_publish(ev, value);
    return 1;
}

int lua_event_get(lua_State* L)
{
    int32_t event = (int32_t)luaL_checknumber(L, 1);
    event_t ev;
    ev.value = event;
    int64_t value = events_get(ev);
    // TODO/NOTE/WARNING: lua_Number is a floating point type, which may not be suitable for representing large integer values. We need to be very aware of this. Update documentation.
    lua_pushnumber(L, (lua_Number)value);
    return 1;
}


int lua_subscribe(lua_State* L)
{
    int n = lua_gettop(L);
    if (n != 2)
        return luaL_error(L, "subscribe(event_type, parameter): expected 2 arguments, got %d", n);
    
    uint16_t event_type = ((uint16_t)luaL_checkinteger(L, 1)) & 0x03FF; // mask out 10 bits
    uint16_t event_parameter = (uint16_t)luaL_checkinteger(L, 2);
    int8_t slot = (int8_t)luaL_optinteger(L, 3, -1);
    event_t event;
    if (slot == -1)
        event = (event_t){create_user_event(event_type, event_parameter)};
    else
        event = (event_t){create_io_event(slot, event_type, event_parameter)};
    int32_t index = (int32_t)events_subscribe(event, lua_event);
    if (index == -1)
        return luaL_error(L, "subscribe(event_type, parameter) failed. Too many subscriptions");
    lua_pushinteger(L, index);
    return 1;
}

int lua_unsubscribe(lua_State* L)
{
    int n = lua_gettop(L);
    if (n != 1)
        return luaL_error(L, "unsubscribe(subscription): expected 1 argument, got %d", n);
    
    int32_t index = (int32_t)luaL_checkinteger(L, 1);
    events_unsubscribe((void*)index);
    return 0;
}

static const luaL_Reg system_api[] = {
    {"publish", lua_publish},
    {"subscribe", lua_subscribe},
    {"unsubscribe", lua_unsubscribe},
    {"event_value", lua_event_get},
    {"timer_create",lua_timer_create},
    {"timer_start",lua_timer_start},
    {"timer_get_remaining_ms",lua_timer_get_remaining_ms},
    {"timer_get_expired_count",lua_timer_get_expired_count},
    {"timer_stop", lua_timer_stop},
    {"timer_destroy", lua_timer_destroy},
    {"mock_hit", lua_mock_hit},
    {NULL, NULL}
};

// This function is called by Lua if it cannot handle an error that occurred.
void luaAbort()
{
    lua_writestringerror("luaAbort", sizeof("luaAbort"));
    printk("luaAbort\n");
}

static void lua_install_uc_globals(lua_State* L)
{
    for (const luaL_Reg* r = system_api; r->name != NULL; r++)
    {
        lua_pushcfunction(L, r->func);
        lua_setglobal(L, r->name);
    }
}

LUAMOD_API int luaopen_uc(lua_State* L)
{
    luaL_newlib(L, system_api);
    return 1;
}

static void lua_register_event_constants(lua_State* L)
{
    lua_newtable(L);
    
    lua_pushstring(L, "UNKNOWN");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_UNKNOWN);
    lua_settable(L, -3);
    
    lua_pushstring(L, "TIME_PERIOD");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_TIME_PERIOD);
    lua_settable(L, -3);
    
    lua_pushstring(L, "TIME");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_TIME);
    lua_settable(L, -3);
    
    lua_pushstring(L, "OUTPUT");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_OUTPUT);
    lua_settable(L, -3);
    
    lua_pushstring(L, "COUNTER");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_COUNTER);
    lua_settable(L, -3);
    
    lua_pushstring(L, "ERROR_CODE");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_ERROR_CODE);
    lua_settable(L, -3);
    
    lua_pushstring(L, "MEASURED_VALUE");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_MEASURED_VALUE);
    lua_settable(L, -3);
    
    lua_pushstring(L, "COMPUTED_VALUE");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_COMPUTED_VALUE);
    lua_settable(L, -3);
    
    lua_pushstring(L, "SETPOINT");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_SETPOINT);
    lua_settable(L, -3);
    
    lua_pushstring(L, "MIN_VALUE");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_MIN_VALUE);
    lua_settable(L, -3);
    
    lua_pushstring(L, "MAX_VALUE");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_MAX_VALUE);
    lua_settable(L, -3);
    
    lua_pushstring(L, "LOW_THRESHOLD");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_LOW_THRESHOLD);
    lua_settable(L, -3);
    
    lua_pushstring(L, "HIGH_THRESHOLD");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_HIGH_THRESHOLD);
    lua_settable(L, -3);
    
    lua_pushstring(L, "RUN_INDICATION");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_RUN_INDICATION);
    lua_settable(L, -3);
    
    lua_pushstring(L, "ALARM_INDICATION");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_ALARM_INDICATION);
    lua_settable(L, -3);
    
    lua_pushstring(L, "RGB_SET");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_RGB_INDICATOR);
    lua_settable(L, -3);
    
    lua_pushstring(L, "MODBUS_UPDATE");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_MODBUS_UPDATE);
    lua_settable(L, -3);
    
    lua_pushstring(L, "MQTT");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_MQTT);
    lua_settable(L, -3);
    
    lua_pushstring(L, "CONFIG");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_CONFIG);
    lua_settable(L, -3);
    
    lua_pushstring(L, "ALL");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_ALL);
    lua_settable(L, -3);
    
    lua_pushstring(L, "NONE");
    lua_pushinteger(L, COLIBRI_EVENT_TYPE_NONE);
    lua_settable(L, -3);
    
    lua_setglobal(L, "events");
}

static int lua_restart()
{
    
    lua_state = luaL_newstate();
    if (lua_state == NULL)
    {
        printk("lua: cannot create state: not enough memory\n");
        return -ENOMEM;
    }
    
    luaL_openlibs(lua_state);
    lua_register_event_constants(lua_state);
    lua_install_uc_globals(lua_state);
    
    if (luaL_dostring(lua_state,mock_prog) != LUA_OK)
    {
        printk("Failed to load script: %s\n", lua_tostring(lua_state, -1));
        lua_close(lua_state);
        return -ESRCH;
    }
    
    return 0;
}

static void lua_script_updated(event_t event, int64_t value)
{
    lua_close(lua_state);
    lua_restart();
}

int lua_initialize()
{
    events_subscribe((event_t){create_user_event(COLIBRI_EVENT_TYPE_LUA_UPDATED, 0)}, lua_script_updated);
    return lua_restart();
}
