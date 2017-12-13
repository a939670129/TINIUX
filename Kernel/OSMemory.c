/**********************************************************************************************************
TINIUX - An Embedded Real Time Operating System (RTOS)
Copyright (C) 2012~2017 SenseRate.Com All rights reserved.
http://www.tiniux.org -- Documentation, latest information, license and contact details.
http://www.SenseRate.com -- Commercial support, development, porting, licensing and training services.
--------------------------------------------------------------------------------------------------------
Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met: 
1. Redistributions of source code must retain the above copyright notice, this list of 
conditions and the following disclaimer. 
2. Redistributions in binary form must reproduce the above copyright notice, this list 
of conditions and the following disclaimer in the documentation and/or other materials 
provided with the distribution. 
3. Neither the name of the copyright holder nor the names of its contributors may be used 
to endorse or promote products derived from this software without specific prior written 
permission. 
--------------------------------------------------------------------------------------------------------
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
--------------------------------------------------------------------------------------------------------
 Notice of Export Control Law 
--------------------------------------------------------------------------------------------------------
 SenseRate TINIUX may be subject to applicable export control laws and regulations, which might 
 include those applicable to SenseRate TINIUX of U.S. and the country in which you are located. 
 Import, export and usage of SenseRate TINIUX in any manner by you shall be in compliance with such 
 applicable export control laws and regulations. 
***********************************************************************************************************/

#include "TINIUX.h"
#include "OSMemory.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The heap is made up as a list of structs of this type.
 * This does not have to be aligned since for getting its size,
 * we only use the macro SIZEOF_OSMEM_ALIGNED, which automatically alignes.*/
typedef struct _tOSMem 
{
  uOSMemSize_t NextMem;	/** index (-> gpOSMemBegin[NextMem]) of the next struct */
  uOSMemSize_t PrevMem;	/** index (-> gpOSMemBegin[PrevMem]) of the previous struct */
  uOS8_t Used;			/** 1: this memory block is Used; 0: this memory block is unused */
}tOSMem_t;

/** All allocated blocks will be MIN_SIZE bytes big, at least!
 * MIN_SIZE can be overridden to suit your needs. Smaller values save space,
 * larger values could prevent too small blocks to fragment the RAM too much. */
#ifndef MIN_SIZE
#define MIN_SIZE             16
#endif /* MIN_SIZE */
/** some alignment macros: we define them here for better source code layout */
#define OSMIN_SIZE_ALIGNED		OSMEM_ALIGN_SIZE(MIN_SIZE)
#define SIZEOF_OSMEM_ALIGNED	OSMEM_ALIGN_SIZE(sizeof(tOSMem_t))
#define OSMEM_SIZE_ALIGNED		OSMEM_ALIGN_SIZE(OSMEM_SIZE)

/** If you want to relocate the heap to external memory, simply define
 * OSRAM_HEAP_POINTER as a void-pointer to that location.
 * If so, make sure the memory at that location is big enough (see below on
 * how that space is calculated). */
#ifndef OSRAM_HEAP_POINTER
/** the heap. we need one tOSMem_t at the end and some room for alignment */
uOS8_t OSRamHeap[OSMEM_SIZE_ALIGNED + (2U*SIZEOF_OSMEM_ALIGNED) + OSMEM_ALIGNMENT];
#define OSRAM_HEAP_POINTER OSRamHeap
#endif /* OSRAM_HEAP_POINTER */

/** pointer to the heap (OSRamHeap): for alignment, gpOSMemBegin is now a pointer instead of an array */
static uOS8_t *gpOSMemBegin = OS_NULL;
/** the last entry, always unused! */
static tOSMem_t *gpOSMemEnd = OS_NULL;
/** pointer to the lowest free block, this is Used for faster search */
static tOSMem_t *gpOSMemLFree = OS_NULL;


/***************************************************************************** 
Function    : OSMemCombine 
Description : "OSMemCombine" by combining adjacent empty struct mems.
              After this function is through, there should not exist
              one empty tOSMem_t pointing to another empty tOSMem_t.
              this function is only called by OSMemFree() and OSMemTrim(),
              This assumes access to the heap is protected by the calling function
              already.
Input       : ptOSMem -- the point to a tOSMem_t which just has been freed.
Output      : None 
Return      : None 
*****************************************************************************/
static void OSMemCombine(tOSMem_t *ptOSMem)
{
	tOSMem_t *ptNextOSMem;
	tOSMem_t *ptPrevOSMem;

	if ( ptOSMem->Used==1 )
	{
		return;
	}
	
	// Combine forward 
	ptNextOSMem = (tOSMem_t *)(void *)&gpOSMemBegin[ptOSMem->NextMem];
	if (ptOSMem != ptNextOSMem && ptNextOSMem->Used == 0 && (uOS8_t *)ptNextOSMem != (uOS8_t *)gpOSMemEnd) 
	{
		// if ptOSMem->NextMem is unused and not end of gpOSMemBegin, combine ptOSMem and ptOSMem->NextMem 
		if (gpOSMemLFree == ptNextOSMem) 
		{
			gpOSMemLFree = ptOSMem;
		}
		ptOSMem->NextMem = ptNextOSMem->NextMem;
		((tOSMem_t *)(void *)&gpOSMemBegin[ptNextOSMem->NextMem])->PrevMem = (uOSMemSize_t)((uOS8_t *)ptOSMem - gpOSMemBegin);
	}

	// Combine backward 
	ptPrevOSMem = (tOSMem_t *)(void *)&gpOSMemBegin[ptOSMem->PrevMem];
	if (ptPrevOSMem != ptOSMem && ptPrevOSMem->Used == 0) 
	{
		// if ptOSMem->PrevMem is unused, combine ptOSMem and ptOSMem->PrevMem 
		if (gpOSMemLFree == ptOSMem) 
		{
			gpOSMemLFree = ptPrevOSMem;
		}
		ptPrevOSMem->NextMem = ptOSMem->NextMem;
		((tOSMem_t *)(void *)&gpOSMemBegin[ptOSMem->NextMem])->PrevMem = (uOSMemSize_t)((uOS8_t *)ptPrevOSMem - gpOSMemBegin);
	}
	return;
}

/***************************************************************************** 
Function    : OSMemInit 
Description : Zero the heap and initialize start, end and lowest-free pointer.
Input       : None
Output      : None 
Return      : None 
*****************************************************************************/
void OSMemInit(void)
{
	tOSMem_t *ptOSMemTemp;

	// align the heap 
	gpOSMemBegin = (uOS8_t *)OSMEM_ALIGN_ADDR(OSRAM_HEAP_POINTER);
	
	// initialize the start of the heap 
	ptOSMemTemp = (tOSMem_t *)(void *)gpOSMemBegin;
	ptOSMemTemp->NextMem = OSMEM_SIZE_ALIGNED;
	ptOSMemTemp->PrevMem = 0;
	ptOSMemTemp->Used = 0;
	
	// initialize the end of the heap 
	gpOSMemEnd = (tOSMem_t *)(void *)&gpOSMemBegin[OSMEM_SIZE_ALIGNED];
	gpOSMemEnd->Used = 1;
	gpOSMemEnd->NextMem = OSMEM_SIZE_ALIGNED;
	gpOSMemEnd->PrevMem = OSMEM_SIZE_ALIGNED;

	// initialize the lowest-free pointer to the start of the heap 
	gpOSMemLFree = (tOSMem_t *)(void *)gpOSMemBegin;
	
	return;
}

/***************************************************************************** 
Function    : OSMemFree 
Description : Put a tOSMem_t back on the heap. 
Input       : pMem -- the data portion of a tOSMem_t as returned by a previous 
                      call to OSMemMalloc()
Output      : None 
Return      : None 
*****************************************************************************/ 
void OSMemFree(void *pMem)
{
	tOSMem_t *ptOSMemTemp;

	if (pMem == OS_NULL) 
	{
		return;
	}

	if ((uOS8_t *)pMem < (uOS8_t *)gpOSMemBegin || (uOS8_t *)pMem >= (uOS8_t *)gpOSMemEnd) 
	{
		return;
	}
	
	// protect the heap from concurrent access 
	OSIntLock();
	// Get the corresponding tOSMem_t ... 
	ptOSMemTemp = (tOSMem_t *)(void *)((uOS8_t *)pMem - SIZEOF_OSMEM_ALIGNED);
	
	//ptOSMemTemp->Used must be 1
	if( ptOSMemTemp->Used==1 )
	{
		// now set it unused. 
		ptOSMemTemp->Used = 0;

		if (ptOSMemTemp < gpOSMemLFree) 
		{
			// the newly freed struct is now the lowest 
			gpOSMemLFree = ptOSMemTemp;
		}

		// finally, see if prev or next are free also 
		OSMemCombine(ptOSMemTemp);		
	}
	OSIntUnlock();
	
	return;
}

/***************************************************************************** 
Function    : OSMemTrim 
Description : Shrink memory returned by OSMemMalloc().
Input       : pMem -- the pointer to memory allocated by OSMemMalloc is to be shrinked
              newsize -- required size after shrinking (needs to be smaller than or
                         equal to the previous size)
Output      : None 
Return      : for compatibility reasons: is always == pMem, at the moment
              or OS_NULL if newsize is > old size, in which case pMem is NOT touched
              or freed!
*****************************************************************************/ 
void* OSMemTrim(void *pMem, uOSMemSize_t newsize)
{
	uOSMemSize_t size;
	uOSMemSize_t ptr, ptr2;
	tOSMem_t *ptOSMemTemp, *ptOSMemTemp2;

	// Expand the size of the allocated memory region so that we can adjust for alignment. 
	newsize = OSMEM_ALIGN_SIZE(newsize);

	if(newsize < OSMIN_SIZE_ALIGNED) 
	{
		// every data block must be at least OSMIN_SIZE_ALIGNED long 
		newsize = OSMIN_SIZE_ALIGNED;
	}

	if (newsize > OSMEM_SIZE_ALIGNED) 
	{
		return OS_NULL;
	}

	if ((uOS8_t *)pMem < (uOS8_t *)gpOSMemBegin || (uOS8_t *)pMem >= (uOS8_t *)gpOSMemEnd) 
	{
		return pMem;
	}
	// Get the corresponding tOSMem_t 
	ptOSMemTemp = (tOSMem_t *)(void *)((uOS8_t *)pMem - SIZEOF_OSMEM_ALIGNED);
	// Get its offset pointer 
	ptr = (uOSMemSize_t)((uOS8_t *)ptOSMemTemp - gpOSMemBegin);

	size = ptOSMemTemp->NextMem - ptr - SIZEOF_OSMEM_ALIGNED;
	if (newsize > size) 
	{
		// not supported
		return OS_NULL;
	}
	if (newsize == size) 
	{
		// No change in size, simply return 
		return pMem;
	}

	// protect the heap from concurrent access 
	OSIntLock();

	ptOSMemTemp2 = (tOSMem_t *)(void *)&gpOSMemBegin[ptOSMemTemp->NextMem];
	if(ptOSMemTemp2->Used == 0) 
	{
		// The next struct is unused, we can simply move it at little 
		uOSMemSize_t NextMem;
		// remember the old next pointer 
		NextMem = ptOSMemTemp2->NextMem;
		// create new tOSMem_t which is moved directly after the shrinked ptOSMemTemp 
		ptr2 = ptr + SIZEOF_OSMEM_ALIGNED + newsize;
		if (gpOSMemLFree == ptOSMemTemp2) 
		{
			gpOSMemLFree = (tOSMem_t *)(void *)&gpOSMemBegin[ptr2];
		}
		ptOSMemTemp2 = (tOSMem_t *)(void *)&gpOSMemBegin[ptr2];
		ptOSMemTemp2->Used = 0;
		// restore the next pointer 
		ptOSMemTemp2->NextMem = NextMem;
		// link it back to ptOSMemTemp 
		ptOSMemTemp2->PrevMem = ptr;
		// link ptOSMemTemp to it 
		ptOSMemTemp->NextMem = ptr2;
		// last thing to restore linked list: as we have moved ptOSMemTemp2,
		// let 'ptOSMemTemp2->NextMem->PrevMem' point to ptOSMemTemp2 again. but only if ptOSMemTemp2->NextMem is not
		// the end of the heap 
		if (ptOSMemTemp2->NextMem != OSMEM_SIZE_ALIGNED) 
		{
			((tOSMem_t *)(void *)&gpOSMemBegin[ptOSMemTemp2->NextMem])->PrevMem = ptr2;
		}
		// no need to combine, we've already done that 
	} 
	else if (newsize + SIZEOF_OSMEM_ALIGNED + OSMIN_SIZE_ALIGNED <= size) 
	{
		// Next struct is used but there's room for another tOSMem_t with
		// at least OSMIN_SIZE_ALIGNED of data.
		// Old size ('size') must be big enough to contain at least 'newsize' plus a tOSMem_t
		// ('SIZEOF_OSMEM_ALIGNED') with some data ('OSMIN_SIZE_ALIGNED').
		ptr2 = ptr + SIZEOF_OSMEM_ALIGNED + newsize;
		ptOSMemTemp2 = (tOSMem_t *)(void *)&gpOSMemBegin[ptr2];
		if (ptOSMemTemp2 < gpOSMemLFree) 
		{
			gpOSMemLFree = ptOSMemTemp2;
		}
		ptOSMemTemp2->Used = 0;
		ptOSMemTemp2->NextMem = ptOSMemTemp->NextMem;
		ptOSMemTemp2->PrevMem = ptr;
		ptOSMemTemp->NextMem = ptr2;
		if (ptOSMemTemp2->NextMem != OSMEM_SIZE_ALIGNED) 
		{
			((tOSMem_t *)(void *)&gpOSMemBegin[ptOSMemTemp2->NextMem])->PrevMem = ptr2;
		}
		// the original ptOSMemTemp->NextMem is Used, so no need to combine! 
	}
/*	else 
	{
		next tOSMem_t is Used but size between ptOSMemTemp and ptOSMemTemp2 is not big enough
		to create another tOSMem_t
		-> don't do anyhting. 
		-> the remaining space stays unused since it is too small
	} 
*/
  OSIntUnlock();
  return pMem;
}

/***************************************************************************** 
Function    : OSMemMalloc 
Description : Allocate a block of memory with a minimum of 'size' bytes.
Input       : size -- the minimum size of the requested block in bytes.
Output      : None 
Return      : pointer to allocated memory or OS_NULL if no free memory was found.
              the returned value will always be aligned (as defined by OSMEM_ALIGNMENT).
*****************************************************************************/ 
void* OSMemMalloc(uOSMemSize_t size)
{
	uOS8_t * pResult = OS_NULL;
	uOSMemSize_t ptr, ptr2;
	tOSMem_t *ptOSMemTemp, *ptOSMemTemp2;

	if(gpOSMemEnd==OS_NULL)
	{
		OSMemInit();
		if(gpOSMemEnd==OS_NULL)
		{
			return pResult;
		}
	}
	if (size == 0) 
	{
		return pResult;
	}

	// Expand the size of the allocated memory region so that we can
	// adjust for alignment. 
	size = OSMEM_ALIGN_SIZE(size);

	if(size < OSMIN_SIZE_ALIGNED) 
	{
		// every data block must be at least OSMIN_SIZE_ALIGNED long 
		size = OSMIN_SIZE_ALIGNED;
	}

	if (size > OSMEM_SIZE_ALIGNED) 
	{
		return pResult;
	}

	// protect the heap from concurrent access 
	OSIntLock();

	// Scan through the heap searching for a free block that is big enough,
	// beginning with the lowest free block.
	for (ptr = (uOSMemSize_t)((uOS8_t *)gpOSMemLFree - gpOSMemBegin); ptr < OSMEM_SIZE_ALIGNED - size;
		ptr = ((tOSMem_t *)(void *)&gpOSMemBegin[ptr])->NextMem) 
	{
		ptOSMemTemp = (tOSMem_t *)(void *)&gpOSMemBegin[ptr];

		if ((!ptOSMemTemp->Used) && (ptOSMemTemp->NextMem - (ptr + SIZEOF_OSMEM_ALIGNED)) >= size) 
		{
			// ptOSMemTemp is not Used and at least perfect fit is possible:
			// ptOSMemTemp->NextMem - (ptr + SIZEOF_OSMEM_ALIGNED) gives us the 'user data size' of ptOSMemTemp 

			if (ptOSMemTemp->NextMem - (ptr + SIZEOF_OSMEM_ALIGNED) >= (size + SIZEOF_OSMEM_ALIGNED + OSMIN_SIZE_ALIGNED)) 
			{
				// (in addition to the above, we test if another tOSMem_t (SIZEOF_OSMEM_ALIGNED) containing
				// at least OSMIN_SIZE_ALIGNED of data also fits in the 'user data space' of 'ptOSMemTemp')
				// -> split large block, create empty remainder,
				// remainder must be large enough to contain OSMIN_SIZE_ALIGNED data: if
				// ptOSMemTemp->NextMem - (ptr + (2*SIZEOF_OSMEM_ALIGNED)) == size,
				// tOSMem_t would fit in but no data between ptOSMemTemp2 and ptOSMemTemp2->NextMem
				ptr2 = ptr + SIZEOF_OSMEM_ALIGNED + size;
				// create ptOSMemTemp2 struct 
				ptOSMemTemp2 = (tOSMem_t *)(void *)&gpOSMemBegin[ptr2];
				ptOSMemTemp2->Used = 0;
				ptOSMemTemp2->NextMem = ptOSMemTemp->NextMem;
				ptOSMemTemp2->PrevMem = ptr;
				// and insert it between ptOSMemTemp and ptOSMemTemp->NextMem 
				ptOSMemTemp->NextMem = ptr2;
				ptOSMemTemp->Used = 1;

				if (ptOSMemTemp2->NextMem != OSMEM_SIZE_ALIGNED) 
				{
					((tOSMem_t *)(void *)&gpOSMemBegin[ptOSMemTemp2->NextMem])->PrevMem = ptr2;
				}
			} 
			else 
			{
				// (a ptOSMemTemp2 struct does no fit into the user data space of ptOSMemTemp and ptOSMemTemp->NextMem will always
				// be Used at this point: if not we have 2 unused structs in a row, OSMemCombine should have
				// take care of this).
				// -> near fit or excact fit: do not split, no ptOSMemTemp2 creation
				// also can't move ptOSMemTemp->NextMem directly behind ptOSMemTemp, since ptOSMemTemp->NextMem
				// will always be Used at this point!
				ptOSMemTemp->Used = 1;
			}

			if (ptOSMemTemp == gpOSMemLFree) 
			{
				// Find next free block after ptOSMemTemp and update lowest free pointer 
				while (gpOSMemLFree->Used && gpOSMemLFree != gpOSMemEnd) 
				{
					gpOSMemLFree = (tOSMem_t *)(void *)&gpOSMemBegin[gpOSMemLFree->NextMem];
				}
			}
			pResult = (uOS8_t *)ptOSMemTemp + SIZEOF_OSMEM_ALIGNED;
			break;
		}
	}

	OSIntUnlock();

	return pResult;
}

/***************************************************************************** 
Function    : OSMemCalloc 
Description : Contiguously allocates enough space for count objects that are size bytes
              of memory each and returns a pointer to the allocated memory.
              The allocated memory is filled with bytes of value zero.
Input       : count -- number of objects to allocate.
              size -- size of the objects to allocate.
Output      : None 
Return      : pointer to allocated memory / OS_NULL pointer if there is an error.
*****************************************************************************/ 
void* OSMemCalloc(uOSMemSize_t count, uOSMemSize_t size)
{
	void *pMem;

	// allocate 'count' objects of size 'size' 
	pMem = OSMemMalloc(count * size);
	if (pMem) 
	{
		// zero the memory 
		memset(pMem, 0, count * size);
	}
	return pMem;
}

#ifdef __cplusplus
}
#endif
