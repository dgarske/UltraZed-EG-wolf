/* tpm_timeset.c
 *
 * Copyright (C) 2006-2020 wolfSSL Inc.
 *
 * This file is part of wolfTPM.
 *
 * wolfTPM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfTPM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

/* This example shows how to increment the TPM2 clock */

#include <wolftpm/tpm2_wrap.h>

#include "tpm_io.h"
#include "tpm_test.h"
#include "tpm_timeset.h"

#include <stdio.h>
#include <stdlib.h>
#include "xil_printf.h"

extern WOLFTPM2_DEV dev;

/******************************************************************************/
/* --- BEGIN TPM Clock Set Example -- */
/******************************************************************************/

int TPM2_ClockGet_Example(void* userCtx, UINT64* clockOut)
{
    int rc = 0;

    union {
        ReadClock_Out readClock;
        byte maxOutput[MAX_RESPONSE_SIZE];
    } cmdOut;

    TPMS_AUTH_COMMAND session[MAX_SESSION_NUM];

    xil_printf("TPM2 Demo of setting the TPM clock forward\r\n");
    rc = wolfTPM2_Init(&dev, TPM2_IoCb, userCtx);
    if (rc != TPM_RC_SUCCESS) {
        xil_printf("wolfTPM2_Init failed 0x%x: %s\r\n", rc, TPM2_GetRCString(rc));
        goto exit;
    }
    xil_printf("wolfTPM2_Init: success\r\n");


    /* Define the default session auth that has NULL password */
    XMEMSET(session, 0, sizeof(session));
    session[0].sessionHandle = TPM_RS_PW;
    session[0].auth.size = 0; /* NULL Password */
    TPM2_SetSessionAuth(session);


    /* ReadClock the current TPM uptime */
    XMEMSET(&cmdOut.readClock, 0, sizeof(cmdOut.readClock));
    rc = TPM2_ReadClock(&cmdOut.readClock);
    if (rc != TPM_RC_SUCCESS) {
        xil_printf("TPM2_ReadClock failed 0x%x: %s\r\n", rc,
            TPM2_GetRCString(rc));
        goto exit;
    }
    xil_printf("TPM2_ReadClock: success\r\n");
#ifdef DEBUG_WOLFTPM
    xil_printf("TPM2_ReadClock: (uptime) time=%lu\r\n",
            (long unsigned int)cmdOut.readClock.currentTime.time);
    xil_printf("TPM2_ReadClock: (total)  clock=%lu\r\n",
            (long unsigned int)cmdOut.readClock.currentTime.clockInfo.clock);
#endif
    if (clockOut)
        *clockOut = cmdOut.readClock.currentTime.clockInfo.clock;

exit:

    if (rc != 0) {
        xil_printf("Failure 0x%x: %s\r\n", rc, wolfTPM2_GetRCString(rc));
    }

    wolfTPM2_Cleanup(&dev);

    return rc;
}

int TPM2_ClockSet_Example(void* userCtx, UINT64* newClock)
{
    int rc = 0;

    union {
        ClockSet_In clockSet;
        byte maxInput[MAX_COMMAND_SIZE];
    } cmdIn;
    union {
        ReadClock_Out readClock;
        byte maxOutput[MAX_RESPONSE_SIZE];
    } cmdOut;

    TPMS_AUTH_COMMAND session[MAX_SESSION_NUM];

    UINT64 oldClock;

    xil_printf("TPM2 Demo of setting the TPM clock forward\r\n");
    rc = wolfTPM2_Init(&dev, TPM2_IoCb, userCtx);
    if (rc != TPM_RC_SUCCESS) {
        xil_printf("wolfTPM2_Init failed 0x%x: %s\r\n", rc, TPM2_GetRCString(rc));
        goto exit;
    }
    xil_printf("wolfTPM2_Init: success\r\n");


    /* Define the default session auth that has NULL password */
    XMEMSET(session, 0, sizeof(session));
    session[0].sessionHandle = TPM_RS_PW;
    session[0].auth.size = 0; /* NULL Password */
    TPM2_SetSessionAuth(session);


    /* ReadClock the current TPM uptime */
    XMEMSET(&cmdOut.readClock, 0, sizeof(cmdOut.readClock));
    rc = TPM2_ReadClock(&cmdOut.readClock);
    if (rc != TPM_RC_SUCCESS) {
        xil_printf("TPM2_ReadClock failed 0x%x: %s\r\n", rc,
            TPM2_GetRCString(rc));
        goto exit;
    }
    xil_printf("TPM2_ReadClock: success\r\n");
#ifdef DEBUG_WOLFTPM
    xil_printf("TPM2_ReadClock: (uptime) time=%lu\r\n",
            (long unsigned int)cmdOut.readClock.currentTime.time);
    xil_printf("TPM2_ReadClock: (total)  clock=%lu\r\n",
            (long unsigned int)cmdOut.readClock.currentTime.clockInfo.clock);
#endif
    oldClock = cmdOut.readClock.currentTime.clockInfo.clock;

    /* Set the TPM clock forward */
    cmdIn.clockSet.auth = TPM_RH_OWNER;
    if (newClock)
        cmdIn.clockSet.newTime = *newClock;
    else
        cmdIn.clockSet.newTime = oldClock + 50000;
    rc = TPM2_ClockSet(&cmdIn.clockSet);
    if (rc != TPM_RC_SUCCESS) {
        xil_printf("TPM2_clockSet failed 0x%x: %s\r\n", rc,
            TPM2_GetRCString(rc));
        goto exit;
    }
    xil_printf("TPM2_ClockSet: success\r\n");

    /* ReadClock to check the new clock time is set */
    XMEMSET(&cmdOut.readClock, 0, sizeof(cmdOut.readClock));
    rc = TPM2_ReadClock(&cmdOut.readClock);
    if (rc != TPM_RC_SUCCESS) {
        xil_printf("TPM2_ReadClock failed 0x%x: %s\r\n", rc,
            TPM2_GetRCString(rc));
        goto exit;
    }
    xil_printf("TPM2_ReadClock: success\r\n");
#ifdef DEBUG_WOLFTPM
    xil_printf("TPM2_ReadClock: (uptime) time=%lu\r\n",
            (long unsigned int)cmdOut.readClock.currentTime.time);
    xil_printf("TPM2_ReadClock: (total)  clock=%lu\r\n",
            (long unsigned int)cmdOut.readClock.currentTime.clockInfo.clock);
#endif
    *newClock = cmdOut.readClock.currentTime.clockInfo.clock;

    xil_printf("\n\t oldClock=%lu \n\t newClock=%lu \n\r\n", 
        (long unsigned int)oldClock, (long unsigned int)*newClock);

exit:

    if (rc != 0) {
        xil_printf("Failure 0x%x: %s\r\n", rc, wolfTPM2_GetRCString(rc));
    }

    wolfTPM2_Cleanup(&dev);

    return rc;
}

/******************************************************************************/
/* --- END TPM Clock Set Example -- */
/******************************************************************************/
