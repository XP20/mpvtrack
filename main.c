#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 200809L
#include <ftw.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <poll.h>
#include <linux/limits.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#define UTILS_IMPL
#include "utils.h"
#include "track.h"
#include "mpv.h"

#define BUF_SIZE 4096
#define FILENAME "progress.txt"


// Scratch buffer
uint8_t buf[BUF_SIZE];

int main(void) {
    da_t *tracks = da_init(sizeof(track_t));

    // Read and parse progress file
    FILE *fp = fopen(FILENAME, "r+");
    if (fp) {
        // TODO: ...
        size_t n;
        while ((n = fread(buf, 1, BUF_SIZE-1, fp))) {
            buf[n] = '\0';
            printf("read %lu bytes\n", n);
        }
    // Populate missing progress file
    } else {
        fp = fopen(FILENAME, "w+");
        if (fp == NULL) THROW("fopen");

        get_tracks(tracks);
        for (size_t i = 0; i < tracks->len; i++)
            put_track(((track_t*)tracks->data)[i], fp);
        if (fflush(fp) == EOF) THROW("fflush");
    }

    // Get resumable track index
    size_t iplay = get_iplay(tracks);
    if (iplay == tracks->len) {
        puts("No remaining tracks");
        goto cleanup;
    }

    // Setup and start mpv subprocess
    mpv_t mpv = mpv_prep(tracks, iplay);
    if (mpv_begin(&mpv))
        THROW("mpv_begin");
    else if (mpv_connect(&mpv) == -1)
        THROW("connect");

    // TODO: abstract to mpv.c
    const char *cmd = "{\"command\":[\"observe_property\",1,\"percent-pos\"]}\n";
    write(mpv.sock, cmd, strlen(cmd));
    ssize_t n;
    while ((n = read(mpv.sock, buf, BUF_SIZE - 1)) > 0) {
        buf[n] = '\0';
        puts((char*)buf);
    }

    mpv_free(&mpv);
cleanup:
    for (size_t i = 0; i < tracks->len; i++) {
        char *path = (char*)((track_t*)tracks->data)[i].path;
        if (path) free(path);
    }
    da_free(tracks);
    fclose(fp);
    return 0;
}
