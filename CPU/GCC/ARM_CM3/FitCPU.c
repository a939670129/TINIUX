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

/* Compiler includes. */
#include <stdint.h>

#include "AIOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FitKERNEL_INTERRUPT_PRIORITY 255

/* Constants required to manipulate the core.  Registers first... */
#define FitNVIC_SYSTICK_CTRL_REG			( * ( ( volatile uOS32_t * ) 0xe000e010 ) )
#define FitNVIC_SYSTICK_LOAD_REG			( * ( ( volatile uOS32_t * ) 0xe000e014 ) )
#define FitNVIC_SYSTICK_CURRENT_VALUE_REG	( * ( ( volatile uOS32_t * ) 0xe000e018 ) )
#define FitNVIC_SYSPRI2_REG					( * ( ( volatile uOS32_t * ) 0xe000ed20 ) )
/* ...then bits in the registers. */
#define FitNVIC_SYSTICK_CLK_BIT	        	( 1UL << 2UL )
#define FitNVIC_SYSTICK_INT_BIT				( 1UL << 1UL )
#define FitNVIC_SYSTICK_ENABLE_BIT			( 1UL << 0UL )
#define FitNVIC_SYSTICK_COUNT_FLAG_BIT		( 1UL << 16UL )
#define FitNVIC_PENDSVCLEAR_BIT 			( 1UL << 27UL )
#define FitNVIC_PEND_SYSTICK_CLEAR_BIT		( 1UL << 25UL )

/* Masks off all bits but the VECTACTIVE bits in the ICOS register. */
#define FitVECTACTIVE_MASK					( 0x1FUL )

#define FitNVIC_PENDSV_PRI					( ( ( uOS32_t ) FitKERNEL_INTERRUPT_PRIORITY ) << 16UL )
#define FitNVIC_SYSTICK_PRI					( ( ( uOS32_t ) FitKERNEL_INTERRUPT_PRIORITY ) << 24UL )

/* Constants required to set up the initial stack. */
#define FitINITIAL_XPSR						( 0x01000000 )
#define FitINITIAL_EXEC_RETURN				( 0xfffffffd )

/* Scheduler utilities. */
#define FitNVIC_INT_CTRL_REG				( * ( ( volatile uOS32_t * ) 0xe000ed04 ) )
#define FitNVIC_PENDSVSET_BIT				( 1UL << 28UL )

/* Constants required to handle critical sections. */
#define FitNO_CRITICAL_NESTING		( ( unsigned long ) 0 )
static volatile uOSBase_t uxCriticalNesting = 9999UL;

/*
 * Setup the timer to generate the tick interrupts.
 */
void FitSetupTimerInterrupt( void );

/*
 * Start first task is a separate function so it can be tested in isolation.
 */
static void FitStartFirstTask( void ) __attribute__ (( naked ));

/*
 * Used to catch tasks that attempt to return from their implementing function.
 */
static void FitTaskExitError( void );



uOSStack_t *FitInitializeStack( uOSStack_t *pxTopOfStack,
		OSTaskFunction_t TaskFunction, void *pvParameters )
{
	/* Simulate the stack frame as it would be created by a context switch
	interrupt. */
	pxTopOfStack--; /* Offset added to account for the way the MCU uses the stack on entry/exit of interrupts. */
	*pxTopOfStack = FitINITIAL_XPSR;	/* xPSR */
	pxTopOfStack--;
	*pxTopOfStack = ( uOSStack_t ) TaskFunction;	/* PC */
	pxTopOfStack--;
	*pxTopOfStack = ( uOSStack_t ) FitTaskExitError;	/* LR */

	pxTopOfStack -= 5;	/* R12, R3, R2 and R1. */
	*pxTopOfStack = ( uOSStack_t ) pvParameters;	/* R0 */
	pxTopOfStack -= 8;	/* R11, R10, R9, R8, R7, R6, R5 and R4. */

	return pxTopOfStack;	
}

static void FitTaskExitError( void )
{
	/* A function that implements a task must not exit or attempt to return to
	its caller as there is nothing to return to.  If a task wants to exit it
	should instead call OSTaskDelete( OS_NULL ). */
	
	FitDISABLE_INTERRUPTS();
	for( ;; );
}

void FitSVCHandler( void )
{
	__asm volatile (
					"	ldr	r3, ptCurrentTCBTemp1		\n" /* Restore the context. */
					"	ldr r1, [r3]					\n" /* Use ptCurrentTCBTemp1 to get the gptCurrentTCB address. */
					"	ldr r0, [r1]					\n" /* The first item in gptCurrentTCB is the task top of stack. */
					"	ldmia r0!, {r4-r11}				\n" /* Pop the registers that are not automatically saved on exception entry and the critical nesting count. */
					"	msr psp, r0						\n" /* Restore the task stack pointer. */
					"	isb								\n"
					"	mov r0, #0 						\n"
					"	msr	basepri, r0					\n"
					"	orr r14, #0xd					\n"
					"	bx r14							\n"
					"									\n"
					"	.align 2						\n"
					"ptCurrentTCBTemp1: .word gptCurrentTCB				\n"
				);
}

static void FitStartFirstTask( void )
{
	__asm volatile(
					" ldr r0, =0xE000ED08 	\n" /* Use the NVIC offset register to locate the stack. */
					" ldr r0, [r0] 			\n"
					" ldr r0, [r0] 			\n"
					" msr msp, r0			\n" /* Set the msp back to the start of the stack. */
					" cpsie i				\n" /* Globally enable interrupts. */
					" cpsie f				\n"
					" dsb					\n"
					" isb					\n"
					" svc 0					\n" /* System call to start first task. */
					" nop					\n"
				);
}

uOSBase_t FitStartScheduler( void )
{
	/* Make PendSV and SysTick the lowest priority interrupts. */
	FitNVIC_SYSPRI2_REG |= FitNVIC_PENDSV_PRI;
	FitNVIC_SYSPRI2_REG |= FitNVIC_SYSTICK_PRI;

	/* Start the timer that generates the tick ISR.  Interrupts are disabled
	here already. */
	FitSetupTimerInterrupt();
        
	/* Initialise the critical nesting count ready for the first task. */
	uxCriticalNesting = 0;


	/* Start the first task. */
	FitStartFirstTask();

	/* Should not get here! */
	FitTaskExitError();

	return 0;	
}

void FitEndScheduler( void )
{

}

/*
 * Called by OSSchedule() to manually force a context switch.
 *
 * When a context switch is performed from the task level the saved task
 * context is made to look as if it occurred from within the tick ISR.  This
 * way the same restore context function can be used when restoring the context
 * saved from the ISR or that saved from a call to FitSchedule.
 */
void FitSchedule( void )
{																				
	/* Set a PendSV to request a context switch. */								
	FitNVIC_INT_CTRL_REG = FitNVIC_PENDSVSET_BIT;								
	__asm volatile( "dsb" );
	__asm volatile( "isb" );
}


void FitIntLock( void )
{
	FitDISABLE_INTERRUPTS();
	uxCriticalNesting++;

	__asm volatile( "dsb" );
	__asm volatile( "isb" );
}

void FitIntUnlock( void )
{
	uxCriticalNesting--;
	if( uxCriticalNesting == 0 )
	{
		FitENABLE_INTERRUPTS();
	}	
}


void FitPendSVHandler( void )
{
	/* This is a naked function. */

	__asm volatile
	(
	"	mrs r0, psp							\n"
	"	isb									\n"
	"										\n"
	"	ldr	r3, ptCurrentTCBTemp2			\n" /* Get the location of the current TCB. */
	"	ldr	r2, [r3]						\n"
	"										\n"
	"	stmdb r0!, {r4-r11}					\n" /* Save the remaining registers. */
	"	str r0, [r2]						\n" /* Save the new top of stack into the first member of the TCB. */
	"										\n"
	"	stmdb sp!, {r3, r14}				\n"
	"	mov r0, %0							\n"
	"	msr basepri, r0						\n"
	"	bl OSTaskSwitchContext				\n"
	"	mov r0, #0							\n"
	"	msr basepri, r0						\n"
	"	ldmia sp!, {r3, r14}				\n"
	"										\n"	/* Restore the context, including the critical nesting count. */
	"	ldr r1, [r3]						\n"
	"	ldr r0, [r1]						\n" /* The first item in gptCurrentTCB is the task top of stack. */
	"	ldmia r0!, {r4-r11}					\n" /* Pop the registers. */
	"	msr psp, r0							\n"
	"	isb									\n"
	"	bx r14								\n"
	"										\n"
	"	.align 2							\n"
	"ptCurrentTCBTemp2: .word gptCurrentTCB	\n"
	::"i"(SETOS_MAX_SYSCALL_INTERRUPT_PRIORITY)
	);
}


void FitOSTickISR()
{
	// Increment the RTOS tick count, then look for the highest priority
	// task that is ready to run. 
	( void ) FitSET_INTERRUPT_MASK_FROM_ISR();
	{
		/* Increment the RTOS tick. */
		if( OSTaskIncrementTick() != OS_FALSE )
		{
			/* A context switch is required.  Context switching is performed in
			the PendSV interrupt.  Pend the PendSV interrupt. */
			FitNVIC_INT_CTRL_REG = FitNVIC_PENDSVSET_BIT;
		}
	}
	FitCLEAR_INTERRUPT_MASK_FROM_ISR( 0 );
}

__attribute__(( weak )) void FitSetupTimerInterrupt( void )
{
	/* Configure SysTick to interrupt at the requested rate. */
	FitNVIC_SYSTICK_LOAD_REG = ( OSCPU_CLOCK_HZ / OSTICK_RATE_HZ ) - 1UL;
	FitNVIC_SYSTICK_CTRL_REG = ( FitNVIC_SYSTICK_CLK_BIT | FitNVIC_SYSTICK_INT_BIT | FitNVIC_SYSTICK_ENABLE_BIT );
}

__attribute__(( naked )) uOS32_t FiIntMask( void )
{
	__asm volatile														\
	(																	\
		"	mrs r0, basepri											\n" \
		"	mov r1, %0												\n"	\
		"	msr basepri, r1											\n" \
		"	bx lr													\n" \
		:: "i" ( SETOS_MAX_SYSCALL_INTERRUPT_PRIORITY ) : "r0", "r1"	\
	);

	/* This return will not be reached but is necessary to prevent compiler
	warnings. */
	return 0;
}
/*-----------------------------------------------------------*/

__attribute__(( naked )) void FiIntUnmask( uOS32_t ulNewMask )
{
	__asm volatile													\
	(																\
		"	msr basepri, r0										\n"	\
		"	bx lr												\n" \
		:::"r0"														\
	);

	/* Just to avoid compiler warnings. */
	( void ) ulNewMask;
}
#ifdef __cplusplus
}
#endif
