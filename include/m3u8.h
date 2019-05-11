/* m3u8.h -- a fast parser for the m3u8 format, following RFC 8216.
 * See https://github.com/WalkerKnapp/m3u8
 */

#pragma once

#include "m3u8-types.h"

#include <string.h>

typedef void M3U8;

void m3u8_setopt_allow_custom_tags(M3U8* m3u8, bool allowed);

M3U8 *create_m3u8();

playlist *create_playlist(M3U8* m3u8);

void parse_playlist_chunk(M3U8* m3u8, playlist *playlist, const char *buffer, int size);

playlist *parse_playlist(M3U8 *m3u8, const char *in) {
    playlist *pl = create_playlist(m3u8);
    parse_playlist_chunk(m3u8, pl, in, strlen(in));
    return pl;
}
