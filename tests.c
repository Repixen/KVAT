/*
 * tests.c
 * KVAT - Key Value Address Table
 *
 * Development and testing playground for KVAT.
 * Inspired by blinky.c included in the TivaWare Blinky example project.
 *
 * Author: repixen
 * Copyright (c) 2020-2021, repixen. All rights reserved.
 */

#include <kvat/kvat.h>
#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memmap.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"

#include "drivers/board_setup.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"
#include "kvat/kvat.h"

#include "driverlib/rom.h"      // To use TivaWare contained in ROM
#include "driverlib/rom_map.h"  //


// The error routine that is called if the driver library encounters an error.
#ifdef DEBUG
void __error__(char *pcFilename, uint32_t ui32Line){
    while(1);
}
#endif

char testingMismatch[] = "*****\n     Expectation mismatch >>\n";
/**
 * Provides logging capabilities by interpreting a KVATException.
 *
 * @param      title                String title of the test being performed
 * @param      exception            Exception being interpreted
 * @param      expectingException   Identifies the expected characteristic of the exception passed.
 *
 * @return Boolean with the overall interpretation of the exception.
 */
bool test(char* title, bool expectingException, KVATException exception){
    UARTprintf("\n<test>%s:\n", title);

    if (exception!=KVATException_none){

        if (!expectingException){
            UARTprintf(testingMismatch);
        }

        UARTprintf("     <KVATException> %d\n", exception);
        return false;

    }

    if (expectingException){
        UARTprintf(testingMismatch);
    }

    UARTprintf("     (no exceptions)\n     ");



    return true;
}

/**
 * Performs a series of tests on KVAT to check for correct operation.
 */
void kvatTest(){

    UARTprintf("============\nRunning Tests...\n\n");

    char* ret;

    // Save first string
    test("Save string", false, KVATSaveString("singKey", "First."));

    // Save another string
    test("Save another string", false, KVATSaveString("secondstuff", "This is the second stuff!"));

    // overwrite first string
    test("Overwrite first string with longer one", false, KVATSaveString("singKey", "First. This part is new."));

    // overwrite first string again
    test("Overwrite first string with even longer one", false, KVATSaveString("singKey", "First. This part is new. This is newer."));

    // Retrieve first string
    if (test("Retrieve first string", false, KVATRetrieveStringByAllocation("singKey", &ret))){
        UARTprintf("<v>%s\n", (char*)ret);

        free(ret);
    }

    // Save with route
    test("Save string with route", false, KVATSaveString("second/key/this.h", "Contents of the string saved with route"));

    // Retrieve with route
    if (test("Retrieve string with route", false, KVATRetrieveStringByAllocation("second/key/this.h", &ret))){
        UARTprintf("<v>%s\n", (char*)ret);

        free(ret);
    }

    // Retrieve with wrong route
    if (test("Retrieve string with (wrong) route", true, KVATRetrieveStringByAllocation("second/key/this.c", &ret))){
        UARTprintf("<v>%s\n", (char*)ret);

        free(ret);
    }

    // Retrieve first string again
    if (test("Retrieve first string again", false, KVATRetrieveStringByAllocation("singKey", &ret))){
        UARTprintf("<v>%s\n", (char*)ret);

        free(ret);
    }

    // Rename second string
    test("Rename second string", false, KVATChangeKey("secondstuff", "secondstuffnewname"));

    // Retrieve second string with new name
    if (test("Retrieve second string with new name", false, KVATRetrieveStringByAllocation("secondstuffnewname", &ret))){
        UARTprintf("<v>%s\n", (char*)ret);

        free(ret);
    }

    // Retrieve
    if (test("Retrieve string with route again", false, KVATRetrieveStringByAllocation("second/key/this.h", &ret))){
        UARTprintf("<v>%s\n", (char*)ret);

        free(ret);
    }


    UARTprintf("\nFinished testing\n============\n");


    MAP_GPIOIntClear(GPIO_PORTJ_BASE, GPIO_PIN_0|GPIO_PIN_1);
}

/**
 * Performs pin setup for on-board LEDs and User Switches.
 * Calls initialization for KVAT
 * Provides LED heartbeat
 */
int main(void){

    uint32_t ui32SysClock;
    //
    // Run from the PLL at 120 MHz.
    // Note: SYSCTL_CFG_VCO_240 is a new setting provided in TivaWare 2.2.x and
    // later to better reflect the actual VCO speed due to SYSCTL#22.
    //
    ui32SysClock = MAP_SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                                           SYSCTL_OSC_MAIN |
                                           SYSCTL_USE_PLL |
                                           SYSCTL_CFG_VCO_240), 120000000);

    // Setup board pins
    boardSetup(&kvatTest);

    //
    // Initialize the UART, clear the terminal, and print banner.
    //
    UARTStdioConfig(0, 115200, ui32SysClock);
    UARTprintf("\033[2J\033[H");
    UARTprintf("KVAT 0.3\n");



    // Init KVAT for testing
    KVATException kvatExc = KVATInit();
    UARTprintf(kvatExc==KVATException_none ? "Init: Pass\n" : "Init Error\n");

    // LED heartbeat
    uint8_t pinStatus = GPIO_PIN_1;
    while(1){

        MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, pinStatus);
        pinStatus ^= GPIO_PIN_1;
        MAP_SysCtlDelay(8000000);

    }
}
