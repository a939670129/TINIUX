﻿/**********************************************************************************************************
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

#ifndef __FIT_CPU_H_
#define __FIT_CPU_H_

#include "OSType.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uOS32_t FitIntMask( void );
extern void FitIntUnmask( uOS32_t ulNewMask );
extern void FitIntLock( void );
extern void FitIntUnlock( void );
extern void FitSchedule( void );

#ifndef FIT_FORCE_INLINE
    #define FIT_FORCE_INLINE inline __attribute__(( always_inline))
#endif

FIT_FORCE_INLINE uOS32_t FitGetIPSR( void )
{
    uOS32_t ulCurrentInterrupt;

    /* Obtain the number of the currently executing interrupt. */
    __asm volatile
    ( 
    "mrs %0, ipsr" : "=r"( ulCurrentInterrupt ) 
    );
    
    return ulCurrentInterrupt;
}

/* Determine whether we are in thread mode or handler mode. */
#define FitIsInsideISR()            ( ( uOSBool_t ) ( FitGetIPSR() != ( uOSBase_t )0 ) )

#define FitNVIC_INT_CTRL_REG        ( * ( ( volatile uOS32_t * ) 0xe000ed04 ) )
#define FitNVIC_PENDSVSET_BIT       ( 1UL << 28UL )
#define FitScheduleFromISR( b )     if( b ) FitSchedule()

/* Generic helper function. */
static FIT_FORCE_INLINE uOS8_t ucFitCountLeadingZeros( uint32_t ulBitmap )
{
    uOS8_t ucReturn;

    __asm volatile ( "clz %0, %1" : "=r" ( ucReturn ) : "r" ( ulBitmap ) );
    return ucReturn;
}

#if (OSHIGHEAST_PRIORITY<=32U)
#define FITQUICK_GET_PRIORITY      ( 1U )
#define FitGET_HIGHEST_PRIORITY( uxTopPriority, guxReadyPriorities ) uxTopPriority = ( 31 - ucFitCountLeadingZeros( ( guxReadyPriorities ) ) )
#endif

#define FitIntMaskFromISR()         FitIntMask()
#define FitIntUnmaskFromISR( x )    FitIntUnmask( x )

uOSStack_t *FitInitializeStack( uOSStack_t *pxTopOfStack, OSTaskFunction_t TaskFunction, void *pvParameters );
uOSBase_t FitStartScheduler( void );

void FitPendSVHandler( void ) __attribute__ (( naked ));
void FitOSTickISR( void );
void FitSVCHandler( void ) __attribute__ (( naked ));

#if ( OS_LOWPOWER_ON!=0 )
/* Tickless idle/low power functionality. */
    #ifndef FitLowPowerIdle
        extern void FitTicklessIdle( uOSTick_t uxLowPowerTicks );
        #define FitLowPowerIdle( uxLowPowerTicks ) FitTicklessIdle( uxLowPowerTicks )
    #endif
#endif

#ifdef __cplusplus
}
#endif

#endif //__FIT_CPU_H_
