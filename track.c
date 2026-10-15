#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 200809L
#include "track.h"
#include <ftw.h>
#include <string.h>
#include "utils.h"

#define TRACK_EXTS ".mkv.mov.mp4"
#define PP_FINISHED 99.5f


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

void parse_track(track_t *tp, char *str) {
    char *delim = strrchr(str, DELIM);
    tp->path = strndup(str, delim - str);
    tp->ppseek = atof(delim+1);
}

void discover_tracks(da_t *tracks) {
    m_tracks = tracks;
    if (nftw(".", visit, 16, FTW_PHYS))
        THROW("nftw");
    qsort(tracks->data, tracks->len, tracks->esize, trackcmp);
}

void put_track(track_t track, FILE *fp) {
    fputs(track.path, fp);
    fputc(DELIM, fp);
    fprintf(fp, "%f\n", track.ppseek);
}
