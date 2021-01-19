/*
 * board_setup.h
 * KVAT - Key Value Address Table
 *
 * Setup for onboard features needed in KVAT playground.
 * Inspired by pinout.c included in TivaWare drivers.
 *
 * Author: repixen
 * repixen 2021
 */

#ifndef _DRIVERS_BOARDSETUP_H_
#define _DRIVERS_BOARDSETUP_H_



extern void boardSetup(void (*usIntHandler)(void));


#endif // _DRIVERS_BOARDSETUP_H_
