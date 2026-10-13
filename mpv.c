#define _POSIX_C_SOURCE 200809L
#include "mpv.h"
#include <signal.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "track.h"
#include "utils.h"

mpv_t mpv_prep(da_t *tracks, size_t iplay) {
    mpv_t mpv = {0};

    char sdir[] = "/tmp/mpvtrack-XXXXXX";
    if (mkdtemp(sdir) == 0) THROW("mkdtemp");

    // Format arg strings
    char osock[64], olist[32], oseek[32];
    snprintf(mpv.spath, sizeof(mpv.spath), "%s/mpv.sock", sdir);
    snprintf(osock, sizeof(osock), "--input-ipc-server=%s", mpv.spath);
    snprintf(olist, sizeof(olist), "--playlist-start=%lu", iplay);
    const track_t track = ((track_t*)tracks->data)[iplay];
    snprintf(oseek, sizeof(oseek), "--start=%f%%", track.ppseek);

    // Populate args array
    mpv.args = da_init(sizeof(const char*));
    *(const char**)da_add(mpv.args) = "mpv";
    *(const char**)da_add(mpv.args) = "--no-terminal";
    *(const char**)da_add(mpv.args) = osock;
    *(const char**)da_add(mpv.args) = olist;
    // *(const char**)da_add(mpv.args) = oseek;
    for (size_t i = 0; i < tracks->len; i++)
        *(const char**)da_add(mpv.args) = ((track_t*)tracks->data)[i].path;
    *(const char**)da_add(mpv.args) = NULL;

    return mpv;
}

int mpv_begin(mpv_t *mpv) {
    // Spawn mpv subprocess
    mpv->pid = fork();
    if (mpv->pid == 0) {
        execvp("mpv", (char**)mpv->args->data);
        _exit(1);
    } else if (mpv->pid < 0) return 1;

    // Setup mpv socket
    mpv->sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (mpv->sock == -1) THROW("socket");
    mpv->sa = (struct sockaddr_un){ .sun_family = AF_UNIX };
    snprintf(mpv->sa.sun_path, sizeof(mpv->sa.sun_path), "%s", mpv->spath);

    // Await socket availability
    int ino = inotify_init1(IN_CLOEXEC);
    inotify_add_watch(ino, dirname(mpv->spath), IN_CREATE);
    struct pollfd pfd = { .fd = ino, .events = POLLIN };
    while (poll(&pfd, 1, -1) > 0)
        if (access(mpv->spath, F_OK) == 0) break;
    close(ino);

    return 0;
}

int mpv_connect(mpv_t *mpv) {
    const struct sockaddr *sa = (struct sockaddr*)&mpv->sa;
    return connect(mpv->sock, sa, sizeof(mpv->sa));
}

void mpv_free(mpv_t *mpv) {
    kill(mpv->pid, SIGTERM);
    waitpid(mpv->pid, NULL, 0);
    close(mpv->sock);
    unlink(mpv->spath);
    rmdir(dirname(mpv->spath));
    da_free(mpv->args);
}
