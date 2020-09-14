/* tpm_io.c
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

/* This example shows IO interfaces for Linux Kernel or STM32 CubeMX HAL */


#include <wolftpm/tpm2.h>
#include <wolftpm/tpm2_tis.h>
#include "tpm_io.h"

#include <stdio.h>
#include "xil_printf.h"



/******************************************************************************/
/* --- BEGIN IO Callback Logic -- */
/******************************************************************************/

/* Configuration for the SPI interface */
/* SPI Requirement: Mode 0 (CPOL=0, CPHA=0) */

/* Use the max speed by default - see tpm2_types.h for chip specific max values */
#ifndef TPM2_SPI_HZ
    #define TPM2_SPI_HZ TPM2_SPI_MAX_HZ
#endif


#include "xspips.h"
static int SpiInitDone;
static XSpiPs SpiInstance;
#ifndef TPM2_SPI_CHIPSELECT
    #define TPM2_SPI_CHIPSELECT 0 /* MIO41 - PMOD P1 (D0) */
#endif
#ifndef TPM2_SPI_DEVID
    #define TPM2_SPI_DEVID      XPAR_XSPIPS_0_DEVICE_ID
#endif

#include "xgpiops.h"
#ifndef TPM2_N_RESET_IO
    #define TPM2_N_RESET_IO      (1 << 11) /* MIO37 (Bank 1 bit 11) - PMOD P8 (D5) */
#endif

#define XSpiPs_SendByte(BaseAddress, Data) \
    XSpiPs_Out32((BaseAddress) + (u32)XSPIPS_TXD_OFFSET, (u32)(Data))
#define XSpiPs_RecvByte(BaseAddress) \
    XSpiPs_In32((u32)((BaseAddress) + (u32)XSPIPS_RXD_OFFSET))

/* Modified version of XSpiPs_PolledTransfer that allows enable and CS to 
    * be used across multiple transfers */
static s32 TPM2_IoCb_Xilinx_SPITransfer(XSpiPs *InstancePtr, u8 *SendBufPtr,
    u8 *RecvBufPtr, u32 ByteCount)
{
    u32 StatusReg;
    u32 ConfigReg;
    u32 TransCount;
    u32 CheckTransfer;
    u8 TempData;

    /* Set up buffer pointers */
    InstancePtr->SendBufferPtr = SendBufPtr;
    InstancePtr->RecvBufferPtr = RecvBufPtr;
    InstancePtr->RequestedBytes = ByteCount;
    InstancePtr->RemainingBytes = ByteCount;

    while((InstancePtr->RemainingBytes > (u32)0U) ||
            (InstancePtr->RequestedBytes > (u32)0U))
    {
        TransCount = 0U;

        /* Fill the TXFIFO with as many bytes as it will take (or as
            * many as we have to send). */
        while ((InstancePtr->RemainingBytes > (u32)0U) && 
            ((u32)TransCount < (u32)XSPIPS_FIFO_DEPTH))
        {
            XSpiPs_SendByte(InstancePtr->Config.BaseAddress, 
                *InstancePtr->SendBufferPtr);
            InstancePtr->SendBufferPtr += 1;
            InstancePtr->RemainingBytes--;
            ++TransCount;
        }

        /* If master mode and manual start mode, issue manual start
            * command to start the transfer. */
        if ((XSpiPs_IsManualStart(InstancePtr) == TRUE) && 
            (XSpiPs_IsMaster(InstancePtr) == TRUE))
        {
            ConfigReg = XSpiPs_ReadReg(InstancePtr->Config.BaseAddress, 
                XSPIPS_CR_OFFSET);
            ConfigReg |= XSPIPS_CR_MANSTRT_MASK;
            XSpiPs_WriteReg(InstancePtr->Config.BaseAddress, 
                XSPIPS_CR_OFFSET, ConfigReg);
        }

        /* Wait for the transfer to finish by polling Tx fifo status. */
        CheckTransfer = (u32)0U;
        while (CheckTransfer == 0U) {
            StatusReg = XSpiPs_ReadReg(InstancePtr->Config.BaseAddress, 
                XSPIPS_SR_OFFSET);
            if ((StatusReg & XSPIPS_IXR_MODF_MASK) != 0U) {
                /* Clear the mode fail bit */
                XSpiPs_WriteReg(InstancePtr->Config.BaseAddress, 
                    XSPIPS_SR_OFFSET, XSPIPS_IXR_MODF_MASK);
                return (s32)XST_SEND_ERROR;
            }
            CheckTransfer = (StatusReg & XSPIPS_IXR_TXOW_MASK);
        }

        /*
            * A transmit has just completed. Process received data and
            * check for more data to transmit.
            * First get the data received as a result of the transmit
            * that just completed. Receive data based on the
            * count obtained while filling tx fifo. Always get the
            * received data, but only fill the receive buffer if it
            * points to something (the upper layer software may not
            * care to receive data).
            */
        while (TransCount != (u32)0U) {
            TempData = (u8)XSpiPs_RecvByte(InstancePtr->Config.BaseAddress);
            if (InstancePtr->RecvBufferPtr != NULL) {
                *(InstancePtr->RecvBufferPtr) = TempData;
                InstancePtr->RecvBufferPtr += 1;
            }
            InstancePtr->RequestedBytes--;
            --TransCount;
        }
    }

    return (s32)XST_SUCCESS;
}

static int TPM2_IoCb_Xilinx_SPI(TPM2_CTX* ctx, const byte* txBuf,
    byte* rxBuf, word16 xferSz, void* userCtx)
{
    int ret = TPM_RC_FAILURE;
    int status;
    XSpiPs_Config *SpiConfig;
#ifdef WOLFTPM_CHECK_WAIT_STATE
    int timeout = TPM_SPI_WAIT_RETRY;
#endif

    if (!SpiInitDone) {
        uint32_t pins;
        XGpioPs_Config* gpio_cfg;
        XGpioPs gpio_inst;

        /* Initialize the SPI driver so that it's ready to use */
        SpiConfig = XSpiPs_LookupConfig(TPM2_SPI_DEVID);
        if (SpiConfig == NULL) {
            return TPM_RC_FAILURE;
        }
        status = XSpiPs_CfgInitialize(&SpiInstance, SpiConfig, 
            SpiConfig->BaseAddress);
        if (status != XST_SUCCESS) {
            return TPM_RC_FAILURE;
        }

        /* Set the SPI device as a master */
        XSpiPs_SetOptions(&SpiInstance, XSPIPS_MASTER_OPTION | 
            XSPIPS_FORCE_SSELECT_OPTION | XSPIPS_MANUAL_START_OPTION);
        /* SPI Core Clock: 200MHz / 16 = 12.5 MHz */
        XSpiPs_SetClkPrescaler(&SpiInstance, XSPIPS_CLK_PRESCALE_16);

        /* Setup the Reset line and set high */
        gpio_cfg = XGpioPs_LookupConfig(XPAR_PSU_GPIO_0_DEVICE_ID);
        XGpioPs_CfgInitialize(&gpio_inst, gpio_cfg, gpio_cfg->BaseAddr);

        /* Set the TPM's reset direction to an output. */
        XGpioPs_SetDirection(&gpio_inst, XGPIOPS_BANK1, TPM2_N_RESET_IO);

        /* Enable the output of the TPM's reset line. */
        XGpioPs_SetOutputEnable(&gpio_inst, XGPIOPS_BANK1, TPM2_N_RESET_IO);

        /* Make sure the reset line is high and preserve the other pins. */
        pins = XGpioPs_Read(&gpio_inst, XGPIOPS_BANK1);
        XGpioPs_Write(&gpio_inst, XGPIOPS_BANK1, (pins | TPM2_N_RESET_IO));

        SpiInitDone = 1;
    }

    XSpiPs_Enable(&SpiInstance);
    XSpiPs_SetSlaveSelect(&SpiInstance, TPM2_SPI_CHIPSELECT);

#ifdef WOLFTPM_CHECK_WAIT_STATE
    /* Send Header */
    status = TPM2_IoCb_Xilinx_SPITransfer(&SpiInstance, 
        (byte*)txBuf, rxBuf, TPM_TIS_HEADER_SZ);
    if (status != XST_SUCCESS) {
        XSpiPs_SetSlaveSelect(&SpiInstance, 0xF); /* deselect CS (set high) */
        XSpiPs_Disable(&SpiInstance);
        return ret;
    }

    /* Check for wait states */
    if ((rxBuf[TPM_TIS_HEADER_SZ-1] & TPM_TIS_READY_MASK) == 0) {
        do {
            /* Check for SPI ready */
            status = TPM2_IoCb_Xilinx_SPITransfer(&SpiInstance, 
                (byte*)txBuf, rxBuf, 1);
            if (status == XST_SUCCESS && rxBuf[0] & TPM_TIS_READY_MASK)
                break;
        } while (ret == TPM_RC_SUCCESS && --timeout > 0);
    #ifdef WOLFTPM_DEBUG_TIMEOUT
        xil_printf("SPI Ready Wait %d\r\n", TPM_SPI_WAIT_RETRY - timeout);
    #endif
        if (timeout <= 0) {
            XSpiPs_SetSlaveSelect(&SpiInstance, 0xF); /* deselect CS (set high) */
            XSpiPs_Disable(&SpiInstance);
            return TPM_RC_FAILURE;
        }
    }

    /* Send remainder of payload */
    status = TPM2_IoCb_Xilinx_SPITransfer(&SpiInstance,
        (byte*)&txBuf[TPM_TIS_HEADER_SZ],
        &rxBuf[TPM_TIS_HEADER_SZ],
        xferSz - TPM_TIS_HEADER_SZ);
#else
    /* Send Entire Message - no wait states */
    status = TPM2_IoCb_Xilinx_SPITransfer(&SpiInstance, 
        (byte*)txBuf, rxBuf, xferSz);
#endif /* WOLFTPM_CHECK_WAIT_STATE */
    if (status == XST_SUCCESS) {
        ret = TPM_RC_SUCCESS;
    }

    XSpiPs_SetSlaveSelect(&SpiInstance, 0xF); /* deselect CS (set high) */
    XSpiPs_Disable(&SpiInstance);

    (void)userCtx;
    (void)ctx;

    return ret;
}


static int TPM2_IoCb_SPI(TPM2_CTX* ctx, const byte* txBuf, byte* rxBuf,
    word16 xferSz, void* userCtx)
{
    return TPM2_IoCb_Xilinx_SPI(ctx, txBuf, rxBuf, xferSz, userCtx);
}


#ifdef WOLFTPM_ADV_IO
int TPM2_IoCb(TPM2_CTX* ctx, int isRead, word32 addr, byte* buf, word16 size,
    void* userCtx)
{
    int ret = TPM_RC_FAILURE;
    byte txBuf[MAX_SPI_FRAMESIZE+TPM_TIS_HEADER_SZ];
    byte rxBuf[MAX_SPI_FRAMESIZE+TPM_TIS_HEADER_SZ];

#ifdef WOLFTPM_DEBUG_IO
    xil_printf("TPM2_IoCb (Adv): Read %d, Addr %x, Size %d\r\n",
        isRead ? 1 : 0, addr, size);
    if (!isRead) {
        xil_printf("Write Size %d\r\n", size);
        TPM2_PrintBin(buf, size);
    }
#endif

    /* Build SPI format buffer */
    if (isRead) {
        txBuf[0] = TPM_TIS_READ | ((size & 0xFF) - 1);
        txBuf[1] = (addr>>16) & 0xFF;
        txBuf[2] = (addr>>8)  & 0xFF;
        txBuf[3] = (addr)     & 0xFF;
        txBuf[4] = 0x00;
        XMEMSET(&txBuf[TPM_TIS_HEADER_SZ], 0, size);
    }
    else {
        txBuf[0] = TPM_TIS_WRITE | ((size & 0xFF) - 1);
        txBuf[1] = (addr>>16) & 0xFF;
        txBuf[2] = (addr>>8)  & 0xFF;
        txBuf[3] = (addr)     & 0xFF;
        txBuf[4] = 0x00;
        XMEMCPY(&txBuf[TPM_TIS_HEADER_SZ], buf, size);
    }

    ret = TPM2_IoCb_SPI(ctx, txBuf, rxBuf, size + TPM_TIS_HEADER_SZ, userCtx);

    if (isRead) {
        XMEMCPY(buf, &rxBuf[TPM_TIS_HEADER_SZ], size);
    }

#ifdef WOLFTPM_DEBUG_IO
    if (isRead) {
        xil_printf("Read Size %d\r\n", size);
        TPM2_PrintBin(buf, size);
    }
#endif

    (void)ctx;

    return ret;
}

#else

/* IO Callback */
int TPM2_IoCb(TPM2_CTX* ctx, const byte* txBuf, byte* rxBuf,
    word16 xferSz, void* userCtx)
{
    int ret = TPM2_IoCb_SPI(ctx, txBuf, rxBuf, xferSz, userCtx);

#ifdef WOLFTPM_DEBUG_IO
    xil_printf("TPM2_IoCb: Ret %d, Sz %d\r\n", ret, xferSz);
    TPM2_PrintBin(txBuf, xferSz);
    TPM2_PrintBin(rxBuf, xferSz);
#endif

    return ret;
}

#endif /* WOLFTPM_ADV_IO */

/******************************************************************************/
/* --- END IO Callback Logic -- */
/******************************************************************************/
