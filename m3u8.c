#include "m3u8.h"

#include <stdlib.h>

#ifdef M3U8_DEBUG
#include <iostream>
#define debug_print(msg, args) fprintf(msg, args);
#else // M3U8 not defined
#define debug_print(msg, args)
#endif

enum parse_token {
    START,
    TAG,
    ATTRIBUTE_NAME,
    ATTRIBUTE,
    URI
};

struct playlist *create_playlist() {
    struct playlist *pl = (struct playlist*) malloc(sizeof(struct playlist));
    pl->_parse_token = START;
    return pl;
}

void parse_playlist_chunk(struct playlist *playlist, const char *buffer, int size) {
    for (unsigned int i = 0; i < size; i++) {
        char c = buffer[i];

        // See if the playlist is being parsed from the beginning of or partly through a line
        if(playlist->_parse_token == START) {
            debug_print("[m3u8] Parsing buffer of size %i\n", size);
        }
    }
}