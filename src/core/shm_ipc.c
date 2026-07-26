#include "shm_ipc.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

int shm_ipc_create(void) {
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
    if (fd < 0) return -1;
    if (ftruncate(fd, SHM_SIZE) < 0) { close(fd); return -1; }
    return fd;
}

void *shm_ipc_map(int fd) {
    void *addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) return NULL;
    return addr;
}

void shm_ipc_unmap(void *addr) {
    if (addr) munmap(addr, SHM_SIZE);
}