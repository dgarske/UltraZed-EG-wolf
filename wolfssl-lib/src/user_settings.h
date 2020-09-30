/* user_settings.h
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

/*
 * user_settings.h
 *
 *  Created on: Mar 20, 2020
 *  Generated using: 
 * ./configure --enable-armasm --enable-ecc --enable-aesgcm --enable-pwdbased --enable-sp --enable-sp-asm \
 *     --disable-dh --disable-sha --disable-md5 --disable-sha224 --disable-aescbc --disable-shake256 
 *  Result: wolfssl/options.h
 */

#ifndef SRC_USER_SETTINGS_H_
#define SRC_USER_SETTINGS_H_


/* Xilinx SDK */
#define WOLFSSL_XILINX
#define FREERTOS
#define WOLFSSL_LWIP
#define NO_FILESYSTEM
#define NO_STDIO_FILESYSTEM

/* Features */
#define HAVE_TLS_EXTENSIONS
#define HAVE_SUPPORTED_CURVES
#define HAVE_EXTENDED_MASTER
#define HAVE_ENCRYPT_THEN_MAC
#define NO_OLD_TLS
#define WOLFSSL_TLS13
#define WOLFSSL_CERT_GEN
#define WOLFSSL_CERT_REQ
#define WOLFSSL_CERT_EXT
#define WOLF_CRYPTO_CB


/* Platform - remap printf */
#include <stdio.h>
#include "xil_printf.h"
#define XPRINTF xil_printf

/* Enable ARMv8 (Aarch64) assembly speedups - SHA256 / AESGCM */
/* Note: Requires CFLAGS="-mcpu=generic+crypto -mstrict-align" */
//#define WOLFSSL_ARMASM
//#define WOLFSSL_XILINX_CRYPT /* Xilinx hardware acceleration */

/* TPM Options */
#define WOLFTPM_SLB9670
#define WOLFTPM2_USE_SW_ECDHE /* Compute shared secret in software */
#define TPM_TIMEOUT_TRIES 6000000 /* about 60 seconds */
#include "sleep.h" /* for usleep() used for network startup wait */
#define XTPM_WAIT() usleep(10)


/* Math */
#define USE_FAST_MATH
#define FP_MAX_BITS (4096 * 2) /* Max RSA 4096-bit */

/* Use Single Precision assembly math speedups for RSA, ECC and DH */
#define WOLFSSL_SP
#define WOLFSSL_SP_ASM
#define WOLFSSL_SP_ARM64_ASM
#define WOLFSSL_HAVE_SP_ECC
#define WOLFSSL_HAVE_SP_RSA
#define WOLFSSL_HAVE_SP_DH
#define WOLFSSL_SP_384
#define WOLFSSL_SP_4096
#define HAVE_DH_DEFAULT_PARAMS

/* Random: HashDRGB / P-RNG (SHA256) */
#define HAVE_HASHDRBG
extern int my_rng_seed_gen(unsigned char* output, unsigned int sz);
#define CUSTOM_RAND_GENERATE_SEED  my_rng_seed_gen

/* Override Current Time */
/* Allows custom "custom_time()" function to be used for benchmark */
#define WOLFSSL_USER_CURRTIME
#define USER_TICKS
extern unsigned long my_time(unsigned long* timer);
#define XTIME my_time

/* Timing Resistance */
#define TFM_TIMING_RESISTANT
#define ECC_TIMING_RESISTANT
#define WC_RSA_BLINDING

/* RSA */
#undef  NO_RSA
#define WC_RSA_PSS /* For TLS v1.3 */

/* ECC */
#define HAVE_ECC
#define ECC_SHAMIR
#define ECC_USER_CURVES /* SECP256R1 on by default */
#define HAVE_ECC384

/* DH */
#undef  NO_DH
#define WOLFSSL_DH_CONST
//#define HAVE_FFDHE_2048
//#define HAVE_FFDHE_4096

/* ChaCha20 / Poly1305 */
#define HAVE_CHACHA
#define HAVE_POLY1305

/* AES-GCM Only */
#define NO_AES_CBC
#define HAVE_AESGCM

/* Hashing */
#undef  NO_SHA256
#define WOLFSSL_SHA512
#define WOLFSSL_SHA384
#define WOLFSSL_SHA3
#define WOLFSSL_NO_HASH_RAW /* not supported with ARMASM */

/* chacha20 / poly1305 suites */
#define HAVE_CHACHA
#define HAVE_POLY1305
#define HAVE_ONE_TIME_AUTH

/* HKDF */
#define HAVE_HKDF

/* Disable Algorithms */
#define NO_DSA
#define NO_RC4
#define NO_MD4
#define NO_MD5
#define NO_SHA
#define NO_HC128
#define NO_RABBIT
#define NO_PSK
#define NO_DES3

/* Other */
#define WOLFSSL_IGNORE_FILE_WARN /* Ignore file include warnings */
#define NO_MAIN_DRIVER /* User supplied "main" entry point */
#define BENCH_EMBEDDED /* Use smaller buffers for benchmarking */

/* Test with "wolfssl/certs_test.h" buffers - no file system */
#define USE_CERT_BUFFERS_256
#define USE_CERT_BUFFERS_2048

/* Debugging */
#if 1
	//#define DEBUG_WOLFSSL
	#define DEBUG_WOLFTPM
	#define WOLFTPM_DEBUG_TIMEOUT
#endif

#endif /* SRC_USER_SETTINGS_H_ */
