/**********************************************************************************************************
TINIUX - A tiny and efficient embedded real time operating system (RTOS)
Copyright (C) SenseRate.com All rights reserved.
http://www.tiniux.org -- Documentation, latest information, license and contact details.
http://www.tiniux.com -- Commercial support, development, porting, licensing and training services.
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
 TINIUX may be subject to applicable export control laws and regulations, which might 
 include those applicable to TINIUX of U.S. and the country in which you are located. 
 Import, export and usage of TINIUX in any manner by you shall be in compliance with such 
 applicable export control laws and regulations. 
***********************************************************************************************************/

/* Compiler includes. */
#include <intrinsics.h>
#include "TINIUX.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Constants required to manipulate the core.  Registers first... */
#define FitNVIC_SYSTICK_CTRL_REG			( * ( ( volatile uOS32_t * ) 0xe000e010 ) )
#define FitNVIC_SYSTICK_LOAD_REG			( * ( ( volatile uOS32_t * ) 0xe000e014 ) )
#define FitNVIC_SYSTICK_CURRENT_VALUE_REG	( * ( ( volatile uOS32_t * ) 0xe000e018 ) )
#define FitNVIC_SYSPRI2_REG					( * ( ( volatile uOS32_t * ) 0xe000ed20 ) )
/* ...then bits in the registers. */
#define FitNVIC_SYSTICK_CLK_BIT				( 1UL << 2UL )
#define FitNVIC_SYSTICK_INT_BIT				( 1UL << 1UL )
#define FitNVIC_SYSTICK_ENABLE_BIT			( 1UL << 0UL )
#define FitNVIC_SYSTICK_COUNT_FLAG_BIT		( 1UL << 16UL )
#define FitNVIC_PENDSVCLEAR_BIT 			( 1UL << 27UL )
#define FitNVIC_PEND_SYSTICK_CLEAR_BIT		( 1UL << 25UL )

/* Constants used to detect a Cortex-M7 r0p1 core, which should use the ARM_CM7
r0p1 Fit. */
#define FitCPUID							( * ( ( volatile uOS32_t * ) 0xE000ed00 ) )
#define FitCORTEX_M7_r0p1_ID				( 0x410FC271UL )
#define FitCORTEX_M7_r0p0_ID				( 0x410FC270UL )

#define FitNVIC_PENDSV_PRI					( ( ( uOS32_t ) OSMIN_HWINT_PRI ) << 16UL )
#define FitNVIC_SYSTICK_PRI					( ( ( uOS32_t ) OSMIN_HWINT_PRI ) << 24UL )

/* Constants required to check the validity of an interrupt priority. */
#define FitFIRST_USER_INTERRUPT_NUMBER		( 16 )
#define FitNVIC_IP_REGISTERS_OFFSET_16 		( 0xE000E3F0 )
#define FitAIRCR_REG						( * ( ( volatile uOS32_t * ) 0xE000ED0C ) )
#define FitMAX_8_BIT_VALUE					( ( uOS8_t ) 0xff )
#define FitTOP_BIT_OF_BYTE					( ( uOS8_t ) 0x80 )
#define FitMAX_PRIGROUP_BITS				( ( uOS8_t ) 7 )
#define FitPRIORITY_GROUP_MASK				( 0x07UL << 8UL )
#define FitPRIGROUP_SHIFT					( 8UL )

/* Masks off all bits but the VECTACTIVE bits in the ICSR register. */
#define FitVECTACTIVE_MASK					( 0xFFUL )

/* Constants required to manipulate the VFP. */
#define FitFPCCR							( ( volatile uOS32_t * ) 0xe000ef34 ) /* Floating point context control register. */
#define FitASPEN_AND_LSPEN_BITS				( 0x3UL << 30UL )

/* Constants required to set up the initial stack. */
#define FitINITIAL_XPSR						( 0x01000000 )
#define FitINITIAL_EXEC_RETURN				( 0xfffffffd )

/* The systick is a 24-bit counter. */
#define FitMAX_24_BIT_NUMBER				( 0xffffffUL )

#ifndef OSSYSTICK_CLOCK_HZ
	#define OSSYSTICK_CLOCK_HZ 				OSCPU_CLOCK_HZ
	/* Ensure the SysTick is clocked at the same frequency as the core. */
	#define FitNVIC_SYSTICK_CLK_BIT			( 1UL << 2UL )
#else
	/* The way the SysTick is clocked is not modified in case it is not the same
	as the core. */
	#define FitNVIC_SYSTICK_CLK_BIT			( 0 )
#endif

#if( OS_LOWPOWER_ON!=0 )
	/* The number of SysTick increments that make up one tick period.*/
	static uint32_t gulTimerCountsPerTick = 0;
	/* The maximum number of tick periods that can be suppressed is limited by the
	 * 24 bit resolution of the SysTick timer.*/
	static uint32_t guxMaxLowPowerTicks = 0;
	/* Compensate for the CPU cycles that pass while the SysTick is stopped.*/
	static uint32_t gulTimerCountsCompensation = 0;
#endif /* OS_LOWPOWER_ON */


/* A fiddle factor to estimate the number of SysTick counts that would have
occurred while the SysTick counter is stopped during tickless idle
calculations. */
#define FitMISSED_COUNTS_FACTOR				( 45UL )


/* Each task maintains its own interrupt status in the lock nesting
variable. */
static uOSBase_t guxIntLocked = 0xaaaaaaaa;


static void FitSetupTimerInterrupt( void );
extern void FitStartFirstTask( void );
extern void FitEnableVFP( void );
static void FitTaskExitError( void );

/*-----------------------------------------------------------*/

/*
 * See header file for description.
 */
uOSStack_t *FitInitializeStack( uOSStack_t *pxTopOfStack, OSTaskFunction_t TaskFunction, void *pvParameters )
{
	/* Simulate the stack frame as it would be created by a context switch
	interrupt. */

	/* Offset added to account for the way the MCU uses the stack on entry/exit
	of interrupts, and to ensure alignment. */
	pxTopOfStack--;

	*pxTopOfStack = FitINITIAL_XPSR;	/* xPSR */
	pxTopOfStack--;
	*pxTopOfStack = ( uOSStack_t ) TaskFunction;	/* PC */
	pxTopOfStack--;
	*pxTopOfStack = ( uOSStack_t ) FitTaskExitError;	/* LR */

	/* Save code space by skipping register initialisation. */
	pxTopOfStack -= 5;	/* R12, R3, R2 and R1. */
	*pxTopOfStack = ( uOSStack_t ) pvParameters;	/* R0 */

	/* A save method is being used that requires each task to maintain its
	own exec return value. */
	pxTopOfStack--;
	*pxTopOfStack = FitINITIAL_EXEC_RETURN;

	pxTopOfStack -= 8;	/* R11, R10, R9, R8, R7, R6, R5 and R4. */

	return pxTopOfStack;
}
/*-----------------------------------------------------------*/

static void FitTaskExitError( void )
{
	/* A function that implements a task must not exit or attempt to return to
	its caller as there is nothing to return to.  If a task wants to exit it
	should instead call OSTaskDelete( OS_NULL ).*/
	
	FitDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

/*
 * See header file for description.
 */
sOSBase_t FitStartScheduler( void )
{

	/* Make PendSV and SysTick the lowest priority interrupts. */
	FitNVIC_SYSPRI2_REG |= FitNVIC_PENDSV_PRI;
	FitNVIC_SYSPRI2_REG |= FitNVIC_SYSTICK_PRI;

	/* Start the timer that generates the tick ISR.  Interrupts are disabled
	here already. */
	FitSetupTimerInterrupt();

	/* Initialise the lock nesting count ready for the first task. */
	guxIntLocked = 0;

	/* Ensure the VFP is enabled - it should be anyway. */
	FitEnableVFP();

	/* Lazy save always. */
	*( FitFPCCR ) |= FitASPEN_AND_LSPEN_BITS;

	/* Start the first task. */
	FitStartFirstTask();

	/* Should not get here! */
	return 0;
}
/*-----------------------------------------------------------*/

void FitEndScheduler( void )
{
}
/*-----------------------------------------------------------*/

void FitIntLock( void )
{
	FitIntMask();
	guxIntLocked++;

}
/*-----------------------------------------------------------*/

void FitIntUnlock( void )
{
	guxIntLocked--;
	if( guxIntLocked == 0 )
	{
		FitIntUnmask( 0 );
	}
}
/*-----------------------------------------------------------*/

void FitOSTickISR( void )
{
	/* The SysTick runs at the lowest interrupt priority, so when this interrupt
	executes all interrupts must be unmasked.  There is therefore no need to
	save and then restore the interrupt mask value as its value is already
	known. */
	( void ) FitIntMaskFromISR();
	{
		/* Increment the RTOS tick. */
		if( OSTaskIncrementTick() != OS_FALSE )
		{
			/* A context switch is required.  Context switching is performed in
			the PendSV interrupt.  Pend the PendSV interrupt. */
			FitNVIC_INT_CTRL_REG = FitNVIC_PENDSVSET_BIT;
		}
	}
	FitIntUnmaskFromISR( 0 );
}

#if( OS_LOWPOWER_ON!=0 )
__weak void FitTicklessIdle( uOSTick_t uxLowPowerTicks )
{
	uint32_t ulReloadValue, ulCompleteLowPowerTicks, ulCompleteLowPowerTimeCounts;

	/* Make sure the SysTick reload value does not overflow the counter. */
	if( uxLowPowerTicks > guxMaxLowPowerTicks )
	{
		uxLowPowerTicks = guxMaxLowPowerTicks;
	}

	/* Stop the timer that is generating the tick interrupt. */
	FitNVIC_SYSTICK_CTRL_REG &= ~FitNVIC_SYSTICK_ENABLE_BIT;

	/* Calculate the reload value required to wait uxLowPowerTicks tick periods.  
	-1 is used because this code will execute part way through one of the tick periods. */
	ulReloadValue = FitNVIC_SYSTICK_CURRENT_VALUE_REG + ( gulTimerCountsPerTick * ( uxLowPowerTicks - 1UL ) );
	if( ulReloadValue > gulTimerCountsCompensation )
	{
		ulReloadValue -= gulTimerCountsCompensation;
	}

	/* Enter a critical section that will not effect interrupts bringing the MCU out of sleep mode. */
	__disable_interrupt();
	__DSB();
	__ISB();

	/* Ensure it is still ok to enter the sleep mode. */
	if( OSEnableLowPowerIdle() == OS_FALSE )
	{
		/* A task has been moved out of the Blocked state since this macro was
		executed, or a context siwth is being held pending.  Do not enter a
		sleep state.  Restart the tick and exit the critical section. */

		/* Restart from whatever is left in the count register to complete this tick period. */
		FitNVIC_SYSTICK_LOAD_REG = FitNVIC_SYSTICK_CURRENT_VALUE_REG;

		/* Restart SysTick. */
		FitNVIC_SYSTICK_CTRL_REG |= FitNVIC_SYSTICK_ENABLE_BIT;

		/* Reset the reload register to the value required for normal tick periods. */
		FitNVIC_SYSTICK_LOAD_REG = gulTimerCountsPerTick - 1UL;

		/* Exit the critical section, Re-enable interrupts*/
		__enable_interrupt();
	}
	else
	{
		/* Configure an interrupt to bring the microcontroller out of its low
		power state at the time the kernel next needs to execute.  */
		/* The interrupt must be generated from a source that remains operational
		when the microcontroller is in a low power state. */
		
		/* Set the new reload value. */
		FitNVIC_SYSTICK_LOAD_REG = ulReloadValue;

		/* Clear the SysTick count flag and set the count value back to zero. */
		FitNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

		/* Restart SysTick. */
		FitNVIC_SYSTICK_CTRL_REG |= FitNVIC_SYSTICK_ENABLE_BIT;

		/* Enter the low power state, sleep until something happens. */
		{
			__DSB();
			__WFI();
			__ISB();
		}

		/* Re-enable interrupts to allow the interrupt that brought the MCU
		out of sleep mode to execute immediately.*/
		__enable_interrupt();
		__DSB();
		__ISB();

		/* Disable interrupts again because the clock is about to be stopped
		and interrupts that execute while the clock is stopped will increase
		any slippage between the time maintained by the RTOS and calendar time. */
		__disable_interrupt();
		__DSB();
		__ISB();
		
		/* Disable the SysTick clock without reading the FitNVIC_SYSTICK_CTRL_REG register to ensure the
		FitNVIC_SYSTICK_COUNT_FLAG_BIT is not cleared if it is set. */
		FitNVIC_SYSTICK_CTRL_REG = ( FitNVIC_SYSTICK_CLK_BIT | FitNVIC_SYSTICK_INT_BIT );

		/* Determine how long the microcontroller was actually in a low power state for, which will be less than uxLowPowerTicks 
		if the microcontroller was brought out of low power mode by an interrupt.*/
		/* Note that the scheduler is suspended. Therefore no other tasks will execute until this function completes. */		
		if( ( FitNVIC_SYSTICK_CTRL_REG & FitNVIC_SYSTICK_COUNT_FLAG_BIT ) != 0 )
		{
			/*The SysTick clock has already counted to zero and been set back to the current reload value*/
			
			uint32_t ulCalculatedLoadValue;

			/*Reset the FitNVIC_SYSTICK_LOAD_REG with whatever remains of this tick period. */
			ulCalculatedLoadValue = ( gulTimerCountsPerTick - 1UL ) - ( ulReloadValue - FitNVIC_SYSTICK_CURRENT_VALUE_REG );

			/* Don't allow a tiny value, or values that have somehow underflowed. */
			if( ( ulCalculatedLoadValue < gulTimerCountsCompensation ) || ( ulCalculatedLoadValue > gulTimerCountsPerTick ) )
			{
				ulCalculatedLoadValue = ( gulTimerCountsPerTick - 1UL );
			}

			FitNVIC_SYSTICK_LOAD_REG = ulCalculatedLoadValue;

			/* As the pending tick will be processed as soon as this function exits, 
			the tick value maintained by the tick is stepped forward by one less than the time spent waiting. */
			ulCompleteLowPowerTicks = uxLowPowerTicks - 1UL;
		}
		else
		{
			/* Something other than the tick interrupt ended the sleep.*/
			/* Work out how long the sleep lasted rounded to complete tick periods. */
			ulCompleteLowPowerTimeCounts = ( uxLowPowerTicks * gulTimerCountsPerTick ) - FitNVIC_SYSTICK_CURRENT_VALUE_REG;
			ulCompleteLowPowerTicks = ulCompleteLowPowerTimeCounts / gulTimerCountsPerTick;

			/* The reload value is set to whatever fraction of a single tick period remains. */
			FitNVIC_SYSTICK_LOAD_REG = ( ( ulCompleteLowPowerTicks + 1UL ) * gulTimerCountsPerTick ) - ulCompleteLowPowerTimeCounts;
		}

		/* Correct the kernels tick count to account for the time the microcontroller spent in its low power state. */
		OSFixTickCount( ulCompleteLowPowerTicks );
		
		/* Restart the timer that is generating the tick interrupt. */
		FitNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;
		FitNVIC_SYSTICK_CTRL_REG |= FitNVIC_SYSTICK_ENABLE_BIT;
		
		/* Reset the reload register to the value required for normal tick periods. */
		FitNVIC_SYSTICK_LOAD_REG = gulTimerCountsPerTick - 1UL;

		/* Exit the critical section, Re-enable interrupts*/
		__enable_interrupt();
	}
}
#endif /* OS_LOWPOWER_ON */

/*
 * Setup the systick timer to generate the tick interrupts at the required
 * frequency.
 */
__weak void FitSetupTimerInterrupt( void )
{
	/* Calculate the constants required to configure the tick interrupt. */
	#if( OS_LOWPOWER_ON!=0 )
	{
		gulTimerCountsPerTick = ( OSSYSTICK_CLOCK_HZ / OSTICK_RATE_HZ );
		guxMaxLowPowerTicks = FitMAX_24_BIT_NUMBER / gulTimerCountsPerTick;
		gulTimerCountsCompensation = FitMISSED_COUNTS_FACTOR / ( OSCPU_CLOCK_HZ / OSSYSTICK_CLOCK_HZ );
	}
	#endif /* OS_LOWPOWER_ON */

	/* Stop and clear the SysTick. */
	FitNVIC_SYSTICK_CTRL_REG = 0UL;
	FitNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

	/* Configure SysTick to interrupt at the requested rate. */
	FitNVIC_SYSTICK_LOAD_REG = ( OSSYSTICK_CLOCK_HZ / OSTICK_RATE_HZ ) - 1UL;
	FitNVIC_SYSTICK_CTRL_REG = ( FitNVIC_SYSTICK_CLK_BIT | FitNVIC_SYSTICK_INT_BIT | FitNVIC_SYSTICK_ENABLE_BIT );
}
/*-----------------------------------------------------------*/


#ifdef __cplusplus
}
#endif
