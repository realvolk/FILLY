#pragma once
#include <sys/socket.h>
#include <sys/un.h>

#define FILLY_PEER_CRED 1
typedef struct cmsgcred filly_ucred_t;
/* FreeBSD uses LOCAL_PEERCRED via recvmsg */
static inline int filly_get_peer_cred(int fd, filly_ucred_t *cred) {
    struct msghdr msg = {0};
    char buf[256];
    struct iovec iov = {buf, sizeof(buf)};
    msg.msg_iov = &iov; msg.msg_iovlen = 1;
    msg.msg_control = buf; msg.msg_controllen = sizeof(buf);
    if (recvmsg(fd, &msg, MSG_PEEK) < 0) return -1;
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_type == SCM_CREDS) {
        *cred = *(struct cmsgcred *)CMSG_DATA(cmsg);
        return 0;
    }
    return -1;
}

#define FILLY_INOTIFY 0
#define FILLY_KQUEUE 1

#define FILLY_SECCOMP 0
#define FILLY_CAPSICUM 1
#include <sys/capsicum.h>

#if FILLY_CAPSICUM
#include <sys/capsicum.h>
static inline void filly_capsicum_enter(void) {
    cap_rights_t rights;
    cap_rights_init(&rights, CAP_READ, CAP_WRITE, CAP_ACCEPT, CAP_BIND,
                    CAP_FCNTL, CAP_FSTAT, CAP_IOCTL, CAP_MMAP, CAP_PDGETPID,
                    CAP_PDKILL, CAP_SEEK, CAP_SETSOCKOPT, CAP_SHMLINK);
    cap_rights_limit(STDIN_FILENO, &rights);
    cap_rights_limit(STDOUT_FILENO, &rights);
    cap_rights_limit(STDERR_FILENO, &rights);
    cap_enter();
}
#endif

#define filly_strerror(err) strerror(err)