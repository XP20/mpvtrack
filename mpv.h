#ifndef MPV_H
#define MPV_H

#include <sys/types.h>
#include <sys/un.h>
#include "utils.h"


typedef struct {
    char spath[32];        // mpv socket path
    int sock;              // mpv socket file descriptor
    struct sockaddr_un sa; // mpv socket address
    pid_t pid;             // mpv subprocess pid
    da_t *args;            // mpv subprocess args
} mpv_t;

// Prepare mpv data
mpv_t mpv_prep(da_t *tracks, size_t iplay);

// Spawn mpv subprocess, returning 1 on error
int mpv_begin(mpv_t *mpv);

// Connect to mpv socket
int mpv_connect(mpv_t *mpv);

// Kill subprocess and free mpv data
void mpv_free(mpv_t *mpv);

#endif // MPV_H
