/*
 * kvat.h
 * KVAT - Key Value Address Table
 * Dictionary-like file system intended for internal EEPROM
 *
 * Author: repixen
 * Copyright (c) 2020-2021, repixen. All rights reserved.
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
 * Reads value from storage corresponding to specified key.
 * Supports passing reference to a buffer to read data into, as well as allowing for memory to be specifically allocated for.
 * Warning: danger of memory leak on allocate mode. Returned pointer is referencing memory from heap. Free when appropriate.
 *
 * @param      key                  String tag for the value to retrieve
 * @param[out] retrieveBuffer       Reference to buffer to retrieve value into.
 * @param      retrieveBufferSize   Size of retrieve buffer.
 * @param[out] valuePointRef        Optional: Reference to a pointer that will be set to point to retrieved data. Useful on allocate mode.
 *                                  Gets set to NULL if no match found.
 *                                  To use allocate mode, pass NULL on retrieveBuffer and retrieveBufferSize, and a valid argument on valuePointRef.
 * @param[out] size                 Optional: Size of the value fetched in bytes.
 *
 * @return KVATException_ (invalidAccess) (notFound) (tableError) (fetchFault) (none)
 */
KVATException KVATRetrieveValue(char* key, void* retrieveBuffer, KVATSize retrieveBufferSize, void** retrievePointerRef, KVATSize* size);


/**
 * Reads value from storage corresponding to specified key. KVATRetrieveValue convenience.
 * Supports passing reference to a buffer to read data into.
 *
 * @param      key                  String tag for the value to retrieve
 * @param[out] retrieveBuffer       Reference to buffer to retrieve value into.
 * @param      retrieveBufferSize   Size of retrieve buffer.
 * @param[out] size                 Optional: Size of the value fetched in bytes.
 *
 * @return KVATException_ ... See KVATRetrieveValue
 */
KVATException KVATRetrieveValueByBuffer(char* key, void* retrieveBuffer, KVATSize retrieveBufferSize, KVATSize* size);


/**
 * Retrieves string corresponding to a key into a buffer. KVATRetrieveValue convenience.
 *
 * @param      key             String tag for the value to retrieve
 * @param[out] retrieveBuffer  Reference to buffer to retrieve string into.
 *
 * @return KVATException_ ... See KVATRetrieveValue
 */
KVATException KVATRetrieveStringByBuffer(char* key, char* retrieveBuffer, KVATSize retrieveBufferSize);


/**
 * Returns pointer to string (in heap) corresponding to a key. KVATRetrieveValue convenience.
 * Warning: danger of memory leak. Returned pointer is referencing memory from heap. Free when appropriate.
 *
 * @param      key            String tag for the value to retrieve
 * @param[out] valuePointRef  Reference to a pointer that will be set to point to retrieved string.
 *                            Set to NULL if no match found.
 *                            Free memory when appropriate.
 *
 * @return KVATException_ ... See KVATRetrieveValue
 */
KVATException KVATRetrieveStringByAllocation(char* key, char** valuePointerRef);


/**
 * Changes the key that labels a value.
 *
 * @param      currentKey      Current key.
 * @param      newKey          New key to change into.
 *
 * @return KVATException_ (invalidAccess) (notFound) (tableError) (unknown) (insufficientSpace) (none)
 */
KVATException KVATChangeKey(char* currentKey, char* newKey);


/**
 * Deletes a saved value from storage
 *
 * @param      key            String tag for the value to delete
 *
 * @return KVATException_ (invalidAccess) (notFound) (tableError) (none)
 */
KVATException KVATDeleteValue(char* key);


#endif /* KVAT_H_ */
