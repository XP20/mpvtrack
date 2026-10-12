#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 200809L
#include <ftw.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <linux/limits.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <poll.h>
#include <sys/inotify.h>

#define BUF_SIZE 4096
#define DA_INIT_SIZE 16
#define FILENAME "progress.txt"
#define DELIM '@'

static uint8_t buf[BUF_SIZE];

struct entry {
    const char *path;
    float pprog;  // 0.0 - 1.0 progress percentage
};

// Alphabetical sort comparison func.
static int cmp_char(const void *a, const void *b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

// Dynamic array impl.
struct da {
    void *data;
    size_t len, cap, esize;
};
static void da_free(struct da *p) { free(p->data); }
static void da_init(struct da *p, size_t size) {
    p->data = malloc((p->cap = DA_INIT_SIZE) * (p->esize = size));
    p->len = 0;
}
static void *da_add(struct da *p) {
    if (p->len == p->cap)
        p->data = realloc(p->data, (p->cap <<= 1) * p->esize);
    return (char*)p->data + (p->len++) * p->esize;
}

// Recursive subtree track discovery
static struct da tracks;
static int visit(const char *path, const struct stat *st,
                 int type, struct FTW *ftw) {
    if (type == FTW_F) {
        const char *ext = path;
        for (size_t i = 0; path[i] != '\0'; i++)
            if (path[i] == '.') ext = path + i;
        if (strcmp(ext, ".mkv") == 0
            && !(*(char**)da_add(&tracks) = strdup(path)))
            return 1;
    }
    return 0;

    (void)st, (void)type, (void)ftw;
}

static void throw(const char *err) {
    perror(err);
    exit(1);
}

int main(void) {
    struct da entries;
    da_init(&entries, sizeof (struct entry));

    FILE *fp = fopen(FILENAME, "r+");
    if (fp) { // Read progress data
        // TODO: ...
        size_t n;
        while ((n = fread(buf, 1, BUF_SIZE-1, fp))) {
            buf[n] = '\0';
            printf("read %lu bytes\n", n);
        }
    } else { // Populate progress tracks if file missing
        fp = fopen(FILENAME, "w+");
        if (!fp) throw("fopen");

        da_init(&tracks, sizeof (const char*));
        if (nftw(".", visit, 16, FTW_PHYS)) throw("nftw");
        qsort(tracks.data, tracks.len, tracks.esize, cmp_char);

        for (size_t i = 0; i < tracks.len; i++) {
            const char *track = ((char**)tracks.data)[i];
            fputs(track, fp);
            fputc(DELIM, fp);
            fputs("0.0\n", fp);
            ((struct entry*)da_add(&entries))->path = track;
        }
        if (fflush(fp) == EOF) throw("fflush");
        da_free(&tracks);
    }

    size_t ien = 0;
    for (; ien < entries.len && ((struct entry*)entries.data)[ien].pprog >= 1.0f; ien++);
    if (ien == entries.len) goto cleanup;

    // MPV IPC
    char sdir[] = "/tmp/anitrack-XXXXXX";
    if (!mkdtemp(sdir)) throw("mkdtemp");
    
    char spath[32], oipc[64], olist[32];
    snprintf(spath, sizeof spath, "%s/mpv.sock", sdir);
    snprintf(oipc, sizeof oipc, "--input-ipc-server=%s", spath);
    snprintf(olist, sizeof olist, "--playlist-start=%lu", ien);

    struct da args;
    da_init(&args, sizeof (const char*));
    *(const char**)da_add(&args) = "mpv";
    *(const char**)da_add(&args) = "--no-terminal";
    *(const char**)da_add(&args) = oipc;
    *(const char**)da_add(&args) = olist;
    for (size_t i = 0; i < entries.len; i++)
        *(const char**)da_add(&args) = ((struct entry*)entries.data)[i].path;
    *(const char**)da_add(&args) = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        execvp("mpv", (char**)args.data);
        _exit(1);
    } else if (pid < 0) goto cleanup;

    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s == -1) throw("socket");
    struct sockaddr_un a = { .sun_family = AF_UNIX };
    snprintf(a.sun_path, sizeof a.sun_path, "%s", spath);

    int ino = inotify_init1(IN_CLOEXEC);
    inotify_add_watch(ino, sdir, IN_CREATE);

    struct pollfd pfd = { .fd = ino, .events = POLLIN };
    while (poll(&pfd, 1, -1) > 0) {
        read(ino, buf, BUF_SIZE);
        if (access(spath, F_OK) == 0) break;
    }
    close(ino);
    if (connect(s, (struct sockaddr*)&a, sizeof a) == -1)
        throw("connect");

    const char *cmd = "{\"command\":[\"observe_property\",1,\"percent-pos\"]}\n";;
    write(s, cmd, strlen(cmd));

    ssize_t n;
    while ((n = read(s, buf, BUF_SIZE - 1)) > 0) {
        buf[n] = '\0';
        puts((char*)buf);
        // fflush(stdout);
    }

    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    close(s); unlink(spath); rmdir(sdir);
    free(args.data);

cleanup:
    for (size_t i = 0; i < entries.len; i++) {
        char *path = (char*)((struct entry*)entries.data)[i].path;
        if (path) free(path);
    }
    da_free(&entries);
    fclose(fp);
    return 0;
}
