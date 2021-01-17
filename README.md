# KVAT

Key Value Address Table.

A Dictionary-like file system intended for the internal EEPROM of TM4C series microcontrollers.

## Internals

KVAT runs on top of the TivaWare EEPROM driver. It formats the memory into an index and a series of pages where data can be stored upon the first call to KVATInit().

The public functions available allow for a simple interface to non-volatile storage, while abstracting the details of the EEPROM away.

```c
KVATException result = KVATSaveString("key", "value");
```

## Project

The sources for KVAT are located in the kvat folder. The enclosing CC project is intended as a playground for development and testing. It is based on the Blinky example project for the ek-tm4c129exl board.

Upon running on actual hardware, the project calls KVATInit(), sets up the onboard user switches for interaction, and prepares uartstdio for output through a serial terminal.

## Example

```c
KVATException initExc = KVATInit();

if (initExc == KVATException_none){
     KVATException exc = KVATSaveString("name", "repixen");
}
```

## Development

As the project is in it's early development stage, future commits can change the public interface.