/* wolf_port.c
 *
 * Copyright (C) 2006-2020 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

#include "xparameters.h"
#include "xrtcpsu.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xuartps_hw.h"

#include "wolfssl/wolfcrypt/settings.h"
#include "wolfssl/wolfcrypt/wc_port.h"
#include "wolfssl/wolfcrypt/types.h"

#include "wolftpm/tpm2_wrap.h"
#include "tpm_io.h"

#include "compile_time.h"

/* TIME CODE */
/* TODO: Implement TPM based time */
static int hw_get_time_sec(void)
{
    time_t sec = 0;
    XRtcPsu_Config* con;
    XRtcPsu         rtc;

    con = XRtcPsu_LookupConfig(XPAR_XRTCPSU_0_DEVICE_ID);
    if (con != NULL) {
        if (XRtcPsu_CfgInitialize(&rtc, con, con->BaseAddr) == XST_SUCCESS) {
            time_t compiledUTC = UNIX_TIMESTAMP;
            sec = (time_t)XRtcPsu_GetCurrentTime(&rtc);
            /* if time has not been set then advance to compiled date/time */
            if (sec < compiledUTC) {
                XRtcPsu_SetTime(&rtc, compiledUTC);
            }
        }
        else {
            xil_printf("Unable to initialize RTC\r\n");
        }
    }

    return sec;
}

/* This is used by wolfCrypt asn.c for cert time checking */
unsigned long my_time(unsigned long* timer)
{
	unsigned long sec = hw_get_time_sec();
	if (timer != NULL)
		*timer = sec;
    return sec;
}

#ifndef WOLFCRYPT_ONLY
/* This is used by TLS only */
unsigned int LowResTimer(void)
{
    return hw_get_time_sec();
}
#endif

#if defined(WOLFSSL_TLS13) && defined(HAVE_SESSION_TICKET)
word32 TimeNowInMilliseconds(void)
{
    return hw_get_time_sec() * 1000;
}
#endif

#ifndef NO_CRYPT_BENCHMARK
/* This is used by wolfCrypt benchmark tool only */
#ifndef XPAR_CPU_CORTEXA53_0_TIMESTAMP_CLK_FREQ
    #define XPAR_CPU_CORTEXA53_0_TIMESTAMP_CLK_FREQ 50000000
#endif
#ifndef COUNTS_PER_SECOND
    #define COUNTS_PER_SECOND     XPAR_CPU_CORTEXA53_0_TIMESTAMP_CLK_FREQ
#endif
double current_time(int reset)
{
    double timer;
    uint64_t cntPct = 0;
    asm volatile("mrs %0, CNTPCT_EL0" : "=r" (cntPct));

    /* Convert to milliseconds */
    timer = (double)(cntPct / (COUNTS_PER_SECOND / 1000));
    /* Convert to seconds.millisecond */
    timer /= 1000;
    return timer;
}
#endif

/* RNG Seed Function */
int my_rng_seed_gen(byte* output, word32 sz)
{
    int rc;
    WOLFTPM2_DEV dev;
    
    rc = wolfTPM2_Init(&dev, TPM2_IoCb, NULL);
    if (rc == 0) {
        rc = wolfTPM2_GetRandom(&dev, output, sz);
        wolfTPM2_Cleanup(&dev);
    }

    return rc;
}
