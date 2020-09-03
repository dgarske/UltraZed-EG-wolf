/* tls_common.h
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

#ifndef _TPM_TLS_COMMON_H_
#define _TPM_TLS_COMMON_H_

#include <wolftpm/tpm2.h>
#include <wolftpm/tpm2_wrap.h>

#include "tpm_io.h"
#include "tpm_test.h"
#include "tls_client.h"

#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/ssl.h>

#include "networking.h"

#include <stdio.h>
#include "xil_printf.h"

#ifdef __cplusplus
    extern "C" {
#endif

/* TLS Configuration */
#ifndef TLS_HOST
    #define TLS_HOST "localhost"
#endif
#ifndef TLS_PORT
    #define TLS_PORT 11111
#endif

#ifndef MAX_MSG_SZ
    #define MAX_MSG_SZ   (1 * 1024)
#endif
#ifndef TOTAL_MSG_SZ
    #define TOTAL_MSG_SZ (16 * 1024)
#endif

/* force use of a TLS cipher suite */
#if 0
    #ifndef TLS_CIPHER_SUITE
        #define TLS_CIPHER_SUITE "ECDHE-RSA-AES128-SHA256"
    #endif
#endif

/* disable mutual auth for client */
#if 0
    #define NO_TLS_MUTUAL_AUTH
#endif

/* enable for testing ECC key/cert when RSA is enabled */
#if 0
    #define TLS_USE_ECC
#endif

/* enable benchmarking mode */
#if 0
    #define TLS_BENCH_MODE
#endif

#ifdef TLS_BENCH_MODE
    extern double benchStart;
#endif


/******************************************************************************/
/* --- BEGIN Supporting TLS functions --- */
/******************************************************************************/

static inline int myVerify(int preverify, WOLFSSL_X509_STORE_CTX* store)
{
    /* Verify Callback Arguments:
     * preverify:           1=Verify Okay, 0=Failure
     * store->current_cert: Current WOLFSSL_X509 object (only with OPENSSL_EXTRA)
     * store->error_depth:  Current Index
     * store->domain:       Subject CN as string (null term)
     * store->totalCerts:   Number of certs presented by peer
     * store->certs[i]:     A `WOLFSSL_BUFFER_INFO` with plain DER for each cert
     * store->store:        WOLFSSL_X509_STORE with CA cert chain
     * store->store->cm:    WOLFSSL_CERT_MANAGER
     * store->ex_data:      The WOLFSSL object pointer
     */

    xil_printf("In verification callback, error = %d, %s\r\n",
        store->error, wolfSSL_ERR_reason_error_string(store->error));
    xil_printf("\tPeer certs: %d\r\n", store->totalCerts);
    xil_printf("\tSubject's domain name at %d is %s\r\n",
        store->error_depth, store->domain);

    (void)preverify;

    /* If error indicate we are overriding it for testing purposes */
    if (store->error != 0) {
        xil_printf("\tAllowing failed certificate check, testing only "
            "(shouldn't do this in production)\r\n");
    }

    /* A non-zero return code indicates failure override */
    return 1;
}

#if defined(WOLF_CRYPTO_DEV) || defined(WOLF_CRYPTO_CB)
/* Function checks key to see if its the "dummy" key */
static inline int myTpmCheckKey(wc_CryptoInfo* info, TpmCryptoDevCtx* ctx)
{
    int ret = 0;

#ifndef NO_RSA
    if (info && info->pk.type == WC_PK_TYPE_RSA) {
        byte    e[sizeof(word32)], e2[sizeof(word32)];
        byte    n[WOLFTPM2_WRAP_RSA_KEY_BITS/8], n2[WOLFTPM2_WRAP_RSA_KEY_BITS/8];
        word32  eSz = sizeof(e), e2Sz = sizeof(e);
        word32  nSz = sizeof(n), n2Sz = sizeof(n);
        RsaKey  rsakey;
        word32  idx = 0;

        /* export the raw public RSA portion */
        ret = wc_RsaFlattenPublicKey(info->pk.rsa.key, e, &eSz, n, &nSz);
        if (ret == 0) {
            /* load the modulus for the dummy key */
            ret = wc_InitRsaKey(&rsakey, NULL);
            if (ret == 0) {
                ret = wc_RsaPrivateKeyDecode(DUMMY_RSA_KEY, &idx, &rsakey,
                    (word32)sizeof(DUMMY_RSA_KEY));
                if (ret == 0) {
                    ret = wc_RsaFlattenPublicKey(&rsakey, e2, &e2Sz, n2, &n2Sz);
                }
                wc_FreeRsaKey(&rsakey);
            }
        }

        if (ret == 0 && XMEMCMP(n, n2, nSz) == 0) {
        #ifdef DEBUG_WOLFTPM
            xil_printf("Detected dummy key, so using TPM RSA key handle\r\n");
        #endif
            ret = 1;
        }
    }
#endif
#if defined(HAVE_ECC)
    if (info && info->pk.type == WC_PK_TYPE_ECDSA_SIGN) {
        byte    qx[WOLFTPM2_WRAP_ECC_KEY_BITS/8], qx2[WOLFTPM2_WRAP_ECC_KEY_BITS/8];
        byte    qy[WOLFTPM2_WRAP_ECC_KEY_BITS/8], qy2[WOLFTPM2_WRAP_ECC_KEY_BITS/8];
        word32  qxSz = sizeof(qx), qx2Sz = sizeof(qx2);
        word32  qySz = sizeof(qy), qy2Sz = sizeof(qy2);
        ecc_key eccKey;
        word32  idx = 0;

        /* export the raw public ECC portion */
        ret = wc_ecc_export_public_raw(info->pk.eccsign.key, qx, &qxSz, qy, &qySz);
        if (ret == 0) {
            /* load the ECC public x/y for the dummy key */
            ret = wc_ecc_init(&eccKey);
            if (ret == 0) {
                ret = wc_EccPrivateKeyDecode(DUMMY_ECC_KEY, &idx, &eccKey,
                    (word32)sizeof(DUMMY_ECC_KEY));
                if (ret == 0) {
                    ret = wc_ecc_export_public_raw(&eccKey, qx2, &qx2Sz, qy2, &qy2Sz);
                }
                wc_ecc_free(&eccKey);
            }
        }

        if (ret == 0 && XMEMCMP(qx, qx2, qxSz) == 0 &&
                        XMEMCMP(qy, qy2, qySz) == 0) {
        #ifdef DEBUG_WOLFTPM
            xil_printf("Detected dummy key, so using TPM ECC key handle\r\n");
        #endif
            ret = 1;
        }
    }
#endif
    (void)info;
    (void)ctx;

    /* non-zero return code means its a "dummy" key (not valid) and the
        provided TPM handle will be used, not the wolf public key info */
    return ret;
}
#endif /* WOLF_CRYPTO_DEV || WOLF_CRYPTO_CB */

/******************************************************************************/
/* --- END Supporting TLS functions --- */
/******************************************************************************/

#ifdef __cplusplus
    }  /* extern "C" */
#endif

#endif /* _TPM_TLS_COMMON_H_ */
