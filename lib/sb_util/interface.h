// interface.h
#ifndef INTERFACE_H
#define INTERFACE_H

#include <stdint.h>
#include "global_vars.h"

// Declarations of your functions:
uint16_t browse_artists(int *exitCode);
int play_track(int *exitCode);
uint16_t browse_albums(int *exitCode);
uint16_t browse_tracks(int *exitCode);

#endif // INTERFACE_H