/*
 * repixen 2020-2021
 *
 * KVAT - Key Value Address Table
 * A simple key-value based file system for EEPROM
 *
 */

#ifndef KVAT_H_
#define KVAT_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

// Exceptions
typedef enum KVATException{
    KVATException_none,
    KVATException_unknown,
    KVATException_invalidAccess,
    KVATException_notFound,
    KVATException_insufficientSpace,
    KVATException_storageFault,
    KVATException_heapError
}KVATException;

// Types
typedef uint32_t KVATSize;

// Functions
KVATException KVATInit();
KVATException KVATSaveValue(char* key, void* value, KVATSize valueSize);
KVATException KVATSaveString(char* key, char* value);
KVATException KVATRetrieveValue(char* key, void** valuePointRef, KVATSize* size);
KVATException KVATRetrieveString(char* key, char** valuePointRef);
KVATException KVATDeleteValue(char* key);


#endif /* KVAT_H_ */
