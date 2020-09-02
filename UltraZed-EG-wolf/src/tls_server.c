/* tls_server.c
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


#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/ssl.h>

#include <wolftpm/tpm2.h>
#include <wolftpm/tpm2_wrap.h>

#include "tpm_io.h"
#include "tpm_test.h"
#include "tls_common.h"
#include "tls_client.h"

#include <stdio.h>

#ifdef TLS_BENCH_MODE
    double benchStart;
#endif

/*
 * Generating the Server Certificate
 *
 * Run example for ./examples/csr/csr
 * Result is: ./certs/tpm-rsa-cert.csr and ./certs/tpm-ecc-cert.csr
 *
 * Run ./certs/certreq.sh
 * Result is: ./certs/server-rsa-cert.pem and ./certs/server-ecc-cert.pem
 *
 * This example server listens on port 11111 by default.
 *
 * You can validate using the wolfSSL example client this like:
 *  ./examples/client/client -h localhost -p 11111 -g -d
 *
 * To validate server certificate use the following:
 *  ./examples/client/client -h localhost -p 11111 -g -A ./certs/tpm-ca-rsa-cert.pem
 *  or
 *  ./examples/client/client -h localhost -p 11111 -g -A ./certs/tpm-ca-ecc-cert.pem
 *
 * Or using your browser: https://localhost:11111
 *
 * With browsers you will get certificate warnings until you load the test CA's
 * ./certs/ca-rsa-cert.pem and ./certs/ca-ecc-cert.pem into your OS key store.
 * With most browsers you can bypass the certificate warning.
 */


/******************************************************************************/
/* --- BEGIN TLS SERVER Example -- */
/******************************************************************************/
int TPM2_TLS_Server(void* userCtx)
{
    int rc;
    WOLFTPM2_DEV dev;
    WOLFTPM2_KEY storageKey;
#ifndef NO_RSA
    WOLFTPM2_KEY rsaKey;
    RsaKey wolfRsaKey;
#endif
#ifdef HAVE_ECC
    WOLFTPM2_KEY eccKey;
    ecc_key wolfEccKey;
    #ifndef WOLFTPM2_USE_SW_ECDHE
    WOLFTPM2_KEY ecdhKey;
    #endif
#endif
    TPMT_PUBLIC publicTemplate;
    TpmCryptoDevCtx tpmCtx;
    SockIoCbCtx sockIoCtx;
    int tpmDevId;
    WOLFSSL_CTX* ctx = NULL;
    WOLFSSL* ssl = NULL;
#ifndef TLS_BENCH_MODE
    const char webServerMsg[] =
        "HTTP/1.1 200 OK\n\r"
        "Content-Type: text/html\n\r"
        "Connection: close\n\r"
        "\n\r"
        "<html>\n\r"
        "<head>\n\r"
        "<title>Welcome to wolfSSL!</title>\n\r"
        "</head>\n\r"
        "<body>\n\r"
        "<p>wolfSSL has successfully performed handshake!</p>\n\r"
        "</body>\n\r"
        "</html>\n\r";
#endif
    char msg[MAX_MSG_SZ];
    int msgSz = 0;
#ifdef TLS_BENCH_MODE
    int total_size;
#endif

    /* initialize variables */
    XMEMSET(&sockIoCtx, 0, sizeof(sockIoCtx));
    sockIoCtx.fd = -1;

    xil_printf("TPM2 TLS Server Example\n\r");

    /* Init the TPM2 device */
    rc = wolfTPM2_Init(&dev, TPM2_IoCb, userCtx);
    if (rc != 0) {
        wolfSSL_Cleanup();
        return rc;
    }

    /* Setup the wolf crypto device callback */
    XMEMSET(&tpmCtx, 0, sizeof(tpmCtx));
#ifndef NO_RSA
    XMEMSET(&wolfRsaKey, 0, sizeof(wolfRsaKey));
    tpmCtx.rsaKey = &rsaKey;
#endif
#ifdef HAVE_ECC
    XMEMSET(&wolfEccKey, 0, sizeof(wolfEccKey));
    tpmCtx.eccKey = &eccKey;
#endif
    tpmCtx.checkKeyCb = myTpmCheckKey; /* detects if using "dummy" key */
    tpmCtx.storageKey = &storageKey;
#ifdef WOLFTPM_USE_SYMMETRIC
    tpmCtx.useSymmetricOnTPM = 1;
#endif
    rc = wolfTPM2_SetCryptoDevCb(&dev, wolfTPM2_CryptoDevCb, &tpmCtx, &tpmDevId);
    if (rc != 0) goto exit;

    /* See if primary storage key already exists */
    rc = wolfTPM2_ReadPublicKey(&dev, &storageKey,
        TPM2_DEMO_STORAGE_KEY_HANDLE);
    if (rc != 0) {
        /* Create primary storage key */
        rc = wolfTPM2_GetKeyTemplate_RSA(&publicTemplate,
            TPMA_OBJECT_fixedTPM | TPMA_OBJECT_fixedParent |
            TPMA_OBJECT_sensitiveDataOrigin | TPMA_OBJECT_userWithAuth |
            TPMA_OBJECT_restricted | TPMA_OBJECT_decrypt | TPMA_OBJECT_noDA);
        if (rc != 0) goto exit;
        rc = wolfTPM2_CreatePrimaryKey(&dev, &storageKey, TPM_RH_OWNER,
            &publicTemplate, (byte*)gStorageKeyAuth, sizeof(gStorageKeyAuth)-1);
        if (rc != 0) goto exit;

        /* Move this key into persistent storage */
        rc = wolfTPM2_NVStoreKey(&dev, TPM_RH_OWNER, &storageKey,
            TPM2_DEMO_STORAGE_KEY_HANDLE);
        if (rc != 0) goto exit;
    }
    else {
        /* specify auth password for storage key */
        storageKey.handle.auth.size = sizeof(gStorageKeyAuth)-1;
        XMEMCPY(storageKey.handle.auth.buffer, gStorageKeyAuth,
            storageKey.handle.auth.size);
    }

#ifndef NO_RSA
    /* Create/Load RSA key for TLS authentication */
    rc = wolfTPM2_ReadPublicKey(&dev, &rsaKey, TPM2_DEMO_RSA_KEY_HANDLE);
    if (rc != 0) {
        rc = wolfTPM2_GetKeyTemplate_RSA(&publicTemplate,
            TPMA_OBJECT_sensitiveDataOrigin | TPMA_OBJECT_userWithAuth |
            TPMA_OBJECT_decrypt | TPMA_OBJECT_sign | TPMA_OBJECT_noDA);
        if (rc != 0) goto exit;
        rc = wolfTPM2_CreateAndLoadKey(&dev, &rsaKey, &storageKey.handle,
            &publicTemplate, (byte*)gKeyAuth, sizeof(gKeyAuth)-1);
        if (rc != 0) goto exit;

        /* Move this key into persistent storage */
        rc = wolfTPM2_NVStoreKey(&dev, TPM_RH_OWNER, &rsaKey,
            TPM2_DEMO_RSA_KEY_HANDLE);
        if (rc != 0) goto exit;
    }
    else {
        /* specify auth password for rsa key */
        rsaKey.handle.auth.size = sizeof(gKeyAuth)-1;
        XMEMCPY(rsaKey.handle.auth.buffer, gKeyAuth, rsaKey.handle.auth.size);
    }

    /* setup wolf RSA key with TPM deviceID, so crypto callbacks are used */
    rc = wc_InitRsaKey_ex(&wolfRsaKey, NULL, tpmDevId);
    if (rc != 0) goto exit;
    /* load public portion of key into wolf RSA Key */
    rc = wolfTPM2_RsaKey_TpmToWolf(&dev, &rsaKey, &wolfRsaKey);
    if (rc != 0) goto exit;
#endif /* !NO_RSA */

#ifdef HAVE_ECC
    /* Create/Load ECC key for TLS authentication */
    rc = wolfTPM2_ReadPublicKey(&dev, &eccKey, TPM2_DEMO_ECC_KEY_HANDLE);
    if (rc != 0) {
        rc = wolfTPM2_GetKeyTemplate_ECC(&publicTemplate,
            TPMA_OBJECT_sensitiveDataOrigin | TPMA_OBJECT_userWithAuth |
            TPMA_OBJECT_sign | TPMA_OBJECT_noDA,
            TPM_ECC_NIST_P256, TPM_ALG_ECDSA);
        if (rc != 0) goto exit;
        rc = wolfTPM2_CreateAndLoadKey(&dev, &eccKey, &storageKey.handle,
            &publicTemplate, (byte*)gKeyAuth, sizeof(gKeyAuth)-1);
        if (rc != 0) goto exit;

        /* Move this key into persistent storage */
        rc = wolfTPM2_NVStoreKey(&dev, TPM_RH_OWNER, &eccKey,
            TPM2_DEMO_ECC_KEY_HANDLE);
        if (rc != 0) goto exit;
    }
    else {
        /* specify auth password for ECC key */
        eccKey.handle.auth.size = sizeof(gKeyAuth)-1;
        XMEMCPY(eccKey.handle.auth.buffer, gKeyAuth, eccKey.handle.auth.size);
    }

    /* setup wolf ECC key with TPM deviceID, so crypto callbacks are used */
    rc = wc_ecc_init_ex(&wolfEccKey, NULL, tpmDevId);
    if (rc != 0) goto exit;
    /* load public portion of key into wolf ECC Key */
    rc = wolfTPM2_EccKey_TpmToWolf(&dev, &eccKey, &wolfEccKey);
    if (rc != 0) goto exit;

    #ifndef WOLFTPM2_USE_SW_ECDHE
    /* Ephemeral Key */
    XMEMSET(&ecdhKey, 0, sizeof(ecdhKey));
    tpmCtx.ecdhKey = &ecdhKey;
    #endif
#endif /* HAVE_ECC */


    /* Setup the WOLFSSL context (factory) */
    if ((ctx = wolfSSL_CTX_new(wolfTLSv1_2_server_method())) == NULL) {
        rc = MEMORY_E; goto exit;
    }

    /* Setup DevID */
    wolfSSL_CTX_SetDevId(ctx, tpmDevId);

    /* Setup IO Callbacks */
    wolfSSL_CTX_SetIORecv(ctx, SockIORecv);
    wolfSSL_CTX_SetIOSend(ctx, SockIOSend);

    /* Server certificate validation */
#if 0
    /* skip server cert validation for this test */
    wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_NONE, myVerify);
#else
    wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_PEER, myVerify);
#ifdef NO_FILESYSTEM
    /* example loading from buffer */
    #if 0
        if (wolfSSL_CTX_load_verify(ctx, ca.buffer, (long)ca.size,
            WOLFSSL_FILETYPE_ASN1) != WOLFSSL_SUCCESS) }
            goto exit;
        }
    #endif
#else
    /* Load CA Certificates */
    #if !defined(NO_RSA) && !defined(TLS_USE_ECC)
    if (wolfSSL_CTX_load_verify_locations(ctx, "./certs/ca-rsa-cert.pem",
        0) != WOLFSSL_SUCCESS) {
        xil_printf("Error loading ca-rsa-cert.pem cert\n\r");
        goto exit;
    }
    if (wolfSSL_CTX_load_verify_locations(ctx, "./certs/wolf-ca-rsa-cert.pem",
        0) != WOLFSSL_SUCCESS) {
        xil_printf("Error loading wolf-ca-rsa-cert.pem cert\n\r");
        goto exit;
    }
    #elif defined(HAVE_ECC)
    if (wolfSSL_CTX_load_verify_locations(ctx, "./certs/ca-ecc-cert.pem",
        0) != WOLFSSL_SUCCESS) {
        xil_printf("Error loading ca-ecc-cert.pem cert\n\r");
        goto exit;
    }
    if (wolfSSL_CTX_load_verify_locations(ctx, "./certs/wolf-ca-ecc-cert.pem",
        0) != WOLFSSL_SUCCESS) {
        xil_printf("Error loading wolf-ca-ecc-cert.pem cert\n\r");
        goto exit;
    }
    #endif
#endif /* !NO_FILESYSTEM */
#endif

#ifdef NO_FILESYSTEM
    /* example loading from buffer */
    #if 0
        if (wolfSSL_CTX_use_certificate_buffer(ctx, cert.buffer, (long)cert.size,
                                        WOLFSSL_FILETYPE_ASN1) != WOLFSSL_SUCCESS) {
            goto exit;
        }
    #endif
#else
    /* Server certificate */
#if !defined(NO_RSA) && !defined(TLS_USE_ECC)
    xil_printf("Loading RSA certificate and dummy key\n\r");

    if ((rc = wolfSSL_CTX_use_certificate_file(ctx, "./certs/server-rsa-cert.pem",
        WOLFSSL_FILETYPE_PEM)) != WOLFSSL_SUCCESS) {
        xil_printf("Error loading RSA client cert\n\r");
        goto exit;
    }

    /* Private key is on TPM and crypto dev callbacks are used */
    /* TLS server requires some dummy key loaded (workaround) */
    if (wolfSSL_CTX_use_PrivateKey_buffer(ctx, DUMMY_RSA_KEY,
            sizeof(DUMMY_RSA_KEY), WOLFSSL_FILETYPE_ASN1) != WOLFSSL_SUCCESS) {
        xil_printf("Failed to set key!\r\n");
        goto exit;
    }
#elif defined(HAVE_ECC)
    xil_printf("Loading ECC certificate and dummy key\n\r");

    if ((rc = wolfSSL_CTX_use_certificate_file(ctx, "./certs/server-ecc-cert.pem",
        WOLFSSL_FILETYPE_PEM)) != WOLFSSL_SUCCESS) {
        xil_printf("Error loading ECC client cert\n\r");
        goto exit;
    }

    /* Private key is on TPM and crypto dev callbacks are used */
    /* TLS server requires some dummy key loaded (workaround) */
    if (wolfSSL_CTX_use_PrivateKey_buffer(ctx, DUMMY_ECC_KEY,
            sizeof(DUMMY_ECC_KEY), WOLFSSL_FILETYPE_ASN1) != WOLFSSL_SUCCESS) {
        xil_printf("Failed to set key!\r\n");
        goto exit;
    }
#endif
#endif /* !NO_FILESYSTEM */

#if 0
    /* Optionally choose the cipher suite */
    rc = wolfSSL_CTX_set_cipher_list(ctx, "ECDHE-RSA-AES128-GCM-SHA256");
    if (rc != WOLFSSL_SUCCESS) {
        goto exit;
    }
#endif

    /* Create wolfSSL object/session */
    if ((ssl = wolfSSL_new(ctx)) == NULL) {
        rc = wolfSSL_get_error(ssl, 0);
        goto exit;
    }

    /* Setup socket and connection */
    rc = SetupSocketAndListen(&sockIoCtx, TLS_PORT);
    if (rc != 0) goto exit;

    /* Setup read/write callback contexts */
    wolfSSL_SetIOReadCtx(ssl, &sockIoCtx);
    wolfSSL_SetIOWriteCtx(ssl, &sockIoCtx);

    /* Accept client connections */
    rc = SocketWaitClient(&sockIoCtx);
    if (rc != 0) goto exit;

    /* perform accept */
#ifdef TLS_BENCH_MODE
    benchStart = gettime_secs(1);
#endif
    do {
        rc = wolfSSL_accept(ssl);
        if (rc != WOLFSSL_SUCCESS) {
            rc = wolfSSL_get_error(ssl, 0);
        }
    } while (rc == WOLFSSL_ERROR_WANT_READ || rc == WOLFSSL_ERROR_WANT_WRITE);
    if (rc != WOLFSSL_SUCCESS) {
        goto exit;
    }
#ifdef TLS_BENCH_MODE
    benchStart = gettime_secs(0) - benchStart;
    xil_printf("Accept: %9.3f sec (%9.3f CPS)\n\r", benchStart, 1/benchStart);
#endif

#ifdef TLS_BENCH_MODE
    rc = 0;
    total_size = 0;
    while (rc == 0 && total_size < TOTAL_MSG_SZ)
#endif
    {
        /* perform read */
    #ifdef TLS_BENCH_MODE
        benchStart = 0; /* use the read callback to trigger timing */
    #endif
        do {
            rc = wolfSSL_read(ssl, msg, sizeof(msg));
            if (rc < 0) {
                rc = wolfSSL_get_error(ssl, 0);
            }
        } while (rc == WOLFSSL_ERROR_WANT_READ);
        if (rc >= 0) {
            msgSz = rc;
        #ifdef TLS_BENCH_MODE
            benchStart = gettime_secs(0) - benchStart;
            xil_printf("Read: %d bytes in %9.3f sec (%9.3f KB/sec)\n\r",
                msgSz, benchStart, msgSz / benchStart / 1024);
            total_size += msgSz;
        #else
            /* null terminate */
            if (msgSz >= (int)sizeof(msg))
                msgSz = (int)sizeof(msg) - 1;
            msg[msgSz] = '\0';
            xil_printf("Read (%d): %s\n\r", msgSz, msg);
        #endif
            rc = 0; /* success */
        }
        if (rc != 0) goto exit;

        /* perform write */
    #ifdef TLS_BENCH_MODE
        benchStart = gettime_secs(1);
    #else
        msgSz = sizeof(webServerMsg);
        XMEMCPY(msg, webServerMsg, msgSz);
    #endif
        do {
            rc = wolfSSL_write(ssl, msg, msgSz);
            if (rc != msgSz) {
                rc = wolfSSL_get_error(ssl, 0);
            }
        } while (rc == WOLFSSL_ERROR_WANT_WRITE);
        if (rc >= 0) {
            msgSz =  rc;
        #ifdef TLS_BENCH_MODE
            benchStart = gettime_secs(0) - benchStart;
            xil_printf("Write: %d bytes in %9.3f sec (%9.3f KB/sec)\n\r",
                msgSz, benchStart, msgSz / benchStart / 1024);
        #else
            xil_printf("Write (%d): %s\n\r", msgSz, msg);
        #endif
            rc = 0; /* success */
        }
    }

exit:

    if (rc != 0) {
        xil_printf("Failure %d (0x%x): %s\n\r", rc, rc, wolfTPM2_GetRCString(rc));
    }

    wolfSSL_shutdown(ssl);

    CloseAndCleanupSocket(&sockIoCtx);
    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);

#ifndef NO_RSA
    wc_FreeRsaKey(&wolfRsaKey);
    wolfTPM2_UnloadHandle(&dev, &rsaKey.handle);
#endif
#ifdef HAVE_ECC
    wc_ecc_free(&wolfEccKey);
    wolfTPM2_UnloadHandle(&dev, &eccKey.handle);
#endif

    wolfTPM2_Cleanup(&dev);

    return rc;
}

/******************************************************************************/
/* --- END TLS Server Example -- */
/******************************************************************************/
