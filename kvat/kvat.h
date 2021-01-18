/*
 * kvat.h
 * KVAT - Key Value Address Table
 * Dictionary-like file system intended for internal EEPROM
 *
 * Author: repixen
 * repixen 2020-2021
 */
#ifndef KVAT_H_
#define KVAT_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

// Exceptions ---------------------

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
    KVATException_tableError            // write/read to entry table failed. Possible origin: logic/hardware. Safety deinit possible.
}KVATException;

// Types --------------------------

typedef uint32_t KVATSize;

// Prototypes ----------------------

/**
 * Initializes kvat for operation. Formats EEPROM if necessary (Format ID mismatch).
 *
 * @return KVATException_ (invalidAccess) (storageFault) (heapError) (recordFault) (none) ...
 */
KVATException KVATInit();

/**
 * Saves data tagged with a key
 *
 * @param      key            String tag for the value to save
 * @param      value          Reference to value to save in storage
 * @param      valueSize      Length of the value to save
 *
 * @return KVATException_ (invalidAccess) (insufficientSpace) (tableError) (none)
 */
KVATException KVATSaveValue(char* key, void* value, KVATSize valueSize);

/**
 * Saves a string of data tagged with a key. KVATSaveValue convenience.
 *
 * @param      key            String tag for the value to save
 * @param      value          Reference to string to save
 *
 * @return KVATException_ ... See KVATSaveValue
 */
KVATException KVATSaveString(char* key, char* value);

/**
 * Returns pointer to value corresponding to specified key.
 * Warning: possible memory leak. Returned pointer is referencing memory from heap. Free when appropriate.
 *
 * @param      key            String tag for the value to retrieve
 * @param[out] valuePointRef  Reference to a pointer that will be set to point to retrieved value.
 *                            Set to NULL if no match found.
 * @param[out] size           Optional: Size of the value returned in bytes.
 *
 * @return KVATException_ (invalidAccess) (notFound) (tableError) (fetchFault) (none)
 */
KVATException KVATRetrieveValue(char* key, void** valuePointRef, KVATSize* size);

/**
 * Returns pointer to string corresponding to a key. KVATRetrieveValue convenience.
 *
 * @param      key            String tag for the value to retrieve
 * @param[out] valuePointRef  Reference to a pointer that will be set to point to retrieved string.
 *                              Set to NULL if no match found.
 *
 * @return KVATException_ ... See KVATRetrieveValue
 */
KVATException KVATRetrieveString(char* key, char** valuePointRef);

/**
 * Deletes a saved value from storage
 *
 * @param      key            String tag for the value to delete
 *
 * @return KVATException_ (invalidAccess) (notFound) (tableError) (none)
 */
KVATException KVATDeleteValue(char* key);


#endif /* KVAT_H_ */
