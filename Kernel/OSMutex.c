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

#include "AIOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#if (OS_MUTEX_ON==1)

AIOS_DATA static sOSBase_t const MUTEX_STATUS_UNLOCKED				= ( ( sOSBase_t ) -1 );
AIOS_DATA static sOSBase_t const MUTEX_STATUS_LOCKED				= ( ( sOSBase_t ) 0 );
AIOS_DATA static uOSTick_t const MUTEX_UNLOCK_BLOCK_TIME			= ( ( uOSTick_t ) 0U );

static uOSBool_t OSMutexIsEmpty( OSMutexHandle_t MutexHandle )
{
	uOSBool_t bReturn;
	tOSMutex_t * const ptMutex = ( tOSMutex_t * ) MutexHandle;
	
	OSIntLock();
	{
		if( ptMutex->uxCurNum == ( uOSBase_t )  0 )
		{
			bReturn = OS_TRUE;
		}
		else
		{
			bReturn = OS_FALSE;
		}
	}
	OSIntUnlock();

	return bReturn;
}

static uOSBool_t OSMutexIsFull( OSMutexHandle_t MutexHandle )
{
	uOSBool_t bReturn;
	tOSMutex_t * const ptMutex = ( tOSMutex_t * ) MutexHandle;

	OSIntLock();
	{
		if( ptMutex->uxCurNum == ptMutex->uxMaxNum )
		{
			bReturn = OS_TRUE;
		}
		else
		{
			bReturn = OS_FALSE;
		}
	}
	OSIntUnlock();

	return bReturn;
}

#define OSMutexStatusLock( ptMutex )                         \
	OSIntLock();											\
	{														\
		if( ( ptMutex )->xMutexPLock == MUTEX_STATUS_UNLOCKED )	\
		{													\
			( ptMutex )->xMutexPLock = MUTEX_STATUS_LOCKED;		\
		}													\
		if( ( ptMutex )->xMutexVLock == MUTEX_STATUS_UNLOCKED )	\
		{													\
			( ptMutex )->xMutexVLock = MUTEX_STATUS_LOCKED;		\
		}													\
	}														\
	OSIntUnlock()


static void OSMutexStatusUnlock( tOSMutex_t * const ptMutex )
{
	/* THIS FUNCTION MUST BE CALLED WITH THE SCHEDULER LOCKED. */

	OSIntLock();
	{
		sOSBase_t xMutexVLock = ptMutex->xMutexVLock;
		
		while( xMutexVLock > MUTEX_STATUS_LOCKED )
		{
			if( OSListIsEmpty( &( ptMutex->tMutexPTaskList ) ) == OS_FALSE )
			{
				if( OSTaskListOfEventRemove( &( ptMutex->tMutexPTaskList ) ) != OS_FALSE )
				{
					OSTaskNeedSchedule();
				}
			}
			else
			{
				break;
			}

			--xMutexVLock;
		}

		ptMutex->xMutexVLock = MUTEX_STATUS_UNLOCKED;
	}
	OSIntUnlock();

	/* Do the same for the Rx lock. */
	OSIntLock();
	{
		sOSBase_t xMutexPLock = ptMutex->xMutexPLock;
		
		while( xMutexPLock > MUTEX_STATUS_LOCKED )
		{
			if( OSListIsEmpty( &( ptMutex->tMutexVTaskList ) ) == OS_FALSE )
			{
				if( OSTaskListOfEventRemove( &( ptMutex->tMutexVTaskList ) ) != OS_FALSE )
				{
					OSTaskNeedSchedule();
				}

				--xMutexPLock;
			}
			else
			{
				break;
			}
		}

		ptMutex->xMutexPLock = MUTEX_STATUS_UNLOCKED;
	}
	OSIntUnlock();
}


OSMutexHandle_t OSMutexCreate( void )
{
	tOSMutex_t *ptNewMutex;

	/* Allocate the new queue structure. */
	ptNewMutex = ( tOSMutex_t * ) OSMemMalloc( sizeof( tOSMutex_t ) );
	if( ptNewMutex != OS_NULL )
	{
		/* Information required for priority inheritance. */
		ptNewMutex->pxMutexHolder = OS_NULL;

		ptNewMutex->uxCurNum = ( uOSBase_t ) 1U;
		ptNewMutex->uxMaxNum = ( uOSBase_t ) 1U;
		ptNewMutex->xMutexPLock = MUTEX_STATUS_UNLOCKED;
		ptNewMutex->xMutexVLock = MUTEX_STATUS_UNLOCKED;

		/* Ensure the event queues start with the correct state. */
		OSListInitialise( &( ptNewMutex->tMutexVTaskList ) );
		OSListInitialise( &( ptNewMutex->tMutexPTaskList ) );
	}
	return (OSMutexHandle_t)ptNewMutex;
}

void OSMutexDelete( OSMutexHandle_t MutexHandle )
{
	tOSMutex_t * const ptMutex = ( tOSMutex_t * ) MutexHandle;

	OSMemFree( ptMutex );
}

sOSBase_t OSMutexSetID(OSMutexHandle_t const MutexHandle, sOSBase_t xID)
{
	if(MutexHandle == OS_NULL)
	{
		return 1;
	}
	OSIntLock();
	{
		MutexHandle->xID = xID;
	}
	OSIntUnlock();

	return 0;
}

sOSBase_t OSMutexGetID(OSMutexHandle_t const MutexHandle)
{
	sOSBase_t xID = 0;
	
	OSIntLock();
	if(MutexHandle != OS_NULL)
	{
		xID = MutexHandle->xID;
	}
	OSIntUnlock();

	return xID;	
}

uOSBool_t OSMutexLock( OSMutexHandle_t MutexHandle, uOSTick_t uxTicksToWait )
{
	uOSBool_t bEntryTimeSet = OS_FALSE;
	tOSTimeOut_t tTimeOut;
	tOSMutex_t * const ptMutex = ( tOSMutex_t * ) MutexHandle;

	for( ;; )
	{
		OSIntLock();
		{
			const uOSBase_t uxCurNum = ptMutex->uxCurNum;

			if( uxCurNum > ( uOSBase_t ) 0 )
			{
				ptMutex->uxCurNum = uxCurNum - 1;
				ptMutex->pxMutexHolder = ( sOS8_t * ) OSTaskGetMutexHolder();
				
				if( OSListIsEmpty( &( ptMutex->tMutexVTaskList ) ) == OS_FALSE )
				{
					if( OSTaskListOfEventRemove( &( ptMutex->tMutexVTaskList ) ) != OS_FALSE )
					{
						OSSchedule();
					}
				}

				OSIntUnlock();
				return OS_TRUE;
			}
			else
			{
				if( uxTicksToWait == ( uOSTick_t ) 0 )
				{
					OSIntUnlock();
					//the mutex is empty
					return OS_FALSE;
				}
				else if( bEntryTimeSet == OS_FALSE )
				{
					OSTaskSetTimeOutState( &tTimeOut );
					bEntryTimeSet = OS_TRUE;
				}
			}
		}
		OSIntUnlock();

		/* Interrupts and other tasks can unlock or lock from the mutex
		To avoid confusion, we lock the scheduler and the mutex. */
		OSScheduleLock();
		OSMutexStatusLock( ptMutex );

		if( OSTaskGetTimeOutState( &tTimeOut, &uxTicksToWait ) == OS_FALSE )
		{
			if( OSMutexIsEmpty( ptMutex ) != OS_FALSE )
			{
				OSIntLock();
				{
					OSTaskPriorityInherit( ( void * ) ptMutex->pxMutexHolder );
				}
				OSIntUnlock();
				
				OSTaskListOfEventAdd( &( ptMutex->tMutexPTaskList ), uxTicksToWait );
				OSMutexStatusUnlock( ptMutex );
				if( OSScheduleUnlock() == OS_FALSE )
				{
					OSSchedule();
				}
			}
			else
			{
				/* Try again. */
				OSMutexStatusUnlock( ptMutex );
				( void ) OSScheduleUnlock();
			}
		}
		else
		{
			OSMutexStatusUnlock( ptMutex );
			( void ) OSScheduleUnlock();
			
			if( OSMutexIsEmpty( ptMutex ) != OS_FALSE )
			{
				//the Mutex is empty
				return OS_FALSE;
			}
		}
	}
}

uOSBool_t OSMutexUnlock( OSMutexHandle_t MutexHandle )
{
	uOSBool_t bEntryTimeSet = OS_FALSE, bNeedSchedule;
	tOSTimeOut_t tTimeOut;
	tOSMutex_t * const ptMutex = ( tOSMutex_t * ) MutexHandle;

	uOSTick_t uxTicksToWait = MUTEX_UNLOCK_BLOCK_TIME;

	for( ;; )
	{
		OSIntLock();
		{
			const uOSBase_t uxCurNum = ptMutex->uxCurNum;

			if( uxCurNum < ptMutex->uxMaxNum )
			{
				/* The mutex is no longer being held. */
				bNeedSchedule = OSTaskPriorityDisinherit( ( void * ) ptMutex->pxMutexHolder );
				ptMutex->pxMutexHolder = OS_NULL;
				ptMutex->uxCurNum = uxCurNum + 1;

				if( OSListIsEmpty( &( ptMutex->tMutexPTaskList ) ) == OS_FALSE )
				{
					if( OSTaskListOfEventRemove( &( ptMutex->tMutexPTaskList ) ) != OS_FALSE )
					{
						OSSchedule();
					}
				}
				else if( bNeedSchedule != OS_FALSE )
				{
					OSSchedule();
				}

				OSIntUnlock();
				return OS_TRUE;
			}
			else
			{
				if( uxTicksToWait == ( uOSTick_t ) 0 )
				{
					OSIntUnlock();
					//the mutex is full
					return OS_FALSE;
				}
				else if( bEntryTimeSet == OS_FALSE )
				{
					OSTaskSetTimeOutState( &tTimeOut );
					bEntryTimeSet = OS_TRUE;
				}
			}
		}
		OSIntUnlock();

		/* Interrupts and other tasks can unlock or lock from the mutex
		To avoid confusion, we lock the scheduler and the mutex. */
		OSScheduleLock();
		OSMutexStatusLock( ptMutex );

		if( OSTaskGetTimeOutState( &tTimeOut, &uxTicksToWait ) == OS_FALSE )
		{
			if( OSMutexIsFull( ptMutex ) != OS_FALSE )
			{
				OSTaskListOfEventAdd( &( ptMutex->tMutexVTaskList ), uxTicksToWait );

				OSMutexStatusUnlock( ptMutex );

				if( OSScheduleUnlock() == OS_FALSE )
				{
					OSSchedule();
				}
			}
			else
			{
				/* Try again. */
				OSMutexStatusUnlock( ptMutex );
				( void ) OSScheduleUnlock();
			}
		}
		else
		{
			/* The timeout has expired. */
			OSMutexStatusUnlock( ptMutex );
			( void ) OSScheduleUnlock();
			//the mutex is full
			return OS_FALSE;
		}
	}
}

#endif //(OS_MUTEX_ON==1)

#ifdef __cplusplus
}
#endif
