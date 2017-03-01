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

#ifndef __AIOS_H_
#define __AIOS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "FitType.h"
#include "OSType.h"
//#include "FitCPU.h"
#include "OSMemory.h"
#include "OSList.h"
//#include "OSTask.h"
//#include "OSMsgQ.h"
//#include "OSSem.h"
//#include "OSMutex.h"
//#include "OSTimer.h"

#define MAJOR_VERSION        0
#define MINOR_VERSION        0
#define REVISION_NUM         1

//MAJOR_VERSION.MINOR_VERSION.REVISION_NUM //version 0.0.1

#define OSIntLock()
#define OSIntLockFromISR()
#define OSIntUnlock()
#define OSIntUnlockFromISR()
#define OSIntMask()
#define OSIntUnmask()
#define OSScheduleLock()
#define OSScheduleUnlock()

#define OSSchedule()
#define OSScheduleFromISR()

#define OSStart()

// Milliseconds to OS Ticks
#define OSM2T(X) 					( ( uOSTick_t )( ( X )*( OSTICK_RATE_HZ/1000.0 ) ) )
// Frequency to OS Ticks
#define OSF2T(X) 					( ( uOSTick_t )( ( OSTICK_RATE_HZ/( X ) ) ) )

#ifdef __cplusplus
}
#endif

#endif //__AIOS_H_
