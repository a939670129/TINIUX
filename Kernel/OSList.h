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

#ifndef __OS_LIST_H_
#define __OS_LIST_H_

#include "OSType.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OSListItem
{
	volatile uOSTick_t                  uxItemValue;	
	struct OSListItem * volatile        ptNext;
	struct OSListItem * volatile        ptPrevious;
	void *                              pvHolder;
	void * volatile                     pvList;
};
typedef struct OSListItem               tOSListItem_t;	

typedef struct OSList
{
	volatile uOSBase_t                  uxNumberOfItems;
	tOSListItem_t * volatile            ptIndex;
	tOSListItem_t                       tListEnd;
} tOSList_t;

#define OSListItemSetHolder( ptListItem, pxHolder )         ( ( ptListItem )->pvHolder = ( void * ) ( pxHolder ) )
#define OSListItemGetHolder( ptListItem )                   ( ( ptListItem )->pvHolder )
#define OSListItemSetValue( ptListItem, xValue )            ( ( ptListItem )->uxItemValue = ( xValue ) )
#define OSListItemGetValue( ptListItem )                    ( ( ptListItem )->uxItemValue )
#define OSListItemGetNextItem( ptListItem )                 ( ( ptListItem )->ptNext )
#define OSListItemGetList( ptListItem )                     ( ( ptListItem )->pvList )

#define OSlistGetHeadItemValue( ptList )                    ( ( ( ptList )->tListEnd ).ptNext->uxItemValue )
#define OSListGetHeadItem( ptList )                         ( ( ( ptList )->tListEnd ).ptNext )
#define OSListGetEndMarkerItem( ptList )                    ( ( tOSListItem_t const * ) ( &( ( ptList )->tListEnd ) ) )
#define OSListIsEmpty( ptList )                             ( ( uOSBase_t ) ( ( ptList )->uxNumberOfItems == ( uOSBase_t ) 0 ) )
#define OSListGetLength( ptList )                           ( ( ptList )->uxNumberOfItems )

#define OSListGetNextItemHolder(ptList, pxHolder)                                           \
{                                                                                           \
	tOSList_t * const ptConstList = ( ptList );                                             \
	/* Increment the index to the next item and return the item, ensuring */                \
	/* we don't return the marker used at the end of the list.  */                          \
	( ptConstList )->ptIndex = ( ptConstList )->ptIndex->ptNext;                            \
	if( ( void * ) ( ptConstList )->ptIndex == ( void * ) &( ( ptConstList )->tListEnd ) )  \
	{                                                                                       \
		( ptConstList )->ptIndex = ( ptConstList )->ptIndex->ptNext;                        \
	}                                                                                       \
	( pxHolder ) = ( ptConstList )->ptIndex->pvHolder;                                      \
}

#define OSListGetHeadItemHolder( ptList )                   ( (&( ( ptList )->tListEnd ))->ptNext->pvHolder )
#define OSListContainListItem( ptList, ptListItem )         ( ( sOSBase_t ) ( ( ptListItem )->pvList == ( void * ) ( ptList ) ) )
#define OSListISInitialised( ptList )                       ( ( ptList )->tListEnd.uxItemValue == OSPEND_FOREVER_VALUE )

void OSListItemInitialise( tOSListItem_t * const ptListItem );

void OSListInitialise( tOSList_t * const ptList );
void OSListInsertItem( tOSList_t * const ptList, tOSListItem_t * const ptNewListItem );
void OSListInsertItemToEnd( tOSList_t * const ptList, tOSListItem_t * const ptNewListItem );
uOSBase_t OSListRemoveItem( tOSListItem_t * const ptItemToRemove );

#ifdef __cplusplus
}
#endif

#endif //__OS_LIST_H_
