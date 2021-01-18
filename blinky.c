/*
 * blinky.c
 * KVAT - Key Value Address Table
 *
 * Development and testing playground for KVAT.
 * Inspired by blinky.c included in the Blinky example project.
 *
 * Author: repixen
 * repixen 2020-2021
 */

#include <kvat/kvat.h>
#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memmap.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"

#include "drivers/pinout.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"
#include "kvat/kvat.h"


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

    // Save
    test("Save String, with line break", KVATSaveString("singKey", "First string saved. \nMake sure it's on multiple pages."), false);

    // Retrieve
    if (test("Retrieve", false, KVATRetrieveString("singKey", &ret))){
        UARTprintf("<v>%s\n", (char*)ret);

        free(ret);
    }

    // Save with another key
    test("Save string with route", KVATSaveString("second/key/this.h", "Contents of the string saved with route"), false);

    // Retrieve
    if (test("Retrieve string with route", false, KVATRetrieveString("second/key/this.h", &ret))){
        UARTprintf("<v>%s\n", (char*)ret);

        free(ret);
    }

    if (test("Retrieve string with (wrong) route", true, KVATRetrieveString("second/key/this.c", &ret))){
        UARTprintf("<v>%s\n", (char*)ret);

        free(ret);
    }

    // Retrieve
    if (test("Retrieve first string", false, KVATRetrieveString("singKey", &ret))){
        UARTprintf("<v>%s\n", (char*)ret);

        free(ret);
    }

    // Delete
    test("Delete first string", false, KVATDeleteValue("singKey"));

    // Retrieve Deleted
    if (test("Retrieve Deleted first string", true, KVATRetrieveString("singKey", &ret))){
        UARTprintf("<v>%s\n", (char*)ret);

        free(ret);
    }

    // Retrieve
    if (test("Retrieve string with route again", false, KVATRetrieveString("second/key/this.h", &ret))){
        UARTprintf("<v>%s\n", (char*)ret);

        free(ret);
    }


    UARTprintf("\nFinished testing\n============\n");


    GPIOIntClear(GPIO_PORTJ_BASE, GPIO_PIN_0|GPIO_PIN_1);
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

    // Set PN1 as output for LED
    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_1);

    // Init KVAT for testing
    KVATException kvatExc = KVATInit();
    UARTprintf(kvatExc==KVATException_none ? "Init: Pass\n" : "Init Error\n");

    // LED heartbeat
    uint8_t pinStatus = GPIO_PIN_1;
    while(1){

        GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, pinStatus);
        pinStatus ^= GPIO_PIN_1;
        SysCtlDelay(8000000);

    }
}
