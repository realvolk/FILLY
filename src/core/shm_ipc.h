#pragma once
#include <stddef.h>

#define SHM_NAME "/filly_shm"
#define SHM_SIZE (16 * 1024 * 1024)

int shm_ipc_create(void);
void *shm_ipc_map(int fd);
void shm_ipc_unmap(void *addr);