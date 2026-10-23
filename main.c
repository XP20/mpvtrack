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

static ssize_t m_itrack = -1;
static da_t   *m_tracks;
static FILE   *m_fp;

// Handle mpv events
static void mpv_handle(mpv_ev_t ev, const json_t *msg) {
    if (ev == MPV_PROPERTY_CHANGE) {
        if (m_itrack == -1)
            return;
        size_t i = hm_geti(msg->val.obj, "name");
        if (i != HM_NONE && strcmp(msg->val.obj[i].val->val.str, "percent-pos"))
            return;

        i = hm_geti(msg->val.obj, "data");
        if (i == HM_NONE) return;
        track_t tcurr = ((track_t*)m_tracks->data)[m_itrack];
        tcurr.ppseek = msg->val.obj[i].val->val.num;
        track_update(tcurr, m_fp);
    } else if (ev == MPV_START_FILE) {
        size_t i = hm_geti(msg->val.obj, "playlist_entry_id");
        if (i != HM_NONE) {
            const json_t *jen = msg->val.obj[i].val;
            const int eidx = jen->val.num;
            m_itrack = eidx - 1;
        }
    } else if (ev == MPV_END_FILE) {
        m_itrack = -1;
    }
}

int main(void) {
    m_tracks = da_init(sizeof(track_t));
    m_fp = fopen(FILENAME, "r+");

    // Read lines and parse tracks
    if (m_fp) while (fgets(buf, BUF_SIZE, m_fp) != NULL) {
        track_t *curr = (track_t*)da_add(m_tracks);
        track_t *prev = (m_tracks->len > 1)
                      ? &((track_t*)m_tracks->data)[m_tracks->len-2]
                      : NULL;
        track_parse(prev, curr, buf);
    } else {
    // Create and populate progress file
        m_fp = fopen(FILENAME, "w+");
        if (m_fp == NULL) THROW("fopen");

        track_discover(m_tracks);
        for (size_t i = 0; i < m_tracks->len; i++)
            track_write(((track_t*)m_tracks->data)[i], m_fp);
        if (fflush(m_fp) == EOF) THROW("fflush");
    }

    // Get resumable track index
    size_t iplay = get_iplay(m_tracks);
    if (iplay == m_tracks->len) {
        puts("No remaining tracks");
        goto cleanup;
    }

    // Begin mpv subprocess and event loop
    mpv_t *mpv = mpv_prep(m_tracks, iplay);
    if (mpv_begin(mpv))
        THROW("mpv_begin");
    else if (mpv_connect(mpv) == -1)
        THROW("connect");
    m_itrack = iplay;
    mpv_cmd(mpv, "{\"command\":[\"observe_property\",1,\"percent-pos\"]}\n");
    mpv_loop(mpv, buf, BUF_SIZE, mpv_handle);

    mpv_free(mpv);
cleanup:
    for (size_t i = 0; i < m_tracks->len; i++)
        free((char*)((track_t*)m_tracks->data)[i].path);
    da_free(m_tracks);
    fclose(m_fp);
    return 0;
}
