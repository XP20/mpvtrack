#ifndef TRACK_H
#define TRACK_H

#include "utils.h"
#include <stdio.h>

#define MAX_DEPTH 3
#define DELIM '@'


typedef struct {
    const char *path; // path of track
    float ppseek;     // [0.0, 100.0] track progress %
} track_t;

// Gets index of track to resume playing
size_t get_iplay(const da_t *tracks);

// Parse string and write it to *tp
void parse_track(track_t *tp, char *str);

// Discover tracks in subtree
void discover_tracks(da_t *tracks);

// Writes track to stream
void put_track(track_t track, FILE *fp);

#endif // TRACK_H
