
#ifndef _NETWORKING_H_
#define _NETWORKING_H_

#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/ssl.h>


#ifdef __cplusplus
    extern "C" {
#endif

typedef struct SockIoCbCtx {
    int listenFd;
    int fd;
} SockIoCbCtx;


void networking_start(void* p);


int SockIORecv(struct WOLFSSL* ssl, char* buff, int sz, void* ctx);
int SockIOSend(struct WOLFSSL* ssl, char* buff, int sz, void* ctx);
int SetupSocketAndConnect(SockIoCbCtx* sockIoCtx, const char* host,
    word32 port);
void CloseAndCleanupSocket(SockIoCbCtx* sockIoCtx);

int SetupSocketAndListen(SockIoCbCtx* sockIoCtx, word32 port);
int SocketWaitClient(SockIoCbCtx* sockIoCtx);



#ifdef __cplusplus
    }  /* extern "C" */
#endif

#endif /* _NETWORKING_H_ */
