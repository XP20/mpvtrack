#ifndef MPV_H
#define MPV_H

#include <sys/types.h>
#include <sys/un.h>
#include "json.h"
#include "utils.h"


// IPC event layout:
// {"event": name, "id": id, "error": error}
typedef enum {
    MPV_START_FILE,
    MPV_END_FILE,
    MPV_FILE_LOADED,
    MPV_SEEK,
    MPV_PLAYBACK_RESTART,
    MPV_SHUTDOWN,
    MPV_LOG_MESSAGE,
    MPV_GET_PROPERTY_REPLY,
    MPV_SET_PROPERTY_REPLY,
    MPV_COMMAND_REPLY,
    MPV_CLIENT_MESSAGE,
    MPV_VIDEO_RECONFIG,
    MPV_AUDIO_RECONFIG,
    MPV_PROPERTY_CHANGE,
    _MPV_EV_COUNT,
} mpv_ev_t;

extern const char *mpv_evtostr[_MPV_EV_COUNT];

typedef struct {
    char spath[32];        // mpv socket path
    int sock;              // mpv socket file descriptor
    struct sockaddr_un sa; // mpv socket address
    pid_t pid;             // mpv subprocess pid
    da_t *args;            // mpv subprocess args
    char *starts;          // file "--start=..." seek offsets
} mpv_t;

// Prepare mpv data
mpv_t *mpv_prep(da_t *tracks, size_t iplay);

// Spawn mpv subprocess, returning 1 on error
int mpv_begin(mpv_t *mpv);

// Connect to mpv socket
int mpv_connect(mpv_t *mpv);

// Send command to mpv
void mpv_cmd(mpv_t *mpv, const char *cmd);

// Begin mpv IPC event loop
void mpv_loop(mpv_t *mpv, char *buf, size_t size, void (*cb)(mpv_ev_t ev, const json_t *msg));

// Event string to enum value
mpv_ev_t mpv_strtoev(const char *str);

// Kill subprocess and free mpv data
void mpv_free(mpv_t *mpv);

#endif // MPV_H
