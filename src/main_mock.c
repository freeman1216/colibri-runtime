/*
 * Copyright (c) 2024 CurrentMakers
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/fs/fs.h>
#include <string.h>

#include "colibri-sdk/colibri-events.h"
#include "colibri/events.h"
//#include "colibri/fs.h"
//#include "colibri/leds.h"
#include "colibri/luaInterface.h"
//#include "colibri/management.h"
//#include "colibri/modbus.h"
//#include "colibri/slots.h"
#include "colibri/tasks.h"
//#include "colibri/usb.h"

int main()
{
    int error = events_initialize();
    
    return 0;
}
