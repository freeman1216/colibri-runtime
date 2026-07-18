cmake_minimum_required(VERSION 3.20.0)

set(ZEPHYR_TOOLCHAIN_VARIANT zephyr CACHE STRING "Zephyr toolchain variant")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(colibri_runtime C CXX ASM) # Enable ASM here, within the project call

target_include_directories(app PRIVATE
        lua/
        include/
)

target_sources(app PRIVATE
        src/main_mock.c
        #src/main.c
        src/events/events_mock.c
        #src/events/events.c
        #src/fs/fs_callback.c
        #src/fs/nvs.c
        #src/leds/neopixels.c
        #src/leds/leds.c
        #src/lua/load-lua.c
        #src/lua/lua.c
        src/lua/lua_mock.c
        #src/management/eeprom.c
        #src/modbus/modbus.c
        #src/slots/host-api.c
        #src/slots/slots.c
        #src/slots/slotcount.c
        #src/slots/i2c.c
        #src/slots/spi.c
        #src/tasks/io-subsystem.c
        #src/tasks/rgb-updater.c
        src/tasks/supervisor.c
        #src/usb/usb.c
        src/swtimer/swtimer.c

        lua/lapi.c
        lua/lbaselib.c
        lua/lctype.c
        lua/ldo.c
        lua/lfunc.c
        lua/linit.c
        lua/lmathlib.c
        lua/lobject.c
        lua/lparser.c
        lua/lstring.c
        lua/ltable.c
        lua/ltm.c
        lua/lvm.c
        lua/lauxlib.c
        lua/lcode.c
        lua/ldebug.c
        lua/ldump.c
        lua/lgc.c
        lua/llex.c
        lua/lmem.c
        lua/lopcodes.c
        lua/lstate.c
        lua/lstrlib.c
        lua/ltablib.c
        lua/lundump.c
        lua/lzio.c
        lua/helper/luaHeap_mock.c
        #lua/helper/luaHeap.c
)

target_compile_definitions(app PRIVATE
        # Platform & Optimization
        ARM
        M4_COMPILER_GCC
)

target_link_libraries(app PUBLIC m)

# TODO: Build of Colibri SDK, for creating I/O module drivers.
#
# For now, that is symlinked into colibri-drivers

