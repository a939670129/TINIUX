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

#include <string.h>
#include "AIOS.h"

#ifdef __cplusplus
extern "C" {
#endif

AIOS_DATA tOSTCB_t * volatile gptCurrentTCB = OS_NULL;

/* Lists for ready and blocked tasks. --------------------*/
AIOS_DATA static tOSList_t gtOSReadyTaskList[ OSHIGHEAST_PRIORITY ];
AIOS_DATA static tOSList_t gtOSTimer1TaskList;
AIOS_DATA static tOSList_t gtOSTimer2TaskList;
AIOS_DATA static tOSList_t * volatile gptOSTimerTaskList;
AIOS_DATA static tOSList_t * volatile gptOSOFTimerTaskList;
AIOS_DATA static tOSList_t gtOSPendingReadyTaskList;
//suspend task list
AIOS_DATA static tOSList_t gtOSSuspendedTaskList;

// delete task
AIOS_DATA static tOSList_t gtOSRecycleTaskList;
AIOS_DATA static volatile 	uOSBase_t guxTasksDeleted 			= ( uOSBase_t ) 0U;

/* Other file private variables. --------------------------------*/
AIOS_DATA static volatile 	uOSBase_t guxCurrentNumberOfTasks 	= ( uOSBase_t ) 0U;
AIOS_DATA static volatile 	uOSTick_t guxTickCount 				= ( uOSTick_t ) 0U;
AIOS_DATA static volatile 	uOSBase_t guxTopReadyPriority 		= OSLOWEAST_PRIORITY;
AIOS_DATA static volatile 	uOSBool_t gbSchedulerRunning 		= OS_FALSE;
AIOS_DATA static volatile 	uOSBase_t guxPendedTicks 			= ( uOSBase_t ) 0U;
AIOS_DATA static volatile 	uOSBool_t gbNeedSchedule 			= OS_FALSE;
AIOS_DATA static volatile 	sOSBase_t gxNumOfOverflows 			= ( sOSBase_t ) 0U;
AIOS_DATA static volatile 	uOSTick_t guxNextTaskUnblockTime	= ( uOSTick_t ) 0U;
AIOS_DATA static volatile 	uOSBase_t guxSchedulerLocked		= ( uOSBase_t ) OS_FALSE;

#if ( OS_TASK_SIGNAL_ON == 1 )
AIOS_DATA static uOSBase_t const SIG_STATE_NOTWAITING		= ( ( uOSBase_t ) 0 );
AIOS_DATA static uOSBase_t const SIG_STATE_WAITING			= ( ( uOSBase_t ) 1 );
AIOS_DATA static uOSBase_t const SIG_STATE_RECEIVED			= ( ( uOSBase_t ) 2 );
#endif

static void OSTaskRecordReadyPriority(uOSBase_t uxPriority)
{
#if ( FIT_QUICK_GET_PRIORITY == 1 )
	guxTopReadyPriority |= ( 1UL << ( uxPriority ) ) ;
#endif

	/* Just to avoid compiler warnings. */
	( void ) uxPriority;
}

static void OSTaskResetReadyPriority(uOSBase_t uxPriority)
{
#if ( FIT_QUICK_GET_PRIORITY == 1 )
	if( OSListGetLength( &( gtOSReadyTaskList[ ( uxPriority ) ] ) ) == ( uOSBase_t ) 0 )
	{
		guxTopReadyPriority &= ~( 1UL << ( uxPriority ) );
	}
#endif

	/* Just to avoid compiler warnings. */
	( void ) uxPriority;
}

static uOSBase_t OSTaskFindHighestReadyPriority()
{
	uOSBase_t uxTopPriority = OSHIGHEAST_PRIORITY - 1;

#if ( FIT_QUICK_GET_PRIORITY == 1 )
	FitGET_HIGHEST_PRIORITY( uxTopPriority, guxTopReadyPriority );
#else
	/* Find the highest priority queue that contains ready tasks. */
	while( OSListIsEmpty( &( gtOSReadyTaskList[ uxTopPriority ] ) ) )
	{
		--uxTopPriority;
	}
#endif
	return uxTopPriority;
}

static void* OSTaskGetTCBFromHandle(OSTaskHandle_t pxHandle)
{
	return ( ( ( pxHandle ) == OS_NULL ) ? ( tOSTCB_t * ) gptCurrentTCB : ( tOSTCB_t * ) ( pxHandle ) );
}

static void OSTaskListsInitialise( void )
{
	uOSBase_t uxPriority;

	for( uxPriority = ( uOSBase_t ) 0U; uxPriority < ( uOSBase_t ) OSHIGHEAST_PRIORITY; uxPriority++ )
	{
		OSListInitialise( &( gtOSReadyTaskList[ uxPriority ] ) );
	}

	OSListInitialise( &gtOSTimer1TaskList );
	OSListInitialise( &gtOSTimer2TaskList );
	OSListInitialise( &gtOSPendingReadyTaskList );
	OSListInitialise( &gtOSRecycleTaskList );
	OSListInitialise( &gtOSSuspendedTaskList );

	gptOSTimerTaskList = &gtOSTimer1TaskList;
	gptOSOFTimerTaskList = &gtOSTimer2TaskList;
}

static void OSTaskResetCurrentTask()
{
	uOSBase_t uxTopPriority;

	/* Find the highest priority queue that contains ready tasks. */
	uxTopPriority = OSTaskFindHighestReadyPriority();
	OSListGetNextItemHolder( &( gtOSReadyTaskList[ uxTopPriority ] ), gptCurrentTCB );
}

static void OSTaskUpdateUnblockTime( void )
{
	tOSTCB_t *ptTCB;

	if( OSListIsEmpty( gptOSTimerTaskList ) != OS_FALSE )
	{
		guxNextTaskUnblockTime = OSPEND_FOREVER_VALUE;
	}
	else
	{
		( ptTCB ) = ( tOSTCB_t * ) OSListGetHeadItemHolder( gptOSTimerTaskList );
		guxNextTaskUnblockTime = OSListItemGetValue( &( ( ptTCB )->tTimerListItem ) );
	}
}

static void OSTaskListOfTimerSwitch()
{
	tOSList_t *ptTempList;

	ptTempList = gptOSTimerTaskList;
	gptOSTimerTaskList = gptOSOFTimerTaskList;
	gptOSOFTimerTaskList = ptTempList;
	gxNumOfOverflows++;
	OSTaskUpdateUnblockTime();
}

static void OSTaskListOfReadyAdd(tOSTCB_t* ptTCB)
{
	OSTaskRecordReadyPriority( ( ptTCB )->uxPriority );
	OSListInsertItemToEnd( &( gtOSReadyTaskList[ ( ptTCB )->uxPriority ] ), &( ( ptTCB )->tTimerListItem ) );
}

static OSTaskHandle_t OSAllocateTCBAndStack( const uOS16_t usStackDepth, uOSStack_t *puxStackBuffer )
{
	OSTaskHandle_t ptNewTCB;
	ptNewTCB = ( OSTaskHandle_t ) OSMemMalloc( sizeof( tOSTCB_t ) );
	
	/* Just to avoid compiler warnings. */
	( void ) puxStackBuffer;
	
	if( ptNewTCB != OS_NULL )
	{
		ptNewTCB->puxStartStack = ( uOSStack_t * )OSMemMalloc( ( ( uOS16_t )usStackDepth ) * sizeof( uOSStack_t ));

		if( ptNewTCB->puxStartStack == OS_NULL )
		{
			OSMemFree( ptNewTCB );
			ptNewTCB = OS_NULL;
		}
		else
		{
			memset( (void*)ptNewTCB->puxStartStack, ( uOS8_t ) 0xA1U, ( uOS32_t ) usStackDepth * sizeof( uOSStack_t ) );
		}
	}

	return ptNewTCB;
}

static void OSTaskInitTCB( tOSTCB_t * const ptTCB, const char * const pcName, uOSBase_t uxPriority, const uOS16_t usStackDepth )
{
	uOSBase_t x;

	// Store the task name in the TCB.
	for( x = ( uOSBase_t ) 0; x < ( uOSBase_t ) OSNAME_MAX_LEN; x++ )
	{
		ptTCB->pcTaskName[ x ] = pcName[ x ];
		if( pcName[ x ] == 0x00 )
		{
			break;
		}
	}
	ptTCB->pcTaskName[ OSNAME_MAX_LEN - 1 ] = '\0';

	if( uxPriority >= ( uOSBase_t ) OSHIGHEAST_PRIORITY )
	{
		uxPriority = ( uOSBase_t ) OSHIGHEAST_PRIORITY - 1;
	}

	ptTCB->uxPriority = uxPriority;

	#if ( OS_MUTEX_ON == 1 )
	{
		ptTCB->uxBasePriority = uxPriority;
		ptTCB->uxMutexHoldNum = 0;
	}
	#endif // OS_MUTEX_ON
	
	#if ( OS_TASK_SIGNAL_ON == 1 )
	{
		ptTCB->uxSigState = SIG_STATE_NOTWAITING;		/*< Task signal state: NotWaiting Waiting Received. */
		ptTCB->xSigValue = 0;							/*< Task signal value: Msg or count. */		
	}
	#endif // OS_TASK_SIGNAL_ON == 1
		
	OSListItemInitialise( &( ptTCB->tTimerListItem ) );
	OSListItemInitialise( &( ptTCB->tEventListItem ) );

	OSListItemSetHolder( &( ptTCB->tTimerListItem ), ptTCB );

	/* Event lists are always in priority order. */
	OSListItemSetValue( &( ptTCB->tEventListItem ), ( uOSTick_t ) OSHIGHEAST_PRIORITY - ( uOSTick_t ) uxPriority );
	OSListItemSetHolder( &( ptTCB->tEventListItem ), ptTCB );

	( void ) usStackDepth;
}

OSTaskHandle_t OSTaskCreate(OSTaskFunction_t	pxTaskFunction,
                            void*				pvParameter,
                            const uOS16_t 		usStackDepth,
                            uOSBase_t			uxPriority,
                            sOS8_t*				pcTaskName)
{
	sOSBase_t xStatus;
	OSTaskHandle_t ptNewTCB;
	uOSStack_t *puxTopOfStack;

	ptNewTCB = (tOSTCB_t * )OSAllocateTCBAndStack( usStackDepth, OS_NULL );

	if( ptNewTCB != OS_NULL )
	{
		#if( OSSTACK_GROWTH < 0 )
		{
			puxTopOfStack = ptNewTCB->puxStartStack + ( usStackDepth - ( uOS16_t ) 1 );
			puxTopOfStack = ( uOSStack_t * ) ( ( ( uOS32_t ) puxTopOfStack ) & ( ~( ( uOS32_t ) OSMEM_ALIGNMENT_MASK ) ) );
		}
		#else
		{
			puxTopOfStack = ptNewTCB->puxStartStack;
			ptNewTCB->puxEndOfStack = ptNewTCB->puxStartStack + ( usStackDepth - 1 );
		}
		#endif

		OSTaskInitTCB( ptNewTCB, pcTaskName, uxPriority, usStackDepth );
		ptNewTCB->puxTopOfStack = FitInitializeStack( puxTopOfStack, pxTaskFunction, pvParameter);
		
		OSIntLock();
		{
			guxCurrentNumberOfTasks++;
			if( gptCurrentTCB == OS_NULL )
			{
				gptCurrentTCB =  ptNewTCB;

				if( guxCurrentNumberOfTasks == ( uOSBase_t ) 1 )
				{
					OSTaskListsInitialise();
				}
			}
			else
			{
				if( gbSchedulerRunning == OS_FALSE )
				{
					if( gptCurrentTCB->uxPriority <= uxPriority )
					{
						gptCurrentTCB = ptNewTCB;
					}
				}
			}

			OSTaskListOfReadyAdd( ptNewTCB );

			xStatus = OS_PASS;
		}
		OSIntUnlock();
	}
	else
	{
		xStatus = OS_FAIL;
	}

	if( xStatus == OS_PASS )
	{
		if( gbSchedulerRunning != OS_FALSE )
		{
			if( gptCurrentTCB->uxPriority < uxPriority )
			{
				OSSchedule();
			}
		}
	}
	return ptNewTCB;
}

void OSTaskDelete( OSTaskHandle_t xTaskToDelete )
{
	tOSTCB_t *ptTCB;

	OSIntLock();
	{
		ptTCB = OSTaskGetTCBFromHandle( xTaskToDelete );

		if( OSListRemoveItem( &( ptTCB->tTimerListItem ) ) == ( uOSBase_t ) 0 )
		{
			OSTaskResetReadyPriority( ptTCB->uxPriority );
		}

		if( OSListItemGetList( &( ptTCB->tEventListItem ) ) != OS_NULL )
		{
			( void ) OSListRemoveItem( &( ptTCB->tEventListItem ) );
		}

		if( ptTCB == gptCurrentTCB )
		{
			OSListInsertItemToEnd( &gtOSRecycleTaskList, &( ptTCB->tTimerListItem ) );
			++guxTasksDeleted;
		}
		else
		{
			--guxCurrentNumberOfTasks;
			OSMemFree(ptTCB->puxStartStack);
			OSTaskUpdateUnblockTime();
		}		
	}
	OSIntUnlock();

	if( gbSchedulerRunning != OS_FALSE )
	{
		if( ptTCB == gptCurrentTCB )
		{
			OSSchedule();
		}
	}
}

sOSBase_t OSTaskSetID(OSTaskHandle_t const TaskHandle, sOSBase_t xID)
{
	if(TaskHandle == OS_NULL)
	{
		return 1;
	}
	OSIntLock();
	{
		TaskHandle->xID = xID;
	}
	OSIntUnlock();

	return 0;
}

sOSBase_t OSTaskGetID(OSTaskHandle_t const TaskHandle)
{
	sOSBase_t xID = 0;
	
	OSIntLock();
	if(TaskHandle != OS_NULL)
	{
		xID = TaskHandle->xID;
	}
	OSIntUnlock();

	return xID;
}

static void OSTaskListOfRecycleRemove( void )
{
	uOSBool_t bListIsEmpty;

	while( guxTasksDeleted > ( uOSBase_t ) 0U )
	{
		( void ) OSScheduleLock();
		{
			bListIsEmpty = OSListIsEmpty( &gtOSRecycleTaskList );
		}
		( void ) OSScheduleUnlock();

		if( bListIsEmpty == OS_FALSE )
		{
			tOSTCB_t *ptTCB;
			OSIntLock();
			{
				ptTCB = ( tOSTCB_t * ) OSListGetHeadItemHolder( ( &gtOSRecycleTaskList ) );
				( void ) OSListRemoveItem( &( ptTCB->tTimerListItem ) );
				--guxCurrentNumberOfTasks;
				--guxTasksDeleted;
			}
			OSIntUnlock();

			OSMemFree(ptTCB->puxStartStack);
		}
	}

}

static void OSTaskListOfTimerAdd(tOSTCB_t* ptTCB, const uOSTick_t uxTicksToWait, uOSBool_t bNeedSuspend )
{
	uOSTick_t uxTimeToWake;
	const uOSTick_t uxTickCount = guxTickCount;
	
	if(ptTCB == OS_NULL)
	{
		ptTCB = gptCurrentTCB;
	}
	
	if( OSListRemoveItem( &( ptTCB->tTimerListItem ) ) == ( uOSBase_t ) 0 )
	{
		OSTaskResetReadyPriority(ptTCB->uxPriority);
	}

	if( (uxTicksToWait==OSPEND_FOREVER_VALUE) && (bNeedSuspend==OS_TRUE) )
	{
		OSListInsertItemToEnd( &gtOSSuspendedTaskList, &( ptTCB->tTimerListItem ) );
	}
	else
	{
		uxTimeToWake = uxTickCount + uxTicksToWait;
		OSListItemSetValue( &( ptTCB->tTimerListItem ), uxTimeToWake );

		if( uxTimeToWake < uxTickCount )
		{
			OSListInsertItem( gptOSOFTimerTaskList, &( ptTCB->tTimerListItem ) );
		}
		else
		{
			OSListInsertItem( gptOSTimerTaskList, &( ptTCB->tTimerListItem ) );

			if( uxTimeToWake < guxNextTaskUnblockTime )
			{
				guxNextTaskUnblockTime = uxTimeToWake;
			}
		}
	}	
}

void OSTaskListOfEventAdd( tOSList_t * const ptEventList, const uOSTick_t uxTicksToWait )
{
	OSListInsertItem( ptEventList, &( gptCurrentTCB->tEventListItem ) );

	OSTaskListOfTimerAdd( gptCurrentTCB, uxTicksToWait, OS_TRUE );
}

uOSBool_t OSTaskListOfEventRemove( const tOSList_t * const ptEventList )
{
	tOSTCB_t *pxUnblockedTCB;
	uOSBool_t bReturn;

	pxUnblockedTCB = ( tOSTCB_t * ) OSListGetHeadItemHolder( ptEventList );

	( void ) OSListRemoveItem( &( pxUnblockedTCB->tEventListItem ) );

	if( guxSchedulerLocked == ( uOSBase_t ) OS_FALSE )
	{
		( void ) OSListRemoveItem( &( pxUnblockedTCB->tTimerListItem ) );
		OSTaskListOfReadyAdd( pxUnblockedTCB );
	}
	else
	{
		OSListInsertItemToEnd( &( gtOSPendingReadyTaskList ), &( pxUnblockedTCB->tEventListItem ) );
	}

	if( pxUnblockedTCB->uxPriority > gptCurrentTCB->uxPriority )
	{
		bReturn = OS_TRUE;
		gbNeedSchedule = OS_TRUE;
	}
	else
	{
		bReturn = OS_FALSE;
	}
	OSTaskUpdateUnblockTime();
	
	return bReturn;
}

uOSBool_t OSTaskIncrementTick( void )
{
	tOSTCB_t * ptTCB;
	uOSTick_t uxItemValue;
	uOSBool_t bNeedSchedule = OS_FALSE;

	if( guxSchedulerLocked == ( uOSBase_t ) OS_FALSE )
	{
		const uOSTick_t uxTickCount = guxTickCount + 1;
		guxTickCount = uxTickCount;

		if( uxTickCount == ( uOSTick_t ) 0U )
		{
			OSTaskListOfTimerSwitch();
		}

		if( uxTickCount >= guxNextTaskUnblockTime )
		{
			for( ;; )
			{
				if( OSListIsEmpty( gptOSTimerTaskList ) != OS_FALSE )
				{
					guxNextTaskUnblockTime = OSPEND_FOREVER_VALUE;
					break;
				}
				else
				{
					ptTCB = ( tOSTCB_t * ) OSListGetHeadItemHolder( gptOSTimerTaskList );
					uxItemValue = OSListItemGetValue( &( ptTCB->tTimerListItem ) );

					if( uxTickCount < uxItemValue )
					{
						guxNextTaskUnblockTime = uxItemValue;
						break;
					}

					( void ) OSListRemoveItem( &( ptTCB->tTimerListItem ) );

					if( OSListItemGetList( &( ptTCB->tEventListItem ) ) != OS_NULL )
					{
						( void ) OSListRemoveItem( &( ptTCB->tEventListItem ) );
					}

					OSTaskListOfReadyAdd( ptTCB );

					if( ptTCB->uxPriority >= gptCurrentTCB->uxPriority )
					{
						bNeedSchedule = OS_TRUE;
					}
				}
			}
		}
	}
	else
	{
		++guxPendedTicks;
	}

	if( gbNeedSchedule != OS_FALSE )
	{
		bNeedSchedule = OS_TRUE;
	}

	return bNeedSchedule;
}

uOSTick_t OSGetTicksCount( void )
{
	uOSTick_t uxTicks;

	uxTicks = guxTickCount;

	return uxTicks;
}

OSTaskHandle_t OSGetCurrentTaskHandle( void )
{
	OSTaskHandle_t xReturn;

	/* A critical section is not required as this is not called from
	an interrupt and the current TCB will always be the same for any
	individual execution thread. */
	xReturn = gptCurrentTCB;

	return xReturn;
}

static void OSTaskCheckStackStatus()
{
	uOSStack_t* puxStackTemp = (uOSStack_t*)gptCurrentTCB->puxTopOfStack;

	#if( OSSTACK_GROWTH < 0 )
	if( puxStackTemp <= gptCurrentTCB->puxStartStack )
	{// Task stack overflow
		for( ; ; );
	}
	#else
	if( puxStackTemp >= gptCurrentTCB->puxEndOfStack )
	{// Task stack overflow
		for( ; ; );
	}
	#endif
}

void OSTaskSwitchContext( void )
{
	if( guxSchedulerLocked != ( uOSBase_t ) OS_FALSE )
	{
		gbNeedSchedule = OS_TRUE;
	}
	else
	{
		gbNeedSchedule = OS_FALSE;

		OSTaskCheckStackStatus();
		OSTaskResetCurrentTask();
	}
}


void OSScheduleLock( void )
{
	++guxSchedulerLocked;
}

uOSBool_t OSScheduleUnlock( void )
{
	tOSTCB_t *ptTCB = OS_NULL;
	uOSBool_t bAlreadyScheduled = OS_FALSE;

	OSIntLock();
	{
		--guxSchedulerLocked;

		if( guxSchedulerLocked == ( uOSBase_t ) OS_FALSE )
		{
			if( guxCurrentNumberOfTasks > ( uOSBase_t ) 0U )
			{
				while( OSListIsEmpty( &gtOSPendingReadyTaskList ) == OS_FALSE )
				{
					ptTCB = ( tOSTCB_t * ) OSListGetHeadItemHolder( ( &gtOSPendingReadyTaskList ) );
					( void ) OSListRemoveItem( &( ptTCB->tEventListItem ) );
					( void ) OSListRemoveItem( &( ptTCB->tTimerListItem ) );
					OSTaskListOfReadyAdd( ptTCB );

					if( ptTCB->uxPriority >= gptCurrentTCB->uxPriority )
					{
						gbNeedSchedule = OS_TRUE;
					}
				}
				if( ptTCB != OS_NULL )
				{
					OSTaskUpdateUnblockTime();
				}
				if( guxPendedTicks > ( uOSBase_t ) 0U )
				{
					uOSBase_t uxPendedTicks = guxPendedTicks;
					while( uxPendedTicks > ( uOSBase_t ) 0U )
					{
						if( OSTaskIncrementTick() != OS_FALSE )
						{
							gbNeedSchedule = OS_TRUE;
						}
						--uxPendedTicks;
					}
					guxPendedTicks = 0;
				}

				if( gbNeedSchedule != OS_FALSE )
				{
					bAlreadyScheduled = OS_TRUE;
					OSSchedule();
				}
			}
		}
	}
	OSIntUnlock();

	return bAlreadyScheduled;
}

sOSBase_t OSGetScheduleState( void )
{
	sOSBase_t xReturn;

	if( gbSchedulerRunning == OS_FALSE )
	{
		xReturn = SCHEDULER_NOT_STARTED;
	}
	else
	{
		if( guxSchedulerLocked == ( uOSBase_t )OS_FALSE )
		{
			xReturn = SCHEDULER_RUNNING;
		}
		else
		{
			xReturn = SCHEDULER_LOCKED;
		}
	}

	return xReturn;
}

void OSTaskSleep( const uOSTick_t uxTicksToSleep )
{
	uOSBool_t bAlreadyScheduled = OS_FALSE;

	if( uxTicksToSleep > ( uOSTick_t ) 0U )
	{
		OSScheduleLock();
		{
			OSTaskListOfTimerAdd( gptCurrentTCB, uxTicksToSleep, OS_FALSE );
		}
		bAlreadyScheduled = OSScheduleUnlock();
	}

	if( bAlreadyScheduled == OS_FALSE )
	{
		OSSchedule();
	}
}

void OSTaskSetTimeOutState( tOSTimeOut_t * const ptTimeOut )
{
	ptTimeOut->xOverflowCount = gxNumOfOverflows;
	ptTimeOut->uxTimeOnEntering = guxTickCount;
}

uOSBool_t OSTaskGetTimeOutState( tOSTimeOut_t * const ptTimeOut, uOSTick_t * const puxTicksToWait )
{
	uOSBool_t bReturn;

	OSIntLock();
	{
		const uOSTick_t uxTickCount = guxTickCount;

		if( *puxTicksToWait == OSPEND_FOREVER_VALUE )
		{
			bReturn = OS_FALSE;
		}
		else if( ( gxNumOfOverflows != ptTimeOut->xOverflowCount ) && ( uxTickCount >= ptTimeOut->uxTimeOnEntering ) )
		{
			bReturn = OS_TRUE;
		}
		else if( ( uxTickCount - ptTimeOut->uxTimeOnEntering ) < *puxTicksToWait )
		{
			*puxTicksToWait -= ( uxTickCount -  ptTimeOut->uxTimeOnEntering );
			OSTaskSetTimeOutState( ptTimeOut );
			bReturn = OS_FALSE;
		}
		else
		{
			bReturn = OS_TRUE;
		}
	}
	OSIntUnlock();

	return bReturn;
}

void OSTaskNeedSchedule( void )
{
	gbNeedSchedule = OS_TRUE;
}

eOSTaskState_t OSTaskGetState( OSTaskHandle_t TaskHandle )
{
	eOSTaskState_t eReturn;
	tOSList_t *ptStateList;
	const tOSTCB_t * const ptTCB = ( tOSTCB_t * ) TaskHandle;

	if( ptTCB == gptCurrentTCB )
	{
		eReturn = eTaskStateRuning;
	}
	else
	{
		OSIntLock();
		{
			ptStateList = ( tOSList_t * ) OSListItemGetList( &( ptTCB->tTimerListItem ) );
		}
		OSIntUnlock();

		if( ( ptStateList == gptOSTimerTaskList ) || ( ptStateList == gptOSOFTimerTaskList ) )
		{
			eReturn = eTaskStateBlocked;
		}
		else if( ptStateList == &gtOSSuspendedTaskList )
		{
			if( OSListItemGetList( &( ptTCB->tEventListItem ) ) == OS_NULL )
			{
				eReturn = eTaskStateSuspended;
			}
			else
			{
				eReturn = eTaskStateBlocked;
			}
		}

		else if( ptStateList == &gtOSRecycleTaskList )
		{
			eReturn = eTaskStateDeath;
		}
		else
		{
			eReturn = eTaskStateReady;
		}
	}

	return eReturn;
}

uOSBase_t OSTaskGetPriority( OSTaskHandle_t TaskHandle )
{
	tOSTCB_t *ptTCB;
	uOSBase_t uxReturn;

	OSIntLock();
	{
		/* If null is passed in here then it is the priority of the that
		called uxTaskPriorityGet() that is being queried. */
		ptTCB = OSTaskGetTCBFromHandle( TaskHandle );
		uxReturn = ptTCB->uxPriority;
	}
	OSIntUnlock();

	return uxReturn;
}

uOSBase_t OSTaskGetPriorityFromISR( OSTaskHandle_t TaskHandle )
{
	tOSTCB_t *ptTCB;
	uOSBase_t uxReturn, uxSavedInterruptState;

	uxSavedInterruptState = OSIntMaskFromISR();
	{
		ptTCB = OSTaskGetTCBFromHandle( TaskHandle );
		uxReturn = ptTCB->uxPriority;
	}
	OSIntUnmaskFromISR( uxSavedInterruptState );

	return uxReturn;
}

void OSTaskSetPriority( OSTaskHandle_t TaskHandle, uOSBase_t uxNewPriority )
{
	tOSTCB_t *ptTCB;
	uOSBase_t uxCurrentBasePriority, uxPriorityUsedOnEntry;
	uOSBool_t bNeedSchedule = OS_FALSE;

	if( uxNewPriority >= ( uOSBase_t ) OSHIGHEAST_PRIORITY )
	{
		uxNewPriority = ( uOSBase_t ) OSHIGHEAST_PRIORITY - 1;
	}

	OSIntLock();
	{
		ptTCB = OSTaskGetTCBFromHandle( TaskHandle );

		#if ( OS_MUTEX_ON == 1 )
		{
			uxCurrentBasePriority = ptTCB->uxBasePriority;
		}
		#else
		{
			uxCurrentBasePriority = ptTCB->uxPriority;
		}
		#endif

		if( uxCurrentBasePriority != uxNewPriority )
		{
			if( uxNewPriority > uxCurrentBasePriority )
			{
				if( ptTCB != gptCurrentTCB )
				{
					if( uxNewPriority >= gptCurrentTCB->uxPriority )
					{
						bNeedSchedule = OS_TRUE;
					}
				}
			}
			else if( ptTCB == gptCurrentTCB )
			{
				bNeedSchedule = OS_TRUE;
			}

			uxPriorityUsedOnEntry = ptTCB->uxPriority;

			#if ( OS_MUTEX_ON == 1 )
			{
				if( ptTCB->uxBasePriority == ptTCB->uxPriority )
				{
					ptTCB->uxPriority = uxNewPriority;
				}

				ptTCB->uxBasePriority = uxNewPriority;
			}
			#else
			{
				ptTCB->uxPriority = uxNewPriority;
			}
			#endif

			OSListItemSetValue( &( ptTCB->tEventListItem ), ( ( uOSTick_t ) OSHIGHEAST_PRIORITY - ( uOSTick_t ) uxNewPriority ) );

			if( OSListContainListItem( &( gtOSReadyTaskList[ uxPriorityUsedOnEntry ] ), &( ptTCB->tTimerListItem ) ) != OS_FALSE )
			{
				if( OSListRemoveItem( &( ptTCB->tTimerListItem ) ) == ( uOSBase_t ) 0 )
				{
					OSTaskResetReadyPriority( uxPriorityUsedOnEntry );
				}

				OSTaskListOfReadyAdd( ptTCB );
			}

			if( bNeedSchedule == OS_TRUE )
			{
				OSSchedule();
			}
		}
	}
	OSIntUnlock();
}

#if ( OS_MUTEX_ON == 1 )
void *OSTaskGetMutexHolder( void )
{
	if( gptCurrentTCB != OS_NULL )
	{
		( gptCurrentTCB->uxMutexHoldNum )++;
	}
	return gptCurrentTCB;
}

void OSTaskPriorityInherit( OSTaskHandle_t const pxMutexHolder )
{
	tOSTCB_t * const ptTCB = ( tOSTCB_t * ) pxMutexHolder;

	if( pxMutexHolder != OS_NULL )
	{
		if( ptTCB->uxPriority < gptCurrentTCB->uxPriority )
		{
			OSListItemSetValue( &( ptTCB->tEventListItem ), ( uOSTick_t ) OSHIGHEAST_PRIORITY - ( uOSTick_t ) gptCurrentTCB->uxPriority );

			if( OSListContainListItem( &( gtOSReadyTaskList[ ptTCB->uxPriority ] ), &( ptTCB->tTimerListItem ) ) != OS_FALSE )
			{
				if( OSListRemoveItem( &( ptTCB->tTimerListItem ) ) == ( uOSBase_t ) 0 )
				{
					OSTaskResetReadyPriority( ptTCB->uxPriority );
				}

				ptTCB->uxPriority = gptCurrentTCB->uxPriority;
				OSTaskListOfReadyAdd( ptTCB );
			}
			else
			{
				/* Just inherit the priority. */
				ptTCB->uxPriority = gptCurrentTCB->uxPriority;
			}
		}
	}
}

uOSBool_t OSTaskPriorityDisinherit( OSTaskHandle_t const pxMutexHolder )
{
	tOSTCB_t * const ptTCB = ( tOSTCB_t * ) pxMutexHolder;
	uOSBool_t bNeedSchedule = OS_FALSE;

	if( pxMutexHolder != OS_NULL )
	{
		( ptTCB->uxMutexHoldNum )--;

		if( ptTCB->uxPriority != ptTCB->uxBasePriority )
		{
			if( ptTCB->uxMutexHoldNum == ( uOSBase_t ) 0 )
			{
				if( OSListRemoveItem( &( ptTCB->tTimerListItem ) ) == ( uOSBase_t ) 0 )
				{
					OSTaskResetReadyPriority( ptTCB->uxPriority );
				}

				ptTCB->uxPriority = ptTCB->uxBasePriority;

				OSListItemSetValue( &( ptTCB->tEventListItem ), ( uOSTick_t ) OSHIGHEAST_PRIORITY - ( uOSTick_t ) ptTCB->uxPriority );
				OSTaskListOfReadyAdd( ptTCB );

				bNeedSchedule = OS_TRUE;
			}
		}
	}

	return bNeedSchedule;
}
#endif /* ( OS_MUTEX_ON == 1 ) */


void OSTaskSuspend( OSTaskHandle_t TaskHandle )
{
	tOSTCB_t *ptTCB;
	uOSBase_t uxTasksNumTemp = guxCurrentNumberOfTasks;
	
	OSIntLock();
	{
		/* If TaskHandle is null then it is the running task that is
		being suspended. */
		ptTCB = OSTaskGetTCBFromHandle( TaskHandle );

		/* Remove task from the ready/timer list */
		if( OSListRemoveItem( &( ptTCB->tTimerListItem ) ) == ( uOSBase_t ) 0 )
		{
			OSTaskResetReadyPriority( ptTCB->uxPriority );
		}

		/* Is the task waiting on an event list */
		if( OSListItemGetList( &( ptTCB->tEventListItem ) ) != OS_NULL )
		{
			( void ) OSListRemoveItem( &( ptTCB->tEventListItem ) );
		}

		/* place the task in the suspended list. */
		OSListInsertItemToEnd( &gtOSSuspendedTaskList, &( ptTCB->tTimerListItem ) );
	}
	OSIntUnlock();

	if( gbSchedulerRunning != OS_FALSE )
	{
		/* Update the next expected unblock time */
		OSIntLock();
		{
			OSTaskUpdateUnblockTime();
		}
		OSIntUnlock();
	}

	if( ptTCB == gptCurrentTCB )
	{
		if( gbSchedulerRunning != OS_FALSE )
		{
			OSSchedule();
		}
		else
		{
			if( OSListGetLength( &gtOSSuspendedTaskList ) == uxTasksNumTemp )
			{
				gptCurrentTCB = OS_NULL;
			}
			else
			{
				OSTaskSwitchContext();
			}
		}
	}
}

static uOSBool_t OSTaskIsSuspended( const OSTaskHandle_t TaskHandle )
{
	uOSBool_t bReturn = OS_FALSE;
	const tOSTCB_t * const ptTCB = ( tOSTCB_t * ) TaskHandle;

	/* Is the task being resumed actually in the suspended list? */
	if( OSListContainListItem( &gtOSSuspendedTaskList, &( ptTCB->tTimerListItem ) ) != OS_FALSE )
	{
		/* Has the task already been resumed from within an ISR? */
		if( OSListContainListItem( &gtOSPendingReadyTaskList, &( ptTCB->tEventListItem ) ) == OS_FALSE )
		{
			/* Is it in the suspended list because it is in the	Suspended
			state, or because is is blocked with no timeout? */
			if( OSListContainListItem( OS_NULL, &( ptTCB->tEventListItem ) ) != OS_FALSE )
			{
				bReturn = OS_TRUE;
			}
		}
	}

	return bReturn;
}

void OSTaskResume( OSTaskHandle_t TaskHandle )
{
	tOSTCB_t * const ptTCB = ( tOSTCB_t * ) TaskHandle;

	if( ( ptTCB != OS_NULL ) && ( ptTCB != gptCurrentTCB ) )
	{
		OSIntLock();
		{
			if( OSTaskIsSuspended( ptTCB ) != OS_FALSE )
			{
				/* In a critical section we can access the ready lists. */
				( void ) OSListRemoveItem(  &( ptTCB->tTimerListItem ) );
				OSTaskListOfReadyAdd( ptTCB );

				if( ptTCB->uxPriority >= gptCurrentTCB->uxPriority )
				{
					OSSchedule();
				}
			}
		}
		OSIntUnlock();
	}
}


sOSBase_t OSTaskResumeFromISR( OSTaskHandle_t TaskHandle )
{
	uOSBool_t bNeedSchedule = OS_FALSE;
	tOSTCB_t * const ptTCB = ( tOSTCB_t * ) TaskHandle;
	uOSBase_t uxIntSave;

	uxIntSave = OSIntMaskFromISR();
	{
		if( OSTaskIsSuspended( ptTCB ) != OS_FALSE )
		{
			/* Check the ready lists can be accessed. */
			if( guxSchedulerLocked == ( uOSBase_t ) OS_FALSE )
			{
				/* Ready lists can be accessed so move the task from the
				suspended list to the ready list directly. */
				if( ptTCB->uxPriority >= gptCurrentTCB->uxPriority )
				{
					bNeedSchedule = OS_TRUE;
				}

				( void ) OSListRemoveItem( &( ptTCB->tTimerListItem ) );
				OSTaskListOfReadyAdd( ptTCB );
			}
			else
			{
				/* The timer or ready lists cannot be accessed so the task
				is held in the pending ready list until the scheduler is
				unsuspended. */
				OSListInsertItemToEnd( &( gtOSPendingReadyTaskList ), &( ptTCB->tEventListItem ) );
			}
		}
	}
	OSIntUnmaskFromISR( uxIntSave );

	return bNeedSchedule;
}

#if (OS_TIMER_ON==1)
void OSTaskBlockAndDelay( tOSList_t * const ptEventList, uOSTick_t uxTicksToWait, uOSBool_t bNeedSuspend )
{
	OSListInsertItemToEnd( ptEventList, &( gptCurrentTCB->tEventListItem ) );

	OSTaskListOfTimerAdd( gptCurrentTCB, uxTicksToWait, bNeedSuspend );
}
#endif /* (OS_TIMER_ON==1) */

static void OSIdleTask( void *pvParameters)
{
	int i = 0;
	
	/* Just to avoid compiler warnings. */
	( void ) pvParameters;
	
	for( ;; )
	{
		// if there is not any other task ready, then OS enter idle task;
		 i += 1;
		 
		if( OSListGetLength( &( gtOSReadyTaskList[ OSLOWEAST_PRIORITY ] ) ) > ( uOSBase_t ) 1 )
		{
			OSSchedule();
		}		 
		 
		if(guxTasksDeleted > ( uOSBase_t ) 0U)
		{
			OSTaskListOfRecycleRemove();
		}
	}
}

uOS16_t OSStart( void )
{
	uOS16_t ReturnValue = 0;
	OSTaskHandle_t TaskHandle = OS_NULL;

	TaskHandle = OSTaskCreate(OSIdleTask, OS_NULL, OSMINIMAL_STACK_SIZE, OSLOWEAST_PRIORITY, "OSIdleTask");
	if(TaskHandle != OS_NULL)
	{
#if (OS_TIMER_ON==1)
		OSTimerCreateMoniteTask();
#endif /* (OS_TIMER_ON==1) */		
		
		guxNextTaskUnblockTime = OSPEND_FOREVER_VALUE;
		gbSchedulerRunning = OS_TRUE;
		guxTickCount = ( uOSTick_t ) 0U;

		FitStartScheduler();
	}
	else
	{
		ReturnValue = 1;
	}

	// Should not get here!
	return ReturnValue;
}

#if ( OS_TASK_SIGNAL_ON == 1 )

uOSBool_t OSTaskSignalWait( uOSTick_t const uxTicksToWait)
{
	sOSBase_t xTemp;
	uOSBool_t bReturn = OS_FALSE;

	OSIntLock();
	{
		/* Only block if the signal count is not already non-zero. */
		if( gptCurrentTCB->xSigValue == 0UL )
		{
			/* Mark this task as waiting for a signal. */
			gptCurrentTCB->uxSigState = SIG_STATE_WAITING;

			if( uxTicksToWait > ( uOSTick_t ) 0 )
			{
				OSTaskListOfTimerAdd( gptCurrentTCB, uxTicksToWait, OS_TRUE );

				OSSchedule();
			}
		}
	}
	OSIntUnlock();

	OSIntLock();
	{
		xTemp = gptCurrentTCB->xSigValue;

		if( (uOS32_t)xTemp > 0UL )
		{
			gptCurrentTCB->xSigValue = xTemp - 1;
			
			bReturn = OS_TRUE;
		}

		gptCurrentTCB->uxSigState = SIG_STATE_NOTWAITING;
	}
	OSIntUnlock();

	return bReturn;
}
uOSBool_t OSTaskSignalEmit( OSTaskHandle_t const TaskHandle )
{
	tOSTCB_t * ptTCB;
	uOSBool_t bReturn = OS_FALSE;
	uOS8_t ucOldState;

	ptTCB = ( tOSTCB_t * ) TaskHandle;

	OSIntLock();
	{
		ucOldState = ptTCB->uxSigState;

		ptTCB->uxSigState = SIG_STATE_RECEIVED;
		if( ptTCB->xSigValue>0xF )
		{
			bReturn = OS_FALSE;
		}
		else
		{
			ptTCB->xSigValue += 1;
			bReturn = OS_TRUE;
		}
		/* If the task is in the blocked state specifically to wait for a
		signal then unblock it now. */
		if( ucOldState == SIG_STATE_WAITING )
		{
			( void ) OSListRemoveItem( &( ptTCB->tTimerListItem ) );
			OSTaskListOfReadyAdd( ptTCB );

			OSTaskUpdateUnblockTime();

			if( ptTCB->uxPriority > gptCurrentTCB->uxPriority )
			{
				/* The signaled task has a priority above the currently
				executing task so schedule is required. */
				OSSchedule();
			}
		}
	}
	OSIntUnlock();

	return bReturn;
}
uOSBool_t OSTaskSignalEmitFromISR( OSTaskHandle_t const TaskHandle )
{
	tOSTCB_t * ptTCB;
	uOS8_t ucOldState;
	uOSBase_t uxIntSave;
	uOSBool_t bNeedSchedule = OS_FALSE;
	uOSBool_t bReturn = OS_FALSE;

	ptTCB = ( tOSTCB_t * ) TaskHandle;

	uxIntSave = OSIntMaskFromISR();
	{
		ucOldState = ptTCB->uxSigState;
		ptTCB->uxSigState = SIG_STATE_RECEIVED;

		if( ptTCB->xSigValue>0xF )
		{
			bReturn = OS_FALSE;
		}
		else
		{
			ptTCB->xSigValue += 1;
			bReturn = OS_TRUE;
		}

		/* If the task is in the blocked state specifically to wait for a
		signal then unblock it now. */
		if( ucOldState == SIG_STATE_WAITING )
		{
			/* The task should not have been on an event list. */
			if( guxSchedulerLocked == ( uOSBase_t ) OS_FALSE )
			{
				( void ) OSListRemoveItem( &( ptTCB->tTimerListItem ) );
				OSTaskListOfReadyAdd( ptTCB );
			}
			else
			{
				/* The timer and ready lists cannot be accessed, so hold
				this task pending until the scheduler is resumed. */
				OSListInsertItemToEnd( &( gtOSPendingReadyTaskList ), &( ptTCB->tEventListItem ) );
			}

			if( ptTCB->uxPriority > gptCurrentTCB->uxPriority )
			{
				/* The signaled task has a priority above the currently
				executing task so schedule is required. */
				bNeedSchedule = OS_TRUE;
			}
		}
	}
	OSIntUnmaskFromISR( uxIntSave );	
	
	if(bNeedSchedule == OS_TRUE)
	{
		OSSchedule();
	}
	return bReturn;
}
uOSBool_t OSTaskSignalWaitMsg( sOSBase_t xSigValue, uOSTick_t const uxTicksToWait)
{
	uOSBool_t bReturn = OS_FALSE;
	( void )xSigValue;
	
	OSIntLock();
	{
		/* Only block if a signal is not already pending. */
		if( gptCurrentTCB->uxSigState != SIG_STATE_RECEIVED )
		{
			/* clear the value to zero. */
			gptCurrentTCB->xSigValue = 0;

			/* Mark this task as waiting for a signal. */
			gptCurrentTCB->uxSigState = SIG_STATE_WAITING;

			if( uxTicksToWait > ( uOSTick_t ) 0 )
			{
				OSTaskListOfTimerAdd( gptCurrentTCB, uxTicksToWait, OS_TRUE );

				/* All ports are written to allow schedule in a critical
				section (some will schedule immediately, others wait until the
				critical section exits) - but it is not something that
				application code should ever do. */
				OSSchedule();
			}
		}
	}
	OSIntUnlock();

	OSIntLock();
	{
		/* Output the current signal value. */
		xSigValue = gptCurrentTCB->xSigValue;

		/* If xSigValue is set then either the task never entered the
		blocked state (because a signal was already pending) or the
		task unblocked because of a signal.  Otherwise the task
		unblocked because of a timeout. */
		if( gptCurrentTCB->uxSigState == SIG_STATE_WAITING )
		{
			/* A signal was not received. */
			bReturn = OS_FALSE;
		}
		else
		{
			/* A signal was already pending or a signal was
			received while the task was waiting. */
			gptCurrentTCB->xSigValue = 0;
			bReturn = OS_TRUE;
		}

		gptCurrentTCB->uxSigState = SIG_STATE_NOTWAITING;
	}
	OSIntUnlock();

	return bReturn;
}
uOSBool_t OSTaskSignalEmitMsg( OSTaskHandle_t const TaskHandle, sOSBase_t const xSigValue, uOSBool_t bOverWrite )
{
	tOSTCB_t * ptTCB;
	uOSBool_t bReturn = OS_FALSE;
	uOS8_t ucOldState;

	ptTCB = ( tOSTCB_t * ) TaskHandle;

	OSIntLock();
	{
		ucOldState = ptTCB->uxSigState;

		ptTCB->uxSigState = SIG_STATE_RECEIVED;
		if( ucOldState != SIG_STATE_RECEIVED || bOverWrite == OS_TRUE )
		{
			ptTCB->xSigValue = xSigValue;
			bReturn = OS_TRUE;
		}
		else
		{
			/* The value could not be written to the task. */
			bReturn = OS_FALSE;
		}

		/* If the task is in the blocked state specifically to wait for a
		signal then unblock it now. */
		if( ucOldState == SIG_STATE_WAITING )
		{
			( void ) OSListRemoveItem( &( ptTCB->tTimerListItem ) );
			OSTaskListOfReadyAdd( ptTCB );

			OSTaskUpdateUnblockTime();

			if( ptTCB->uxPriority > gptCurrentTCB->uxPriority )
			{
				/* The signaled task has a priority above the currently
				executing task so schedule is required. */
				OSSchedule();
			}
		}
	}
	OSIntUnlock();

	return bReturn;
}
uOSBool_t OSTaskSignalEmitMsgFromISR( OSTaskHandle_t const TaskHandle, sOSBase_t const xSigValue, uOSBool_t bOverWrite )
{
	tOSTCB_t * ptTCB;
	uOS8_t ucOldState;
	uOSBool_t bReturn = OS_TRUE;
	uOSBase_t uxIntSave;
	uOSBool_t bNeedSchedule = OS_FALSE;
	
	ptTCB = ( tOSTCB_t * ) TaskHandle;

	uxIntSave = OSIntMaskFromISR();
	{
		ucOldState = ptTCB->uxSigState;
		ptTCB->uxSigState = SIG_STATE_RECEIVED;
		if( ucOldState != SIG_STATE_RECEIVED || bOverWrite == OS_TRUE )
		{
			ptTCB->xSigValue = xSigValue;
			bReturn = OS_TRUE;
		}
		else
		{
			/* The value could not be written to the task. */
			bReturn = OS_FALSE;
		}

		/* If the task is in the blocked state specifically to wait for a
		signal then unblock it now. */
		if( ucOldState == SIG_STATE_WAITING )
		{
			if( guxSchedulerLocked == ( uOSBase_t ) OS_FALSE )
			{
				( void ) OSListRemoveItem( &( ptTCB->tTimerListItem ) );
				OSTaskListOfReadyAdd( ptTCB );
			}
			else
			{
				/* The timer and ready lists cannot be accessed, so hold
				this task pending until the scheduler is resumed. */
				OSListInsertItemToEnd( &( gtOSPendingReadyTaskList ), &( ptTCB->tEventListItem ) );
			}

			if( ptTCB->uxPriority > gptCurrentTCB->uxPriority )
			{
				/* The signaled task has a priority above the currently
				executing task so schedule is required. */

				bNeedSchedule = OS_TRUE;
			}
		}
	}
	OSIntUnmaskFromISR( uxIntSave );

	if(bNeedSchedule == OS_TRUE)
	{
		OSSchedule();
	}
	return bReturn;
}
uOSBool_t OSTaskSignalClear( OSTaskHandle_t const TaskHandle )
{
	tOSTCB_t *ptTCB;
	uOSBool_t bReturn;

	/* If null is passed in here then it is the calling task that is having
	its signal state cleared. */
	ptTCB = OSTaskGetTCBFromHandle( TaskHandle );

	OSIntLock();
	{
		if( ptTCB->uxSigState == SIG_STATE_RECEIVED )
		{
			ptTCB->uxSigState = SIG_STATE_NOTWAITING;
			ptTCB->xSigValue = 0;
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
#endif

#ifdef __cplusplus
}
#endif
