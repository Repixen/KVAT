/*
 * repixen 2020-2021
 *
 * KVAT - Key Value Address Table
 * A simple key-value based file system for EEPROM
 *
 */

#ifndef KVAT_H_
#define KVAT_H_

#include <stdbool.h>
#include <stdint.h>

// Exceptions
typedef enum KVATException{
    KVATException_none,
    KVATException_unknown,
    KVATException_invalidAccess,
    KVATException_notFound,
    KVATException_insufficientSpace
}KVATException;

// Types
typedef uint32_t KVATSize;

// Functions
bool KVATInit();
KVATException KVATSaveValue(char* key, void* value, KVATSize valueSize);
void* KVATRetrieveValue(char* key, KVATSize* size);


#endif /* KVAT_H_ */
