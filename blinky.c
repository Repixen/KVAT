//*****************************************************************************
//
// blinky.c - Simple example to blink the on-board LED.
//
// Copyright (c) 2013-2020 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.2.0.295 of the EK-TM4C129EXL Firmware Package.
//
//*****************************************************************************

#include <kvat/kvat.h>
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"

#include "drivers/pinout.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"
#include "kvat/kvat.h"


//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
    while(1);
}
#endif


void kvatTest(){

    KVATException xcpt = KVATSaveString("singKey", "12345678901234567890123456789012345678901234567890123456789A");


    KVATSize readSize;

    char* greg;

    xcpt = KVATRetrieveString("singKey", &greg);

    if (!xcpt){
        UARTprintf("<v>%s\n", (char*)greg);
    }

    KVATDeleteValue("singKey");

    xcpt = KVATRetrieveString("singKey", &greg);

    if (!xcpt){
        UARTprintf("<v>%s\n", (char*)greg);
    }

    free(greg);


    GPIOIntClear(GPIO_PORTJ_BASE, GPIO_PIN_0|GPIO_PIN_1);
}

//*****************************************************************************
//
// Blink the on-board LED.
//
//*****************************************************************************
int main(void){

    uint32_t ui32SysClock;
    //
    // Run from the PLL at 120 MHz.
    // Note: SYSCTL_CFG_VCO_240 is a new setting provided in TivaWare 2.2.x and
    // later to better reflect the actual VCO speed due to SYSCTL#22.
    //
    ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                                           SYSCTL_OSC_MAIN |
                                           SYSCTL_USE_PLL |
                                           SYSCTL_CFG_VCO_240), 120000000);

    PinoutSet(false, false);

    //
    // Initialize the UART, clear the terminal, and print banner.
    //
    UARTStdioConfig(0, 115200, ui32SysClock);
    UARTprintf("\033[2J\033[H");
    UARTprintf("KVAT 0.1\n");


    // Register J0 interrupt
    GPIOIntRegister(GPIO_PORTJ_BASE, &kvatTest);

    // This is basically GPIOPinTypeGPIOInput, but we can't just call it because it doesn't configure the pull-up J0 needs. Need to do it manually.
    GPIODirModeSet(GPIO_PORTJ_BASE, GPIO_PIN_0, GPIO_DIR_MODE_IN);
    GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    // Set Interrupt type and enable
    GPIOIntTypeSet(GPIO_PORTJ_BASE, GPIO_PIN_0, GPIO_RISING_EDGE);
    GPIOIntEnable(GPIO_PORTJ_BASE, GPIO_PIN_0);

    //
    // Enable the GPIO pin for the LED (PN0).  Set the direction as output, and
    // enable the GPIO pin for digital function.
    //
    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_0);

    KVATException kvatExc = KVATInit();
    UARTprintf(kvatExc==KVATException_none ? "Init: Pass\n" : "Init Error\n");


    volatile uint32_t ui32Loop;
    //
    // Loop forever.
    //
    while(1)
    {
        //
        // Turn on the LED.
        //
        GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, GPIO_PIN_0);
        for(ui32Loop = 0; ui32Loop < 2000000; ui32Loop++);

        //
        // Turn off the LED.
        //
        GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, 0x0);
        for(ui32Loop = 0; ui32Loop < 2000000; ui32Loop++);
    }
}
