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
    KVATException_invalidAccess,        // Call parameters, or timing is invalid
    KVATException_notFound,             // Requested resource not found
    KVATException_fetchFault,           // Read from multi-page data failed. Specific origin not specified.
    KVATException_insufficientSpace,    // Not enough space in storage
    KVATException_storageFault,         // Related to non-volatile memory.
    KVATException_heapError,            // Related to memory allocation from heap
    KVATException_recordFault,          // Related to empty page record (vector)
    KVATException_tableError            // write/read to entry table failed. Possible origin: logic/hardware. Safety deinit is possible.
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
