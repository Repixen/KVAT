/*
 * board_setup.h
 * KVAT - Key Value Address Table
 *
 * Setup for onboard features needed in KVAT playground.
 * Inspired by pinout.c included in TivaWare drivers.
 *
 * Author: repixen
 * Copyright (c) 2020-2021, repixen. All rights reserved.
 */

#ifndef _DRIVERS_BOARDSETUP_H_
#define _DRIVERS_BOARDSETUP_H_



extern void boardSetup(void (*usIntHandler)(void));


#endif // _DRIVERS_BOARDSETUP_H_
