/*
 * kvat.c
 * KVAT 0.3 - Key Value Address Table
 * Dictionary-like file system intended for internal EEPROM
 *
 * Author: repixen
 * Copyright (c) 2020-2021, repixen. All rights reserved.
 */

#include "kvat/kvat.h"

#include <string.h>
#include <driverlib/sysctl.h>
#include <driverlib/eeprom.h>

#include "driverlib/rom.h"      // To use TivaWare contained in ROM
#include "driverlib/rom_map.h"  //

//==========================================================
// FORMATTING LIMITS

#define FORMATID 210    // Persistence marker for formatting. Mismatch from storage will invalidate it.
#define PAGESIZE 12     // Size of a single page in bytes. Pages need to be a multiple of 4 bytes in size (256 max on single-byte-remains scheme)
#define PAGECOUNT 128   // 255 max on a single-byte-paging scheme

// NOTE: Current implementation scheme is single-byte-paging and single-byte-remains (usable storage on max: 65KB)

//==========================================================
// GENERAL LIMITS

#define INDEXSTART 0    // Address that the index starts on in storage

//==========================================================
// RECOMMENDED LIMITS

#define STRINGKEYSTDLEN 16  // Expected maximum length for string-keys (baseline, not enforced)

//==========================================================
/* TABLE ENTRY METADATA FORMATTING
 *
 * x x KF KF  VT KT ST ST > lsb
 *
 */

// OVERALL
#define MDEFAULT        0x00    // Default value for metadata

// STATUS
#define MACTIVE         0x01    // (bool) Active entry (currently pointing to valid chains)
#define MOPEN           0x02    // (bool) Entry is currently being edited

// KEY TYPE
#define MKC_ISMULTIPLE  0x04    // Mask
#define MKC_MULTIPLE    0x04    // Key is stored in multiple pages
#define MKC_SINGLE      0x00    // Key is stored in single page

// VALUE
#define MVC_ISMULTIPLE  0x08    // Mask
#define MVC_MULTIPLE    0x08    // Value is stored in multiple pages
#define MVC_SINGLE      0x00    // Value is stored in single page

// KEY FORMAT
#define MKEYFORMAT      0x30    // Mask
#define MKF_STRING      0x00    // String
//#define MKF_UINT32      0x10    // Unsigned int
//#define MKF_            0x20    // (undefined)
//#define MKF_            0x30    // (undefined)


//==========================================================
// Internal types

typedef unsigned char MetaData;
typedef unsigned char PageNumber;
typedef uint32_t StorageAddress;
typedef uint32_t PageData;
typedef uint32_t* PageDataRef;

//==========================================================
// Index and table
// Structs need to be a multiple of 4 bytes in size!!

// Entry in index table
// Multiple of 4 by design
typedef struct KVATKeyValueEntry{
    MetaData metadata;
    PageNumber keyPage;
    PageNumber valuePage;
    unsigned char remains; // Number of bytes that the value should be truncated from max page-chain (data) size
}KVATKeyValueEntry;

// Index (header portion)
// Multiple of 4 by alignment
// The table is part of the index, but never loaded or saved from storage entirely. Entries from table should be handled individually.
typedef struct KVATIndex{
    uint16_t formatID;
    KVATSize pageSize;
    PageNumber pageCount;
    StorageAddress pageBeginAddress;   // Since this is 4 byte and 4-byte-aligned, the table (next) will be as well
    //KVATKeyValueEntry table[PAGECOUNT];
}KVATIndex;

//==========================================================

static KVATIndex* index = NULL;             // Runtime instance of KVATIndex, loaded into memory by readIndex()
static void deinit();                       // Major fail safe. Call upon an unrecoverable exception to void runtime.
static bool didInit = false;
static unsigned char* pageRecord = NULL;

//==========================================================

/**
 * Writes index into storage
 *
 * @return KVATException_ (storageFault) (heapError) (none)
 */
static KVATException saveIndex(){

    // Produce a copy of the index to store
    uint32_t* indexCopy = malloc(sizeof(KVATIndex));
    if (indexCopy==NULL){return KVATException_heapError;}

    memcpy(indexCopy, index, sizeof(KVATIndex));

    uint32_t programResult = MAP_EEPROMProgram(indexCopy, INDEXSTART, sizeof(KVATIndex));

    // Free space from the copy
    free(indexCopy);

    if (programResult!=0){  // Something came up with EEPROMProgram
        return KVATException_storageFault;
    }

    return KVATException_none;
}

/**
 * Reads stored index from storage into 'index'
 *
 * @return KVATException_ (invalidAccess) (heapError) (none)
 */
static KVATException readIndex(){
    if (index==NULL){return KVATException_invalidAccess;}

    // Read into compatible uint32_t buffer
    uint32_t* indexBuff = malloc(sizeof(KVATIndex));
    if (indexBuff==NULL){return KVATException_heapError;}
    MAP_EEPROMRead(indexBuff, INDEXSTART, sizeof(KVATIndex));

    // Copy into actual index
    memcpy(index, indexBuff, sizeof(KVATIndex));

    // Get rid of buffer
    free(indexBuff);

    return KVATException_none;
}

/**
 * Eases the task of setting or clearing portions of metadata.
 *
 * @param      entry         Reference to a KVATKeyValueEntry for modification of metadata
 * @param      mask          Mask for the bits to modify
 * @param      value         Value to set on modifying bits only
 *
 */
static void setEntryMetadata(KVATKeyValueEntry* entry, MetaData mask, MetaData value){
    entry->metadata &= ~mask;         // Clear position
    entry->metadata |= value & mask;  // Set value
}

/**
 * Converts a masked portion of an entrie's metadata into a bool
 *
 * @param      entry         Reference to a KVATKeyValueEntry
 * @param      mask          Mask for the bits of the metadata to convert to boolean
 *
 * @return bool
 */
static bool getMetadataBool(KVATKeyValueEntry* entry, MetaData mask){
    return (entry->metadata & mask) ? true : false;
}

/**
 * Returns the address in storage of a table entry
 *
 * @param      entryPosition          Position of the entry in the table
 *
 * @return Address of the table entry in storage.
 */
static StorageAddress getEntryAddressFromPosition(PageNumber entryPosition){
    return INDEXSTART + sizeof(KVATIndex) + sizeof(KVATKeyValueEntry)*entryPosition;
}

/**
 * Writes only the section of the index pertaining to a single table entry into storage.
 *
 * @param      entryToSave          Reference to a KVATKeyValueEntry instance to save
 * @param      entryPosition        The position to save in the entry table.
 *
 * @return Success of the save process. True if successful.
 */
static bool saveTableEntry(KVATKeyValueEntry* entryToSave, PageNumber entryPosition){

    // Copy table entry into compatible uint32_t pointer
    uint32_t* entryCopy = malloc(sizeof(KVATKeyValueEntry));
    if (entryCopy==NULL){return false;}

    memcpy(entryCopy, entryToSave, sizeof(KVATKeyValueEntry));

    // Get address of the table entry position to save in
    StorageAddress entryAddress = getEntryAddressFromPosition(entryPosition);

    // Program the entry into storage
    uint32_t programResult = MAP_EEPROMProgram(entryCopy, entryAddress, sizeof(KVATKeyValueEntry));

    // Get rid of the memory for copy
    free(entryCopy);

    return !programResult;
}

/**
 * Reads only the section of the index pertaining to a single table entry into memory.
 *
 * @param      entryRead          Reference to a KVATKeyValueEntry instance to read into
 * @param      entryPosition      The position of the entry in table to read.
 *
 * @return Success of the read process. True if successful.
 */
static bool readTableEntry(KVATKeyValueEntry* entryRead, PageNumber entryPosition){
    // Prepare buffer to read into
    uint32_t* entryBuff = malloc(sizeof(KVATKeyValueEntry));
    if (entryBuff==NULL){return false;}

    // Get address of the table entry to read
    StorageAddress entryAddress = getEntryAddressFromPosition(entryPosition);

    // Read entry from storage
    MAP_EEPROMRead(entryBuff, entryAddress, sizeof(KVATKeyValueEntry));

    // Copy read data into the right place
    memcpy(entryRead, entryBuff, sizeof(KVATKeyValueEntry));

    // Get rid of the read buffer
    free(entryBuff);

    return true;
}

/**
 * Returns the number in the index table of an empty entry spot.
 * Note: 0 is reserved for invalid page.
 *
 * @return Number of the empty entry, or 0 if all full (or fault).
 */
static PageNumber getEmptyTableEntryNumber(){
    // Get some local references of page count (also number of table entries)
    PageNumber entryCount = index->pageCount;

    KVATKeyValueEntry entry;
    bool didReadEntry;

    // Start checkin'
    for (PageNumber entryN = 1; entryN<entryCount; entryN++){
        didReadEntry = readTableEntry(&entry, entryN);
        if (!didReadEntry){break;}   // Sanity check: if at any point table read fails, abort.

        if (!(entry.metadata & (MACTIVE | MOPEN))){ // Check status to see if actually empty
            return entryN;
        }
    }
    return 0;
}

/**
 * Calculates and returns the address of page 0 based on the definitions for INDEXSTART, PAGECOUNT & KVAT structs.
 * Warning: this is intended for formatting calculation. If already formatted, get from format settings (in index).
 *
 * @return Address of page 0 in storage.
 */
static StorageAddress getNaturalAddressOfPage0(){
    return INDEXSTART + sizeof(KVATIndex) + sizeof(KVATKeyValueEntry)*PAGECOUNT;
}

/**
 * Formats storage based on defined formatting limits.
 * (Writes empty index)
 * Should only be called by an init or a reformat operation.
 *
 * @return KVATException_ (none) (invalidAccess) (tableError) ...
 */
static KVATException formatMemory(){
    //GUARD - no formatting allowed if initialized
    if (didInit){return KVATException_invalidAccess;}

    //Prepare index with formatting limits and paging region
    index->formatID = FORMATID;
    index->pageSize = PAGESIZE;
    index->pageCount = PAGECOUNT;
    index->pageBeginAddress = getNaturalAddressOfPage0();

    KVATKeyValueEntry emptyEntry = {.metadata = MDEFAULT};

    bool didSaveEntry;

    //Save entries as new (empty) (including invalid page 0)
    for (PageNumber entryN = 0; entryN<PAGECOUNT; entryN++){
        didSaveEntry = saveTableEntry(&emptyEntry, entryN);
        if (!didSaveEntry){return KVATException_tableError;}
    }

    return saveIndex();
}

/**
 * Converts the number of a page into the address it should hold in storage.
 *
 * @param      pageNumber        Number of a page to convert to address.
 *
 * @return Address of the page in storage based on page region address and page size.
 */
static StorageAddress getPageAddress(PageNumber pageNumber){
    if (pageNumber==0){return 0;}

    // Convert page number into relative address
    StorageAddress pageAddress = pageNumber*index->pageSize;

    // Offset into absolute address
    pageAddress += index->pageBeginAddress;

    return pageAddress;
}

/**
 * Reads page from storage into a buffer.
 *
 * @param[out] pageData        Reference to a buffer that will be used to dump the page data
 * @param      pageNumber      Number of the page to read.
 * @param      limitReadSize   Optional: Number of bytes to limit the reading to. Pass 0 to read entire length of page.
 */
static void readPage(PageDataRef pageData, PageNumber pageNumber, uint32_t limitReadSize){
    StorageAddress pageAddress = getPageAddress(pageNumber);

    // Read from address
    MAP_EEPROMRead(pageData, pageAddress, limitReadSize ? limitReadSize : index->pageSize);
}

/**
 * Writes page to storage from a buffer
 *
 * @param      pageData        Reference to a buffer containing the data of the page to write
 * @param      pageNumber      Number of the page to write to.
 * @param      limitWriteSize  Optional: Number of bytes to limit the writing to. Pass 0 to write entire length of page.
 *
 * @return Boolean with success of operation.
 */
static bool writePage(PageDataRef pageData, PageNumber pageNumber, uint32_t limitWriteSize){
    StorageAddress pageAddress = getPageAddress(pageNumber);

    // Write to address
    uint32_t programResult = MAP_EEPROMProgram(pageData, pageAddress, limitWriteSize ? limitWriteSize : index->pageSize);

    return !programResult;
}

/**
 * Obtains the next page from the one passed, and returns it.
 * Warning: Does not validate result. Pages do not contain metadata to validate on their own.
 *
 * @param      pageData        Reference to the data of a page.
 *
 * @return Number of the page that is next.
 */
static PageNumber getNextPageNumberFromPage(PageDataRef pageData){
    // Copy the page number block to an actual PageNumber using memcpy
    // Could just alias pageData, but it's illegal and can cause problems if using compiler optimizations.
    PageNumber nextPage;
    memcpy(&nextPage, pageData, sizeof(PageNumber));
    return nextPage;
}

/**
 * Reads page from storage, obtains the number of the next page, and returns it.
 * Warning: Does not validate result. Pages do not contain metadata to validate on their own.
 *
 * @param      pageNumber        Page to get the next of.
 *
 * @return Number of the page that is next.
 */
static PageNumber readNextPageNumber(PageNumber pageNumber){
    PageData pageData;  // A single instance of PageData can contain the data for next page
    readPage(&pageData, pageNumber, sizeof(PageData));

    return getNextPageNumberFromPage(&pageData);
}

static bool saveNextPageNumber(PageNumber pageNumber, PageNumber nextPageNumber){
    PageData pageData;  // A single instance of PageData (smallest chunk of a page) can contain the data for next page
    readPage(&pageData, pageNumber, sizeof(PageData));              // Read smallest chunk of page (containing the next header)
    memcpy(&pageData, &nextPageNumber, sizeof(PageNumber));          // Modify next header portion
    return writePage(&pageData, pageNumber, sizeof(PageData));      // Save back into storage
}

//////////////////////////////////////////////////////////////////
//  SIZES

static KVATSize getPageNextSize(bool isPartOfMultipleChain){
    return isPartOfMultipleChain ? sizeof(PageNumber) : 0;
}


//////////////////////////////////////////////////////////////////
//  PAGE RECORD

/**
 * Returns the recommended size of the page record based on the number of pages in the format.
 *
 * @return Size in bytes.
 */
static KVATSize getPageRecordSize(){
    // Calculate record size (bytes) based on the number of pages. Each byte can hold record for 8 pages.
    return (index->pageCount/8)+1;
}

/**
 * Sets the status of a page in the runtime record.
 *
 * @param      pageNumber        The number of the page to set status of.
 * @param      isUsed            The status to set. true if used.
 */
static void markPageInRecord(PageNumber pageNumber, bool isUsed){
    if (pageRecord==NULL){return;}
    KVATSize recordSegment = pageNumber/8;
    char recordBit = pageNumber%8;

    if (isUsed){//Bitset
        pageRecord[recordSegment] |= 1<<recordBit;
    }else{
        //Bitclear
        pageRecord[recordSegment] &= ~(1<<recordBit);
    }
}

/**
 * Returns the status of a page based on the runtime record.
 *
 * @param      pageNumber        The number of the page to check
 *
 * @return true    If used
 * @return false   If empty
 */
static bool checkPageFromRecord(PageNumber pageNumber){
    if (pageRecord==NULL || pageNumber==0){return true;}
    KVATSize recordSegment = pageNumber/8;
    char recordBit = pageNumber%8;

    return pageRecord[recordSegment]&(1<<recordBit) ? true : false;
}

/**
 * Gets the number of an empty page in the system based on the runtime page record. Can also mark the empty page as used in record.
 *
 * @param      shouldMarkAsUsed        Indicator for the record marking. Pass true to also mark the page as used.
 *
 * @return Number of an empty page
 */
static PageNumber getEmptyPageNumber(bool shouldMarkAsUsed){
    KVATSize pageRecordSize = getPageRecordSize();
    // Check by bytes (faster)
    KVATSize recordSegment = 0;
    char recordBit = 0;
    for ( ; recordSegment<pageRecordSize; recordSegment++){
        // Check if not full
        if (pageRecord[recordSegment]!=0xFF){
            unsigned char openSegment = pageRecord[recordSegment];
            // Find the 0
            //Check lower nibble
            if ((openSegment|0xF0) != 0xFF){
                // Lower is a charm
                openSegment &= 0x0F;
            }else{
                // High is the way to go
                openSegment = openSegment>>4;
                recordBit = 4;
            }

            // Check remaining 4 bits
            if ((openSegment|0xC) != 0xF){
                // Lower is a charm
                openSegment &= 0x3;
            }else{
                // Go hi
                openSegment = openSegment>>2;
                recordBit+=2;
            }

            // Only 2 bits to go
            if ((openSegment|0x2) != 0x3){
                // Lowest wins. Nothing to do
            }else{
                // High takes the race
                recordBit+=1;
            }
            break;
        }
    }

    PageNumber emptyPageFound = recordSegment*8+recordBit;

    if (shouldMarkAsUsed && emptyPageFound){
        markPageInRecord(emptyPageFound, true);
    }

    return emptyPageFound;
}

/**
 * Finds all the pages being used by a data chain starting in a specific page and sets the record.
 *
 * @param      chainStart        Page number the chain starts in.
 * @param      isActive          Status used to set the pages of the chain as.
 * @param      isMultiple        Indicates if a chain is multiple pages.
 *
 */
static void followPageChainAndSetPageRecord(PageNumber chainStart, bool isActive, bool isChainMultiple){
    if (chainStart==0){return;}

    PageNumber currentPageN = chainStart;
    PageNumber chainPageN = 0;// Marks the position of the current page in the chain
    PageNumber maxPageCount = index->pageCount;

    while (currentPageN!=0 && chainPageN<maxPageCount){    // Stop on chain's end, or on safe limit
        // Mark page in record
        markPageInRecord(currentPageN, isActive);

        if (isChainMultiple){
            // Get next page
            currentPageN = readNextPageNumber(currentPageN);
        }else{
            // There is no next page on single chains
            currentPageN = 0;
        }

        // Add to safe limiter
        chainPageN++;
    }
}

/**
 * Allocates space for pageRecord and traverses the tables to reflect the status of the pages.
 * Called during init process.
 *
 * @return boolean of operation result. true on success.
 */
static bool updatePageRecord(){
    // Update might need reallocation. If called after initial exploration, throwaway.
    if (pageRecord!=NULL){
        free(pageRecord);
    }

    // Get memory to hold the page record
    KVATSize pageRecordSize = getPageRecordSize();
    pageRecord = malloc(pageRecordSize);    // Permanent allocation
    if (pageRecord==NULL){return false;}

    // Set to 0's (empty)
    memset(pageRecord, 0, pageRecordSize);

    // Set page 0 to used (reserved)
    markPageInRecord(0, true);

    // Keep the number of existing entries locally (table entries equals the number of available pages)
    PageNumber numberOfEntries = index->pageCount;
    KVATKeyValueEntry entry;

    bool didReadEntry;

    // Go through all table entries (starting at 1)
    for (PageNumber entryN = 1; entryN<numberOfEntries; entryN++){
        didReadEntry = readTableEntry(&entry, entryN);
        if (!didReadEntry){return false;}

        // Check if entry is active and follow chains for name and value to update records
        if (entry.metadata & MACTIVE){
            //Follow key
            followPageChainAndSetPageRecord(entry.keyPage, true, entry.metadata & MKC_ISMULTIPLE);
            //Follow value
            followPageChainAndSetPageRecord(entry.valuePage, true, entry.metadata & MVC_ISMULTIPLE);
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////
//  FETCH

/**
 * Allocates!
 * Pulls entire data chain into a single allocated buffer and returns pointer. Extra null terminated after max size for security.
 * If expecting to perform multiple fetches, a preallocated memory region can be used for the fetched data.
 *
 * @param      startPage                   The number of the page that the data chain starts on.
 * @param      isChainMultiple             The type of chain. Pass true for a multiple page chain.
 * @param[out] maxSize                     Optional: The maximum size of the data read (if it filled all pages exactly).
 * @param      preallocBuffer              Optional: Reference to memory region for fetched data dumping (recommended for repetitive fetching).
 *                                                   Note: if fetched data does not fit in this buffer, a separate memory region will be allocated
 *                                                         unless true is passed on forceFetchOnPreallocBuffer.
 * @param      preallocBufferSize          Optional: The size of the preallocated buffer.
 * @param      forceFetchOnPreallocBuffer  Optional: Indicates if preallocated buffer should be used even if unfit.
 *
 * @return Pointer to allocated buffer, preallocated buffer if used, or NULL.
 */
static PageDataRef fetchData(PageNumber startPage, bool isChainMultiple, KVATSize* maxSize, PageDataRef preallocBuffer, KVATSize preallocBufferSize, bool forceFetchOnPreallocBuffer){
    //Get total size of chain
    PageNumber pageCount = 1;
    PageNumber currentPageN = startPage;
    if (isChainMultiple){   // Only perform chain size calculation if chain is multiple pages

        for (; pageCount < index->pageCount; pageCount++){
            currentPageN = readNextPageNumber(currentPageN);

            if (currentPageN == 0){
                break;
            }
        }
    }

    // Calculate page internal sizes (take into account the single page case)
    KVATSize pageNextSize = getPageNextSize(isChainMultiple);
    KVATSize pageDataSize = index->pageSize-pageNextSize;
    KVATSize recordSize = pageDataSize*pageCount+1; // Size of the record being fetched (rounded up by page count) (plus 1 byte for null terminator)

    // See if trimming is necessary as a result from forceFetchOnPreallocBuffer
    KVATSize lastPageTrim = 0;
    if (forceFetchOnPreallocBuffer && preallocBufferSize<recordSize){ // Force is on & force is needed
        // we'll have trimming, then
        recordSize = preallocBufferSize;
        pageCount = recordSize/pageDataSize;
        lastPageTrim = recordSize%pageDataSize;
        if (lastPageTrim){pageCount++;};
    }

    // Make buffer to keep a single page
    PageDataRef singlePage = malloc(index->pageSize);
    if (singlePage==NULL){return NULL;}

    // Returnable allocation, see if preallocated buffer can (or should) be used
    PageDataRef record = (preallocBuffer!=NULL && preallocBufferSize>=recordSize) ? preallocBuffer : malloc(recordSize);
    if (record==NULL){free(singlePage); return NULL;}

    // Add null terminator in extra byte (cast to char* so it's indexed by bytes)
    ((char*)record)[recordSize-1] = '\0';

    // Restart current page number
    currentPageN = startPage;

    // Fetch into buffer
    for (PageNumber i = 0; i<pageCount; i++){
        // Get the page (with next page pointer and all)
        readPage(singlePage, currentPageN, 0);

        // Only transfer data to nice record
        // Cast to char* [legal move] to do pointer arithmetic
        // (offset destination to fill the space of the current page)
        // (and offset source to jump over the next page segment).
        // Check if lastPageTrim is active, if so, and this is the last loop,
        // only copy that portion of the page.
        memcpy((char*)record+pageDataSize*i, (char*)singlePage+pageNextSize, (lastPageTrim && i+1==pageCount) ? lastPageTrim : pageDataSize);

        // Get next Page
        currentPageN = getNextPageNumberFromPage(singlePage);
    }

    // Free space from single page
    free(singlePage);

    // Write to inout maxSize
    if (maxSize!=NULL){
        *maxSize = pageCount*pageDataSize;
    }

    return record;
}

//////////////////////////////////////////////////////////////////
//  WRITE

/**
 * Programs data into a page chain in storage.
 *
 * @param      data                      Data to be written to storage.
 * @param      size                      Size of data to be written (in bytes).
 * @param      reuseChainStartPage       Optional: Page number of the beginning of an existing chain to overwrite with data
 * @param      isReuseChainMultiple      Optional: Boolean to indicate if overwrite chain is multiple pages
 * @param[out] didSaveInMultipleChain    Optional: Indicates if data was saved into a chain with multiple pages
 * @param[out] remains                   Optional: Indicates how much space was left empty in the last page written.
 *
 * @return Number of first page in the chain. Returns 0 (illegal page) to indicate insufficient space to store, invalid call, or error.
 *         If write operation runs out
 */
static PageNumber writeData(PageDataRef data, KVATSize size, PageNumber reuseChainStartPage, bool isReuseChainMultiple, bool* didSaveInMultipleChain, KVATSize* remains){
    if (size==0){return 0;}

    // Calculate if data fits in single page
    bool isMultipleChain = size > index->pageSize;

    // Calculate the page segment sizes
    KVATSize pageSize = index->pageSize;
    KVATSize pageNextSize = getPageNextSize(isMultipleChain);
    KVATSize pageDataSize = pageSize-pageNextSize;

    // Calculate pages needed (easy when it's single page)
    PageNumber pagesNeeded = isMultipleChain ? size/pageDataSize : 1;
    if (isMultipleChain && size%pageDataSize){ // Get crude ceil of division
        pagesNeeded++;
    }

    // Guard pages needed (see if it's not even feasible)
    if (pagesNeeded > index->pageCount){return 0;}

    // Get buffer to hold page data when assembling before saving
    PageDataRef pageData = malloc(index->pageSize);
    if (pageData==NULL){return 0;}

    // Support for overwrite chain
    PageNumber reuseChainNext = reuseChainStartPage ? reuseChainStartPage : 0; // Page from reuse chain for next loop, if any
    PageNumber reuseChainDryI = 0; // First iteration in which reuse chain wasn't used.

    // Prepare place to keep track of the pages used
    PageNumber* pagesUsed = malloc(sizeof(PageNumber)*pagesNeeded);
    if (pagesUsed==NULL){free(pageData); return 0;}

    // Effective trackers. The loop cycles thisPage into nextPage before using.
    PageNumber thisPageN = 0;
    PageNumber nextPageN = reuseChainNext ? reuseChainNext : getEmptyPageNumber(true);

    for (PageNumber currentPageI = 0; currentPageI<pagesNeeded; currentPageI++){

        // =========================================
        // == MANAGE & VALIDATE PAGING
        // =========================================

        // Try to cycle to the next overwriteChainNext.
        // Will be transfered to nextPageN before paging managing ends to be used on next loop (if valid).

        if (reuseChainNext && isReuseChainMultiple){ // If the reuse chain "next" from last loop was from a multiple page chain,
                                                     // maybe there is more for next loop.

            reuseChainNext = readNextPageNumber(reuseChainNext); // This will find it.
                                                                 // Or set to 0 if it's filled
                                                                 // (no more) (nothing for next loop).

            // If reuse chain will not be used on next loop, that is the dry iteration
            if (!reuseChainNext){
                reuseChainDryI = currentPageI+1;
            }

        }else if (reuseChainNext){ // If the reuse chain "next" from last loop was from a single chain.
                                   // Last "loop" was actually setup. Also, there is nothing for next loop.

            reuseChainNext = 0;              // Filled (nothing for next loop).
            reuseChainDryI = currentPageI+1; // Means that in the next loop the reuse chain will not be used.
        }

        // Cycle to the next page and validate it

        thisPageN = nextPageN;

        // Storage filled test

        if (thisPageN==0){ // Looks like no more pages were available.

            // Return all new pages obtained (starting right after reuse chain was filled) and fail gracefully.
            for (PageNumber returnI = reuseChainDryI; returnI<currentPageI; returnI++){
                markPageInRecord(pagesUsed[returnI], false); // Mark as not used
            }

            // Correctly terminate reuse chain. If there was one.
            if (isReuseChainMultiple){
                saveNextPageNumber(pagesUsed[reuseChainDryI-1], 0);
            }

            pagesUsed[0] = 0; // Invalidate first page so caller knows that write operation failed
            break;

        }else{
            // Mark in local tracker
            pagesUsed[currentPageI] = thisPageN;
        }

        // See if we will need a next page in the next loop, and get it ready

        if (currentPageI+1<pagesNeeded){ // Need another page

            // Try to reuse chain if available
            nextPageN = reuseChainNext ? reuseChainNext : getEmptyPageNumber(true);

        }else{ // No more pages needed

            nextPageN = 0;
        }

        // =========================================
        // == ACTUAL TRANSFER & WRITING
        // =========================================

        // Write next page number into the working page
        memcpy(pageData, &nextPageN, pageNextSize);

        // Write actual data - cast to char* [legal move] to do pointer arithmetic
        memcpy((char*)pageData+pageNextSize, (char*)data+pageDataSize*currentPageI, pageDataSize);

        // Page is complete, now put it on storage. Write the whole page (no limit).
        writePage(pageData, thisPageN, 0);

    }

    // free allocated buffer
    free(pageData);

    // Free pages used tracker
    free(pagesUsed);

    // Write to inout wasMultipleChain
    if (didSaveInMultipleChain!=NULL){
        *didSaveInMultipleChain = isMultipleChain;
    }

    // Write to inout remains
    if (remains!=NULL){
        KVATSize overflow = size%pageDataSize;
        *remains = overflow ? pageDataSize-overflow : 0;
    }

    // Take care of overwrite chain if not all was used
    if (reuseChainNext){
        followPageChainAndSetPageRecord(reuseChainNext, false, isReuseChainMultiple);
    }

    // Return page number of first page
    return pagesUsed[0];

}

//////////////////////////////////////////////////////////////////
//  LOOKUP

/**
 * Looks for the entry number that matches a key, either exactly or partially.
 *
 * @param      key                       String tag to look for.
 * @param      isPartialKey              Indicates if key passed is only part of the string to match.
 * @param      entryNumberSearchStart    Entry number to start searching from. Valid entry numbers start at 1.
 *
 * @return Number of the first entry that matched the key.
 */
static PageNumber lookupByKey(char* key, bool isPartialKey, PageNumber entryNumberSearchStart){
    PageNumber match = 0;   // To keep the entry that matched

    // Prepare preallocated buffer for multiple key fetches, keep a separate definition for actual return
    char entryKeyPreallocBuff[STRINGKEYSTDLEN];
    char* entryKey;

    // Get the length of the key being searched
    KVATSize keySize = strlen(key);

    PageNumber entryCount = index->pageCount;   // Total number of possible entries. (equals page count by design)
    KVATKeyValueEntry entry;                    // Used to store current entry
    int keyCompare;
    KVATSize entryKeySize;
    bool didReadEntry;
    for (PageNumber entryN = entryNumberSearchStart ? entryNumberSearchStart : 1; entryN<entryCount; entryN++){ // Search in all entries until found

        // Read entry from storage
        didReadEntry = readTableEntry(&entry, entryN);
        if (!didReadEntry){break;} // If readTableEntry fails, abort.

        // Check if entry is active
        if (entry.metadata & MACTIVE){

            // Fetch the key
            entryKey = (char*)fetchData(entry.keyPage, entry.metadata & MKC_ISMULTIPLE, NULL, (PageDataRef)entryKeyPreallocBuff, STRINGKEYSTDLEN, false);
            if (entryKey==NULL){break;} // If fetchData fails, abort.

            // Check key
            entryKeySize = strlen(entryKey);

            // Compare em
            keyCompare = strncmp(key, entryKey, keySize);

            // Cleanup. Yes, at this point is fine. Shall not forget to free this possible allocated space.
            if (entryKey != entryKeyPreallocBuff){ // In the case that the fetch had to reserve a longer buffer
                free(entryKey);
            }

            // Size check
            if ( (isPartialKey && keySize<=entryKeySize) || (!isPartialKey && keySize==entryKeySize)){

                if (!keyCompare){ // 0  means equal
                    match = entryN;
                    break;
                }
            }

        }
    }

    return match;
}

//////////////////////////////////////////////////////////////////
//  PUBLIC SAVE

KVATException KVATSaveValue(char* key, void* value, KVATSize valueSize){
    if (!didInit || !key){return KVATException_invalidAccess;}

    // Get empty table entry for new, or existing for overwrite
    PageNumber tableEntryN = lookupByKey(key, false, 1);   // Look for same key (overwrite)
    bool isOverwrite = true;
    if (tableEntryN==0){                            // Same key not found
        tableEntryN = getEmptyTableEntryNumber();   // Get new entry
        isOverwrite = false;
    }
    // Guard
    if (tableEntryN==0){return KVATException_insufficientSpace;}

    // Get table entry into a local variable. No need to read the entry's current value from storage if not overwriting -it's empty-
    KVATKeyValueEntry tableEntry = {};
    if (isOverwrite){
        bool didReadEntry = readTableEntry(&tableEntry, tableEntryN);
        if (!didReadEntry){return KVATException_tableError;}
    }

    // Set is as open and save with that status
    tableEntry.metadata |= MOPEN;   // All that matters is that it's open, but keep old stuff in case of overwrite
    bool didSaveEntry = saveTableEntry(&tableEntry, tableEntryN);
    if (!didSaveEntry){return KVATException_tableError;}

    bool keySavedInMultipleChain, valueSavedInMultipleChain;
    KVATSize valueRemains;

    // Try to save the key if it's not an overwrite
    if (!isOverwrite){
        PageNumber keyStartPage = writeData((PageDataRef)key, strlen(key)+1, NULL, NULL, &keySavedInMultipleChain, NULL);
        // Guard
        if (keyStartPage==0){return KVATException_insufficientSpace;}
        // Save start page
        tableEntry.keyPage = keyStartPage;
    }

    // Prepare overwrite variables (if needed)
    PageNumber overwriteChainStart = isOverwrite ? tableEntry.valuePage : NULL; // The start page of the old chain
    bool isOverwriteChainMultiple = tableEntry.metadata & MVC_ISMULTIPLE;

    // Try to save the data (value)
    PageNumber valueStartPage = writeData((PageDataRef)value, valueSize, overwriteChainStart, isOverwriteChainMultiple, &valueSavedInMultipleChain, &valueRemains);
    // Guard
    if (valueStartPage==0){return KVATException_insufficientSpace;}
    // Save start page
    tableEntry.valuePage = valueStartPage;

    // Set right metadata.
    if (isOverwrite){
        tableEntry.metadata &= MKC_ISMULTIPLE;   // Only keep previous key settings
    }else{
        tableEntry.metadata = keySavedInMultipleChain ? MKC_MULTIPLE : MKC_SINGLE; // Reset previous contents with new key settings
    }
    tableEntry.metadata |= MACTIVE | (valueSavedInMultipleChain ? MVC_MULTIPLE : MVC_SINGLE) | MKF_STRING;

    // Save remains
    tableEntry.remains = valueRemains;

    // Save entry to storage
    didSaveEntry = saveTableEntry(&tableEntry, tableEntryN);
    if (!didSaveEntry){deinit(); return KVATException_tableError;}  // If saveTableEntry fails at this point, it can be fatal. de-initialize.

    return KVATException_none;
}

/**
 * Saves a string of data tagged with a key. KVATSaveValue convenience.
 *
 * @param      key            String tag for the value to save
 * @param      value          Reference to string to save
 *
 * @return KVATException_ ... See KVATSaveValue
 */
KVATException KVATSaveString(char* key, char* value){
    return KVATSaveValue(key, (void*) value, strlen(value)+1); // Add 1 to length to save null terminator
}

//////////////////////////////////////////////////////////////////
//  PUBLIC RETRIEVE

KVATException KVATRetrieveValue(char* key, void* retrieveBuffer, KVATSize retrieveBufferSize, void** retrievePointerRef, KVATSize* size){
    // Assert
    if (!didInit || !key){return KVATException_invalidAccess;}

    // Reset inout return
    if (retrievePointerRef!=NULL){
        *retrievePointerRef = NULL;
    }

    // Look for this thing
    PageNumber tableEntryN = lookupByKey(key, false, 1);   // Look for same string. See if we need to overwrite.
    if (tableEntryN==0){return KVATException_notFound;}

    // Get entry
    KVATKeyValueEntry tableEntry;
    bool didReadEntry = readTableEntry(&tableEntry, tableEntryN);
    if (!didReadEntry){return KVATException_tableError;}

    KVATSize maxSize = 0;

    // Read value
    PageDataRef value = fetchData(tableEntry.valuePage, tableEntry.metadata & MVC_ISMULTIPLE, &maxSize, retrieveBuffer, retrieveBufferSize, retrieveBuffer!=NULL);
    if (value==NULL){return KVATException_fetchFault;}

    // Calculate actual size
    if (size!=NULL){
        *size = maxSize-tableEntry.remains;
    }

    // Pass return to inout
    if (retrievePointerRef!=NULL){
        *retrievePointerRef = value;
    }

    return KVATException_none;
}

KVATException KVATRetrieveValueByBuffer(char* key, void* retrieveBuffer, KVATSize retrieveBufferSize, KVATSize* size){
    return KVATRetrieveValue(key, retrieveBuffer, retrieveBufferSize, NULL, size);
}

KVATException KVATRetrieveStringByBuffer(char* key, char* retrieveBuffer, KVATSize retrieveBufferSize){
    return KVATRetrieveValue(key, (void*)retrieveBuffer, retrieveBufferSize, NULL, NULL);
}

KVATException KVATRetrieveStringByAllocation(char* key, char** valuePointerRef){
    return KVATRetrieveValue(key, NULL, NULL, (void**) valuePointerRef, NULL);
}

//////////////////////////////////////////////////////////////////
//  PUBLIC RENAME

KVATException KVATChangeKey(char* currentKey, char* newKey){

    if (!didInit || currentKey==NULL || newKey==NULL){return KVATException_invalidAccess;}

    // Look for this thing
    PageNumber tableEntryN = lookupByKey(currentKey, false, 1);
    if (tableEntryN==0){return KVATException_notFound;}

    // Get entry
    KVATKeyValueEntry tableEntry;
    bool didReadEntry = readTableEntry(&tableEntry, tableEntryN);
    if (!didReadEntry){return KVATException_tableError;}

    bool currentKeySavedInMultipleChain = tableEntry.metadata & MKC_ISMULTIPLE;
    bool newKeySavedInMultipleChain;

    // Save new key using the chain of the old key
    PageNumber keyStartPage = writeData((PageDataRef)newKey, strlen(newKey)+1, tableEntry.keyPage, tableEntry.metadata & MKC_ISMULTIPLE, &newKeySavedInMultipleChain, NULL);
    if (!keyStartPage){

        // No luck with new key, try to put old key back
        keyStartPage = writeData((PageDataRef)currentKey, strlen(currentKey)+1, tableEntry.keyPage, tableEntry.metadata & MKC_ISMULTIPLE, NULL, NULL);

        if (!keyStartPage){
            // Still no luck, this is kind of fatal.
            // Sorry to do this, but, loss of data is upon us.
            tableEntry.metadata = MDEFAULT;           // Default this entry
            saveTableEntry(&tableEntry, tableEntryN); // And save it

            deinit(); // Enter safe mode

            return KVATException_unknown;
        }

        return KVATException_insufficientSpace;
    }

    // See if metadata needs changing
    if (newKeySavedInMultipleChain != currentKeySavedInMultipleChain){
        setEntryMetadata(&tableEntry, MKC_ISMULTIPLE, newKeySavedInMultipleChain ? MKC_MULTIPLE : MKC_SINGLE);
        saveTableEntry(&tableEntry, tableEntryN);
    }

    return KVATException_none;
}

//////////////////////////////////////////////////////////////////
//  PUBLIC DELETE

KVATException KVATDeleteValue(char* key){
    // Assert
    if (!didInit || !key){return KVATException_invalidAccess;}

    // Look for this thing
    PageNumber tableEntryN = lookupByKey(key, false, 1);
    if (tableEntryN==0){return KVATException_notFound;}

    // Get entry
    KVATKeyValueEntry tableEntry;
    bool didReadEntry = readTableEntry(&tableEntry, tableEntryN);
    if (!didReadEntry){return KVATException_tableError;}

    // Clear pages used in key and value from registry
    followPageChainAndSetPageRecord(tableEntry.keyPage, false, tableEntry.metadata & MKC_ISMULTIPLE);
    followPageChainAndSetPageRecord(tableEntry.valuePage, false, tableEntry.metadata & MVC_ISMULTIPLE);

    // Change metadata to mark entry as empty
    tableEntry.metadata = MDEFAULT;

    // Save to end
    bool didSaveEntry = saveTableEntry(&tableEntry, tableEntryN);
    if (!didSaveEntry){return KVATException_tableError;}

    return KVATException_none;
}

//////////////////////////////////////////////////////////////////

KVATException KVATInit(){
    if (didInit){return KVATException_invalidAccess;}

    // Enable the EEPROM module.
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_EEPROM0);

    // Wait for the EEPROM module to be ready.
    while(!MAP_SysCtlPeripheralReady(SYSCTL_PERIPH_EEPROM0));

    uint32_t memInitStatus = MAP_EEPROMInit();
    if (memInitStatus==EEPROM_INIT_ERROR){

        return KVATException_storageFault;
    }

    //Get space for the index
    if (index==NULL){
        index = malloc(sizeof(KVATIndex)); // Permanent allocation
        if (index==NULL){return KVATException_heapError;}
    }

    // Read current index from system
    readIndex();

    //Check format ID
    if (index->formatID!=FORMATID){// Need to format memory
        KVATException formatException = formatMemory();
        if (formatException!=KVATException_none){   // There was an exception while formatting. Bubble it up.
            return formatException;
        }
    }

    // Create page record for runtime empty page finding
    bool wasRecordUpdated = updatePageRecord();
    if (!wasRecordUpdated){return KVATException_recordFault;}

    didInit = true;
    return KVATException_none;
}

/**
 * Internal release for major fault. Call upon the occurrence of an unrecoverable error to prevent further damage.
 */
static void deinit(){
    didInit = false;
}
