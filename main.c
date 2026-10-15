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
#include "json.h"
#include "track.h"
#include "mpv.h"

#define BUF_SIZE 4096
#define FILENAME "progress.txt"


// Scratch buffer
char buf[BUF_SIZE];

// Handle mpv events
static void mpv_handle(mpv_ev_t ev, const json_t *msg) {
    printf("%s: ", mpv_evtostr[ev]);
    json_print(msg, stdout);
    putc('\n', stdout);
}

int main(void) {
    da_t *tracks = da_init(sizeof(track_t));
    FILE *fp = fopen(FILENAME, "r+");

    // Read lines and parse tracks
    if (fp) while (fgets(buf, BUF_SIZE, fp) != NULL)
        parse_track((track_t*)da_add(tracks), buf);
    // Create and populate progress file
    else {
        fp = fopen(FILENAME, "w+");
        if (fp == NULL) THROW("fopen");

        discover_tracks(tracks);
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

    // Begin mpv subprocess and event loop
    mpv_t *mpv = mpv_prep(tracks, iplay);
    if (mpv_begin(mpv))
        THROW("mpv_begin");
    else if (mpv_connect(mpv) == -1)
        THROW("connect");
    mpv_cmd(mpv, "{\"command\":[\"observe_property\",1,\"percent-pos\"]}\n");
    mpv_loop(mpv, buf, BUF_SIZE, mpv_handle);

    mpv_free(mpv);
cleanup:
    for (size_t i = 0; i < tracks->len; i++) {
        char *path = (char*)((track_t*)tracks->data)[i].path;
        if (path) free(path);
    }
    da_free(tracks);
    fclose(fp);
    return 0;
}
