#ifndef TRACK_H
#define TRACK_H

#include "utils.h"
#include <stdio.h>

#define MAX_DEPTH 3
#define DELIM '@'


typedef struct {
    const char *path; // path of track
    float ppseek;     // [0.0, 100.0] track progress %
    long ppoff;       // byte offset for progress % in file
} track_t;

// Gets index of track to resume playing
size_t get_iplay(const da_t *tracks);

// Parse string and write it to *curr
void track_parse(track_t *prev, track_t *curr, char *str);

// Discover tracks in subtree
void track_discover(da_t *tracks);

// Writes track to file
void track_write(track_t track, FILE *fp);

// Updates track progress % in file
void track_update(track_t track, FILE *fp);

#endif // TRACK_H
