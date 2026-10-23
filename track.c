#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 200809L
#include "track.h"
#include <ftw.h>
#include <string.h>
#include "utils.h"

#define TRACK_EXTS ".mkv.mov.mp4"
#define PP_FINISHED 99.0f


static da_t *m_tracks;

// Alphabetical sort by path comparison func.
static int trackcmp(const void *a, const void *b) {
    return strcmp(((const track_t*)a)->path,
                  ((const track_t*)b)->path);
}

// Recursive subtree track discovery
static int visit(const char *path, const struct stat *st,
                 int type, struct FTW *ftw) {
    if (type == FTW_D && ftw->level > MAX_DEPTH) {
        return 2; // skip subtree
    } else if (type == FTW_F) {
        // Getting file extension
        const char *ext = strrchr(path, '.');
        if (ext == NULL) return 0; // continue

        // Add video tracks
        if (strstr(TRACK_EXTS, ext)) {
            track_t *track = da_add(m_tracks);
            track->ppseek = 0.0f;
            track->ppoff  = strlen(path);
            if ((track->path = strdup(path)) == NULL)
                return 1; // stop
        }
    }
    return 0; // continue

    (void)st;
}

size_t get_iplay(const da_t *tracks) {
    size_t idx = 0;
    for (; idx < tracks->len; idx++) {
        const track_t track = ((track_t*)tracks->data)[idx];
        if (track.ppseek < PP_FINISHED)
            break;
    }
    return idx;
}

void track_parse(track_t *prev, track_t *curr, char *str) {
    char *d = strrchr(str, DELIM);
    curr->path   = strndup(str, d - str);
    curr->ppseek = atof(d+1);
    curr->ppoff  = prev ? prev->ppoff + 11 : 0;
    curr->ppoff += (long)(d - str) + 1;
}

void track_discover(da_t *tracks) {
    m_tracks = tracks;
    if (nftw(".", visit, 16, FTW_PHYS))
        THROW("nftw");
    qsort(tracks->data, tracks->len, tracks->esize, trackcmp);
}

void track_write(track_t track, FILE *fp) {
    fputs(track.path, fp);
    fputc(DELIM, fp);
    fprintf(fp, "%010.6f\n", track.ppseek);
}

void track_update(track_t track, FILE *fp) {
    fseek(fp, track.ppoff, SEEK_SET);
    fprintf(fp, "%010.6f", track.ppseek);
    fflush(fp);
}
