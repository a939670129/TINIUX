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

#ifndef __FIT_CPU_H_
#define __FIT_CPU_H_
#include "stdint.h"
#include "OSType.h"

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------
 * Fit specific definitions.
 *
 * The settings in this file configure AIOS correctly for the given hardware
 * and compiler.
 *
 * These settings should not be altered.
 *-----------------------------------------------------------
 */

/* Task utilities. */

/* Called at the end of an ISR that can cause a context switch. */
#define FitEND_SWITCHING_ISR( xSwitchRequired )\
{												\
extern uint32_t ulFitYieldRequired;			\
												\
	if( xSwitchRequired != 0 )			\
	{											\
		ulFitYieldRequired = 1;			\
	}											\
}

#define FitScheduleFromISR( x ) FitEND_SWITCHING_ISR( x )
#define FitSchedule() __asm volatile ( "SWI 0" );

/*-----------------------------------------------------------
 * Critical section control
 *----------------------------------------------------------*/

extern void FitIntLock( void );
extern void FitIntUnlock( void );
extern uint32_t FitSetInterruptMask( void );
extern void FitClearInterruptMask( uint32_t ulNewMaskValue );
extern void FitInstallAIOSVectorTable( void );

/* These macros do not globally disable/enable interrupts.  They do mask off
interrupts that have a priority below OSMAX_HWINT_PRI. */
#define FitIntMask()							FitSetInterruptMask()
#define FitIntUnmask( x )						FitClearInterruptMask( x )

#define FitIntMaskFromISR()						FitIntMask()
#define FitIntUnmaskFromISR( x )				FitIntUnmask( x )

/* Prototype of the AIOS tick handler.  This must be installed as the
handler for whichever peripheral is used to generate the RTOS tick. */
void FitOSTickISR( void );

/* Any task that uses the floating point unit MUST call FitTaskUsesFPU()
before any floating point instructions are executed. */
void FitTaskUsesFPU( void );
#define FitTASK_USES_FLOATING_POINT() FitTaskUsesFPU()

#define FitLOWEST_INTERRUPT_PRIORITY ( ( ( uint32_t ) configUNIQUE_INTERRUPT_PRIORITIES ) - 1UL )
#define FitLOWEST_USABLE_INTERRUPT_PRIORITY ( FitLOWEST_INTERRUPT_PRIORITY - 1UL )

#define FIT_QUICK_GET_PRIORITY		1
#define FitGET_HIGHEST_PRIORITY( uxTopPriority, uxReadyPriorities ) uxTopPriority = ( 31 - __builtin_clz( uxReadyPriorities ) )

#define FitNOP() __asm volatile( "NOP" )

/* The number of bits to shift for an interrupt priority is dependent on the
number of bits implemented by the interrupt controller. */
#if configUNIQUE_INTERRUPT_PRIORITIES == 16
	#define FitPRIORITY_SHIFT 4
	#define FitMAX_BINARY_POINT_VALUE	3
#elif configUNIQUE_INTERRUPT_PRIORITIES == 32
	#define FitPRIORITY_SHIFT 3
	#define FitMAX_BINARY_POINT_VALUE	2
#elif configUNIQUE_INTERRUPT_PRIORITIES == 64
	#define FitPRIORITY_SHIFT 2
	#define FitMAX_BINARY_POINT_VALUE	1
#elif configUNIQUE_INTERRUPT_PRIORITIES == 128
	#define FitPRIORITY_SHIFT 1
	#define FitMAX_BINARY_POINT_VALUE	0
#elif configUNIQUE_INTERRUPT_PRIORITIES == 256
	#define FitPRIORITY_SHIFT 0
	#define FitMAX_BINARY_POINT_VALUE	0
#else
	#error Invalid configUNIQUE_INTERRUPT_PRIORITIES setting.  configUNIQUE_INTERRUPT_PRIORITIES must be set to the number of unique priorities implemented by the target hardware
#endif

/* Interrupt controller access addresses. */
#define FitICCPMR_PRIORITY_MASK_OFFSET  						( 0x04 )
#define FitICCIAR_INTERRUPT_ACKNOWLEDGE_OFFSET 				( 0x0C )
#define FitICCEOIR_END_OF_INTERRUPT_OFFSET 					( 0x10 )
#define FitICCBPR_BINARY_POINT_OFFSET							( 0x08 )
#define FitICCRPR_RUNNING_PRIORITY_OFFSET						( 0x14 )

#define FitINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS 		( configINTERRUPT_CONTROLLER_BASE_ADDRESS + configINTERRUPT_CONTROLLER_CPU_INTERFACE_OFFSET )
#define FitICCPMR_PRIORITY_MASK_REGISTER 					( *( ( volatile uint32_t * ) ( FitINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + FitICCPMR_PRIORITY_MASK_OFFSET ) ) )
#define FitICCIAR_INTERRUPT_ACKNOWLEDGE_REGISTER_ADDRESS 	( FitINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + FitICCIAR_INTERRUPT_ACKNOWLEDGE_OFFSET )
#define FitICCEOIR_END_OF_INTERRUPT_REGISTER_ADDRESS 		( FitINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + FitICCEOIR_END_OF_INTERRUPT_OFFSET )
#define FitICCPMR_PRIORITY_MASK_REGISTER_ADDRESS 			( FitINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + FitICCPMR_PRIORITY_MASK_OFFSET )
#define FitICCBPR_BINARY_POINT_REGISTER 					( *( ( const volatile uint32_t * ) ( FitINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + FitICCBPR_BINARY_POINT_OFFSET ) ) )
#define FitICCRPR_RUNNING_PRIORITY_REGISTER 				( *( ( const volatile uint32_t * ) ( FitINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + FitICCRPR_RUNNING_PRIORITY_OFFSET ) ) )

uOSStack_t *FitInitializeStack( uOSStack_t *pxTopOfStack,
		OSTaskFunction_t TaskFunction, void *pvParameters );
uOSBase_t FitStartScheduler( void );

#ifdef __cplusplus
	} /* extern C */
#endif

#endif //__FIT_CPU_H_
