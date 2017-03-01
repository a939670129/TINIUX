/**********************************************************************************************************
AIOS(Advanced Input Output System) - An Embedded Real Time Operating System (RTOS)
Copyright (C) 2012~2017 SenseRate.Com All rights reserved.
http://www.aios.io -- Documentation, latest information, license and contact details.
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
 SenseRate AIOS may be subject to applicable export control laws and regulations, which might 
 include those applicable to SenseRate AIOS of U.S. and the country in which you are located. 
 Import, export and usage of SenseRate AIOS in any manner by you shall be in compliance with such 
 applicable export control laws and regulations. 
***********************************************************************************************************/

#include "OSList.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
Function    : OSListInitialise 
Description : Initialise a list. 
Input       : ptList -- Pointer of the list to be initialised.
Output      : None 
Return      : None 
*****************************************************************************/
void OSListInitialise( tOSList_t * const ptList )
{
	/* The list structure contains a list item which is used to mark the
	end of the list.  To initialise the list the list end is inserted
	as the only list entry. */
	ptList->ptIndex = ( tOSListItem_t * ) &( ptList->tListEnd );

	/* The list end value is the highest possible value in the list to
	ensure it remains at the end of the list. */
	ptList->tListEnd.uxItemValue = OSPEND_FOREVER_VALUE;

	/* The list end next and previous pointers point to itself so we know
	when the list is empty. */
	ptList->tListEnd.ptNext = ( tOSListItem_t * ) &( ptList->tListEnd );
	ptList->tListEnd.ptPrevious = ( tOSListItem_t * ) &( ptList->tListEnd );

	ptList->tListEnd.pvHolder = OS_NULL; //pvHolder is not use in tListEnd
	ptList->tListEnd.pvList = OS_NULL;   //pvList is in tListEnd

	ptList->uxNumberOfItems = ( uOSBase_t ) 0U;
}

/*****************************************************************************
Function    : OSListItemInitialise 
Description : Initialise a list item. 
Input       : ptListItem -- Pointer of the list item to be initialised.
Output      : None 
Return      : None 
*****************************************************************************/
void OSListItemInitialise( tOSListItem_t * const ptListItem )
{
	/* Make sure the list item is not recorded as being on a list. */
	ptListItem->pvList = OS_NULL;
}

/*****************************************************************************
Function    : OSListInsertItemToEnd 
Description : Insert a new list item into ptList, but rather than sort the list,
              makes the new list item the last item. 
Input       : ptList -- Pointer of the list to be inserted.
              ptNewListItem -- Pointer of a new list item.
Output      : None 
Return      : None 
*****************************************************************************/
void OSListInsertItemToEnd( tOSList_t * const ptList, tOSListItem_t * const ptNewListItem )
{
	tOSListItem_t * const ptIndex = ptList->ptIndex;

	ptNewListItem->ptNext = ptIndex;
	ptNewListItem->ptPrevious = ptIndex->ptPrevious;

	ptIndex->ptPrevious->ptNext = ptNewListItem;
	ptIndex->ptPrevious = ptNewListItem;

	/* Remember which list the item is in. */
	ptNewListItem->pvList = ( void * ) ptList;

	( ptList->uxNumberOfItems )++;
}

/*****************************************************************************
Function    : OSListInsertItem 
Description : Insert the new list item into the list, sorted in uxItemValue order. 
Input       : ptList -- Pointer of the list to be inserted.
              ptNewListItem -- Pointer of a new list item.
Output      : None 
Return      : None 
*****************************************************************************/
void OSListInsertItem( tOSList_t * const ptList, tOSListItem_t * const ptNewListItem )
{
	tOSListItem_t *ptIterator;
	const uOSTick_t uxValueOfInsertion = ptNewListItem->uxItemValue;

	/* If the list already contains a list item with the same item value then the
	new list item should be placed after it.  This ensures that TCB's which are
	stored in ready lists (all of which have the same uxItemValue value) get a
	share of the CPU.  However, if the uxItemValue is the same as the back marker
	the iteration loop below will not end.  Therefore the value is checked
	first, and the algorithm slightly modified if necessary. */
	if( uxValueOfInsertion == OSPEND_FOREVER_VALUE )
	{
		ptIterator = ptList->tListEnd.ptPrevious;
	}
	else
	{
		for( ptIterator = ( tOSListItem_t * ) &( ptList->tListEnd ); ptIterator->ptNext->uxItemValue <= uxValueOfInsertion; ptIterator = ptIterator->ptNext )
		{
			/* There is nothing to do here, just iterating to the wanted
			insertion position. */
		}
	}

	ptNewListItem->ptNext = ptIterator->ptNext;
	ptNewListItem->ptNext->ptPrevious = ptNewListItem;
	ptNewListItem->ptPrevious = ptIterator;
	ptIterator->ptNext = ptNewListItem;

	/* Remember which list the item is in.  This allows fast removal of the
	item later. */
	ptNewListItem->pvList = ( void * ) ptList;

	( ptList->uxNumberOfItems )++;
}

/*****************************************************************************
Function    : OSListRemoveItem 
Description : Remove an item from list. 
Input       : ptItemToRemove -- Pointer of a the list item to be removed.
Output      : None 
Return      : None 
*****************************************************************************/
uOSBase_t OSListRemoveItem( tOSListItem_t * const ptItemToRemove )
{
	/* The list item knows which list it is in.  Obtain the list from the list
	item. */
	tOSList_t * const ptList = ( tOSList_t * ) ptItemToRemove->pvList;
	tOSListItem_t * ptListItemTemp;
	
	ptListItemTemp = ptItemToRemove->ptPrevious;
	ptItemToRemove->ptNext->ptPrevious = ptListItemTemp;
	ptListItemTemp = ptItemToRemove->ptNext;
	ptItemToRemove->ptPrevious->ptNext = ptListItemTemp;

	/* Make sure the index is left pointing to a valid item. */
	if( ptList->ptIndex == ptItemToRemove )
	{
		ptList->ptIndex = ptItemToRemove->ptPrevious;
	}

	ptItemToRemove->pvList = OS_NULL;
	( ptList->uxNumberOfItems )--;

	return ptList->uxNumberOfItems;
}

#ifdef __cplusplus
}
#endif

