#pragma once
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define FILLY_PEER_CRED 1
#define filly_get_peer_cred(fd, cred) getpeereid(fd, &(cred)->uid, &(cred)->gid)
typedef struct { uid_t uid; gid_t gid; } filly_ucred_t;

#define FILLY_INOTIFY 0
#define FILLY_KQUEUE 1

#define FILLY_SECCOMP 0
#define FILLY_PLEDGE 1

#define filly_strerror(err) strerror(err)