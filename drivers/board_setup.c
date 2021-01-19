/*
 * board_setup.c
 * KVAT - Key Value Address Table
 *
 * Setup for onboard features needed in KVAT playground.
 * Inspired by pinout.c included in TivaWare drivers.
 *
 * Author: repixen
 * Copyright (c) 2020-2021, repixen. All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_gpio.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "drivers/board_setup.h"

#include "driverlib/rom.h"      // To use TivaWare contained in ROM
#include "driverlib/rom_map.h"  //



/**
 * Configures the device pins for selected usage on the EK-TM4C129EXL.
 *
 * @param      kusIntHandlerey    Pointer to interrupt handler for User Switches
 *
 */
void boardSetup(void (*usIntHandler)(void)){

    // Enable select GPIO peripherals.
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOJ);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);

    // PA0-1 are used for UART0.
    MAP_GPIOPinConfigure(GPIO_PA0_U0RX);
    MAP_GPIOPinConfigure(GPIO_PA1_U0TX);
    MAP_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // USER SWITCHES =========

    // Register J interrupt
    MAP_GPIOIntRegister(GPIO_PORTJ_BASE, usIntHandler);

    // This is basically GPIOPinTypeGPIOInput, but we can't just call it because it doesn't configure the pull-up J0-1 needs. Need to do it manually.
    MAP_GPIODirModeSet(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_DIR_MODE_IN);
    MAP_GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    // Set Interrupt type and enable
    MAP_GPIOIntTypeSet(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_RISING_EDGE);
    MAP_GPIOIntEnable(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // USER LEDs =============

    // Set PN1 as output for LED
    MAP_GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_1);

}

