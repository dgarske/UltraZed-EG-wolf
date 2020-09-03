# Xilinx UltraScale+ MPSoC with FreeRTOS, LWIP and wolfSSL

Instructions for Xilinx Vitis 2020.1 and UltraZed-EG (zu3eg)

## Vitis IDE 2020.1 Build Steps

Loosely based on "UltraZed_EG_SK_Tutorial_2016_4_v1" -> "UltraZed_EG_Starter_Kit_Tutorial_Vivado_2016_4.pdf".

### Create the hardware platform for UltraZed-EG

1) Run Xilinx Vivado 2020.1
2) Create new project
3) Add IP for Zynq UltraScale+ part `xazu3eg-sfva625-1-i`
4) Customize Block: Update IP configuration for UltraZed-EG board presets
    a) Use file `ultrazed_board_definition_files_v2017_2/ultrazed_3eg_som/1.0/preset.xml` to input settings.
    b) Manually for all settings: DDR, Ethernet, UART0/1, etc... based on board presets (long manual process)
    c) PS PMOD TPM: SPI0 for (MIO 38-43) - enable SS[0]=MIO41, SS[1]=MIO40, SS[2]=MIO39
5) Create / Generate HDL Wrapper
6) Generate bit-stream file
7) File -> Export -> Export Hardware

See `design_1_wrapper.xsa`.

### Creating the Platform Project

1) Run Xilinx Vitis 2020.1
2) Choose new workspace
3) File -> New -> Platform project (or use existing)
4) Enter platform name "UltraZed-EG"
5) Choose: Operation system = freertos10_xilinx, Processor=psu_cortexa53_0
6) Open the platform.spr and edit BSP for freertos10_linux.
    a) Check lwip211, xilsecure and xilkey libraries
    b) Change lwip211 "api_mode" value from "RAW API" to "SOCKET API"
    c) Change freertos10_xilinx -> kernel_behavior -> tick_rate from 100 to 1000
    d) increase heap size freertos10_xilinx -> kernel_behavior -> total_heap_size from 65536 to 200000
7) Build platform (this takes a long time)

### Creating the Application Project

1) File -> New -> Application Project
2) Choose existing hardware platform or .xsa
3) Name the application project "UltraZen-EG-wolf"
4) Choose system project or create new "UltraZen-EG-wolf_system"
5) Choose example FreeRTOS LWIP Echo Server
6) Adjust IP address in main.c
7) Build and Run/Debug on Hardware
8) Example output from UART0

```
Xilinx Zynq MP First Stage Boot Loader 
Release 2020.1   Aug 14 2020  -  10:29:54
PMU-FW is not running, certain applications may not be supported.


-----lwIP Socket Mode Echo server Demo Application ------
Board IP: 192.168.0.20
Netmask : 255.255.255.0
Gateway : 192.168.0.1
Start PHY autonegotiation 
Waiting for PHY to complete autonegotiation.
autonegotiation complete 
link speed for phy address 9: 1000

              Server   Port Connect With..
-------------------- ------ --------------------
         echo server      7 $ telnet <board_ip> 7
```

9) Telnet echo test from host: `telnet 192.168.0.20 7`

### Creating library projects

All libraries and application use a shared `user_settings.h` to configure wolf. This requires each project have a pre-processor symbol `WOLFSSL_USER_SETTINGS` defined.

Generic Steps:
1) New -> Library Project
2) Name: [wolflibname], Type: static library
3) System Project: Choose existing "UltraZed-EG-wolf_system".
4) Template: Empty Application
5) Finish

#### wolfSSL Static Library

1) Create a generic static library named wolfssl-lib
2) In "wolfssl-lib/src" either download latest release "https://www.wolfssl.com/wolfssl-4.5.0.zip" and extract OR use "git clone https://github.com/wolfSSL/wolfssl.git"
3) Refresh the "src" folder to see new files
4) Exclude all folders except `src`, `wolfcrypt` and `wolfssl`.
5) Add compiler symbol `WOLFSSL_USER_SETTINGS`.
6) Add the compiler include directory for the `wolfssl-root` and the location of the `user_settings.h` file.

#### wolfTPM Static Library

1) Create a generic static library named wolfssl-lib
2) In "wolftpm-lib/src" either download latest release "https://www.wolfssl.com/wolftpm-1.8.0.zip" and extract OR use "git clone https://github.com/wolfSSL/wolftpm.git"
3) Refresh the "src" folder to see new files
4) Add compiler symbol `WOLFSSL_USER_SETTINGS`.
5) Add the compiler include directory for the wolfTPM root, wolfSSL root and the location of the `user_settings.h` file.

### wolfBoot Application (standalone)

1) Create a new Application project "wolfBoot"
2) Choose existing platform, create new system project and existing domain for standalone.
3) Choose "Empty Application".
4) In "wolfboot/src" either download latest release "https://www.wolfssl.com/wolfBoot-1.5.zip" and extract OR use "git clone https://github.com/wolfSSL/wolfboot.git"
5) wolfBoot has sub-modules for wolfSSL and wolfTPM (`git submodule init`, `git submodule update`)
6) Refresh the "src" folder to see new files
7) Exclude folders / files not used by wolfBoot
8) Add the following build symbols:

```
<listOptionValue builtIn="false" value="USE_BUILTIN_STARTUP"/>
<listOptionValue builtIn="false" value="DEBUG_ZYNQ"/>
<listOptionValue builtIn="false" value="DEBUG=1"/>
<listOptionValue builtIn="false" value="__WOLFBOOT"/>
<listOptionValue builtIn="false" value="PART_UPDATE_EXT=1"/>
<listOptionValue builtIn="false" value="PART_SWAP_EXT=1"/>
<listOptionValue builtIn="false" value="PART_BOOT_EXT=1"/>
<listOptionValue builtIn="false" value="EXT_FLASH=1"/>
<listOptionValue builtIn="false" value="WOLFBOOT_VERSION=0"/>
<listOptionValue builtIn="false" value="WOLFBOOT_HASH_SHA3_384"/>
<listOptionValue builtIn="false" value="ARCH_AARCH64"/>
<listOptionValue builtIn="false" value="ARCH_FLASH_OFFSET=0x00"/>
<listOptionValue builtIn="false" value="MMU"/>
<listOptionValue builtIn="false" value="WOLFBOOT_SIGN_RSA4096"/>
<listOptionValue builtIn="false" value="XMALLOC_USER"/>
<listOptionValue builtIn="false" value="WOLFSSL_USER_SETTINGS"/>
<listOptionValue builtIn="false" value="PLATFORM_zynq"/>
<listOptionValue builtIn="false" value="IMAGE_HEADER_SIZE=1024"/>
```


## Creating a Boot.bin image

### Create new FSBL Project

1) New -> Application Project
2) Named fsbl with new system
3) Domain: standalone
4) Template: Zynq MP FSBL
5) Build and Copy fsbl.elf into Platform `UltraZed-EG-Platform\export\UltraZed-EG-Platform\sw\UltraZed-EG-Platform\boot`

### Create boot image

1) Right-click on Project and choose "Create Boot Image"
2) 

### Program Flash

1) Right-click on Project and choose "Program Flash"
2) Choose destination (QSPI dual parallel or eMMC)


## Steps for older Xilinx SDK

### Xilinx SDK Workspace

1) Open Xilinx SDK with working directory in wolfssl-examples/Xilinx/FreeRTOS/
2) File->New->Board Support Package
    a) select "Specify"
    b) make "Project name" be ultrazed
    c) under "Target Hardware Specification" select "Browse.." and choose the file wolfssl-examples/Xilinx/FreeRTOS/ultrazed_wrapper.hdf"
    d) select "Finish"

3) In Xilinx Board Support Package Project
    a) select Board Support Package OS as "freertos901_xilinx"
    b) leave default project name and target hardware as "ultrazed"
    c) select "Finish"

    d) check "lwip141" and "xilsecure" boxes
    c) change lwip141 "api_mode" value from "RAW API" to "SOCKET API"
    e) chang freertos901_xilinx->kernel_behavior->tick_rate from 100 to 1000
    f) increase heap size freertos901_xilinx->kernel_behavior->total_heap_size from 65536 to 200000
    g) select OK
4) File->New->Application Project
    a) set project name to "fsbl"
    b) select "next"
    c) select "Zynq MP FSBL" from "Available Templates"
    d) select "Finsih"
5) File->Import->General->Existing Projects into Workspace
    a) select "Browse..." and go to directory wolfssl-examples/Xilinx/FreeRTOS
    b) select "Finish"

### Build and Run wolfcrypt_test

1) right click on wolfcrypt_test project and select "Build Project"
2) select wolfcrypt_test so it is highlighted then select Xilinx Tools->Create Boot Image
3) select "Create Image"
4) put the files created in wolfssl-examples/Xilinx/FreeRTOS/wolfcrypt_test/bootimage/ onto an SD card and run it on the board

(Note if error -1303 comes up from when running the image, just restart the board. It is because the time was not set yet)


## Demo

```
Xilinx Zynq MP First Stage Boot Loader 

Release 2020.1   Sep  3 2020  -  12:08:27
PMU-FW is not running, certain applications may not be supported.

Demonstrating wolfSSL software implementation
Waiting for network to start
Start PHY autonegotiation 
Waiting for PHY to complete autonegotiation.
autonegotiation complete 
link speed for phy address 9: 1000
DHCP request success
Board IP: 192.168.0.168
Netmask : 255.255.255.0
Gateway : 192.168.0.1


				MENU

	t. wolfCrypt Test
	b. wolfCrypt Benchmark
	s. wolfSSL TLS Server
	c. wolfSSL TLS Client
	e. Xilinx TCP Echo Server
	r. TPM Generate Certificate Signing Request (CSR)
	g. TPM Get/Set Time
	p. TPM Signed Timestamp
	v. Certification Chain Validate Test
Please select one of the above options:

t
Running wolfCrypt Tests...
------------------------------------------------------------------------------
 wolfSSL version 4.5.0
------------------------------------------------------------------------------
error    test passed!
MEMORY   test passed!
base64   test passed!
asn      test passed!
RANDOM   test passed!
SHA-256  test passed!
SHA-384  test passed!
SHA-512  test passed!
SHA-3    test passed!
Hash     test passed!
HMAC-SHA256 test passed!
HMAC-SHA384 test passed!
HMAC-SHA512 test passed!
HMAC-SHA3   test passed!
HMAC-KDF    test passed!
GMAC     test passed!
Chacha   test passed!
POLY1305 test passed!
ChaCha20-Poly1305 AEAD test passed!
AES      test passed!
AES192   test passed!
AES256   test passed!
AES-GCM  test passed!
RSA      test passed!
PWDBASED test passed!
ECC      test passed!
ECC buffer test passed!
logging  test passed!
mutex    test passed!
crypto callback test passed!
Test complete
Crypt Test: Return code 0
Return code 0

b
Running wolfCrypt Benchmarks...
------------------------------------------------------------------------------
 wolfSSL version 4.5.0
------------------------------------------------------------------------------
wolfCrypt Benchmark (block bytes 1024, min  sec each)
RNG                  1 MB took 1.016 seconds,    1.370 MB/s
AES-128-GCM-enc      1 MB took 1.013 seconds,    1.374 MB/s
AES-128-GCM-dec      1 MB took 1.014 seconds,    1.372 MB/s
AES-192-GCM-enc      1 MB took 1.003 seconds,    1.314 MB/s
AES-192-GCM-dec      1 MB took 1.003 seconds,    1.314 MB/s
AES-256-GCM-enc      1 MB took 1.002 seconds,    1.267 MB/s
AES-256-GCM-dec      1 MB took 1.004 seconds,    1.264 MB/s
CHACHA               6 MB took 1.001 seconds,    5.683 MB/s
CHA-POLY             4 MB took 1.003 seconds,    4.284 MB/s
POLY1305            24 MB took 1.000 seconds,   24.097 MB/s
SHA-256              3 MB took 1.003 seconds,    3.164 MB/s
SHA-384              7 MB took 1.002 seconds,    7.163 MB/s
SHA-512              7 MB took 1.001 seconds,    7.171 MB/s
SHA3-224             8 MB took 1.001 seconds,    7.780 MB/s
SHA3-256             7 MB took 1.002 seconds,    7.358 MB/s
SHA3-384             6 MB took 1.003 seconds,    5.720 MB/s
SHA3-512             4 MB took 1.000 seconds,    4.028 MB/s
HMAC-SHA256          3 MB took 1.003 seconds,    3.140 MB/s
HMAC-SHA384          7 MB took 1.000 seconds,    7.056 MB/s
HMAC-SHA512          7 MB took 1.000 seconds,    7.056 MB/s
PBKDF2             416 bytes took 1.034 seconds,  402.321 bytes/s
RSA     2048 public       1934 ops took 1.000 sec, avg 0.517 ms, 1934.000 ops/sec
RSA     2048 private       102 ops took 1.017 sec, avg 9.971 ms, 100.295 ops/sec
ECC      256 key gen      1445 ops took 1.000 sec, avg 0.692 ms, 1445.000 ops/sec
ECDHE    256 agree         880 ops took 1.000 sec, avg 1.136 ms, 880.000 ops/sec
ECDSA    256 sign         1256 ops took 1.000 sec, avg 0.796 ms, 1256.000 ops/sec
ECDSA    256 verify       1050 ops took 1.001 sec, avg 0.953 ms, 1048.951 ops/sec
Benchmark complete
Return code 0

g
TPM2 Demo of setting the TPM clock forward
TPM2_ReadClock: success
TPM2_ClockSet: success
TPM2_ReadClock: success

	 oldClock=39792062 
	 newClock=39842024 

Return code 0

v

Verification successful on cert1!
------------------------------------------------------------

Verification successful on cert2!
------------------------------------------------------------

Return code 0

```


## Support

For questions email support@wolfssl.com.
