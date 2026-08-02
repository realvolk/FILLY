#pragma once

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif
#define _GNU_SOURCE

#include <sys/socket.h>
#include <sys/un.h>

#define FILLY_PEER_CRED 1
typedef struct ucred filly_ucred_t;
#define filly_get_peer_cred(fd, cred) getsockopt(fd, SOL_SOCKET, SO_PEERCRED, cred, &(socklen_t){sizeof(*cred)})

#define FILLY_INOTIFY 1
#include <sys/inotify.h>

#define FILLY_SECCOMP 1
#include <seccomp.h>

#define filly_strerror(err) strerror(err)