#define _POSIX_C_SOURCE 200809L
#include "mpv.h"
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "track.h"
#include "utils.h"
#include "json.h"


const char *mpv_evtostr[_MPV_EV_COUNT] = {
    [MPV_START_FILE]         = "start-file",
    [MPV_END_FILE]           = "end-file",
    [MPV_FILE_LOADED]        = "file-loaded",
    [MPV_SEEK]               = "seek",
    [MPV_PLAYBACK_RESTART]   = "playback-restart",
    [MPV_SHUTDOWN]           = "shutdown",
    [MPV_LOG_MESSAGE]        = "log-message",
    [MPV_GET_PROPERTY_REPLY] = "get-property-reply",
    [MPV_SET_PROPERTY_REPLY] = "set-property-reply",
    [MPV_COMMAND_REPLY]      = "command-reply",
    [MPV_CLIENT_MESSAGE]     = "client-message",
    [MPV_VIDEO_RECONFIG]     = "video-reconfig",
    [MPV_AUDIO_RECONFIG]     = "audio-reconfig",
    [MPV_PROPERTY_CHANGE]    = "property-change",
};

static void handler(int sig) { (void)sig; }

mpv_t *mpv_prep(da_t *tracks, size_t iplay) {
    mpv_t *mpv = calloc(1, sizeof(mpv_t));

    char sdir[] = "/tmp/mpvtrack-XXXXXX";
    if (mkdtemp(sdir) == 0) THROW("mkdtemp");

    // Format arg strings
    char osock[64], olist[32], oseek[32];
    snprintf(mpv->spath, sizeof(mpv->spath), "%s/mpv.sock", sdir);
    snprintf(osock, sizeof(osock), "--input-ipc-server=%s", mpv->spath);
    snprintf(olist, sizeof(olist), "--playlist-start=%lu", iplay);
    const track_t track = ((track_t*)tracks->data)[iplay];
    snprintf(oseek, sizeof(oseek), "--start=%f%%", track.ppseek);

    // Populate args array
    mpv->args = da_init(sizeof(const char*));
    *(const char**)da_add(mpv->args) = "mpv";
    *(const char**)da_add(mpv->args) = "--no-terminal";
    *(const char**)da_add(mpv->args) = osock;
    *(const char**)da_add(mpv->args) = olist;
    *(const char**)da_add(mpv->args) = oseek;
    for (size_t i = 0; i < tracks->len; i++)
        *(const char**)da_add(mpv->args) = ((track_t*)tracks->data)[i].path;
    *(const char**)da_add(mpv->args) = NULL;

    // Install SIGCHLD handler
    struct sigaction sa = { .sa_handler = handler };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGCHLD, &sa, NULL);

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
    if (mpv->sock == -1)
        THROW("socket");
    mpv->sa = (struct sockaddr_un){ .sun_family = AF_UNIX };
    snprintf(mpv->sa.sun_path, sizeof(mpv->sa.sun_path), "%s", mpv->spath);

    // Await socket availability
    char sdir[sizeof mpv->spath];
    strcpy(sdir, mpv->spath);

    int ino = inotify_init1(IN_CLOEXEC);
    inotify_add_watch(ino, dirname(sdir), IN_CREATE);

    struct pollfd pfd = { .fd = ino, .events = POLLIN };
    do if (poll(&pfd, 1, -1) == -1 && errno == EINTR)
        if (waitpid(mpv->pid, NULL, WNOHANG) == mpv->pid) {
           close(ino);
           return 1;
        }
    while (access(mpv->spath, F_OK) != 0);
    close(ino);

    return 0;
}

int mpv_connect(mpv_t *mpv) {
    const struct sockaddr *sa = (struct sockaddr*)&mpv->sa;
    return connect(mpv->sock, sa, sizeof(mpv->sa));
}

void mpv_cmd(mpv_t *mpv, const char *cmd) {
    ssize_t n = 0;
    do {
        n = write(mpv->sock, cmd + n, strlen(cmd) - n);
        if (n == -1) THROW("write");
    } while (n < (ssize_t)strlen(cmd));
}

void mpv_loop(mpv_t *mpv, char *buf, size_t size,
              void (*cb)(mpv_ev_t ev, const json_t *msg)) {
    ssize_t n;
    while ((n = read(mpv->sock, buf, size-1)) > 0) {
        buf[n] = '\0';
        // TODO: handle multiple events sep \n in one buf read
        const json_t *jev = json_parse(buf);
        size_t iname = hm_geti(jev->val.obj, "event");
        if (iname != HM_NONE) {
            const json_t *jname = jev->val.obj[iname].val;
            cb(mpv_strtoev(jname->val.str), jev);
        }
        json_free(jev);
    }
}

mpv_ev_t mpv_strtoev(const char *str) {
    for (size_t i = 0; i < _MPV_EV_COUNT; i++) {
        const char *evstr = mpv_evtostr[i];
        if (evstr && strcmp(evstr, str) == 0)
            return (mpv_ev_t)i;
    }
    return _MPV_EV_COUNT;
}

void mpv_free(mpv_t *mpv) {
    kill(mpv->pid, SIGTERM);
    waitpid(mpv->pid, NULL, 0);
    close(mpv->sock);
    unlink(mpv->spath);
    rmdir(dirname(mpv->spath));
    da_free(mpv->args);
    free(mpv);
}
