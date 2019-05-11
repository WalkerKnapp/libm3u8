#pragma once

#include <stdbool.h>
#include <corecrt.h>

enum key_method {
    KEY_METHOD_NONE,
    KEY_METHOD_AES128,
    KEY_METHOD_SAMPLEAES
};

enum media_playlist_type {
    EVENT, // Media Segments can be added onto the end of the playlist
    VOD // The Media Playlist cannot change
};

typedef struct {
    char *uri;

    int subrange_length;
    int subrange_offset;
} media_init_section;

typedef struct {
    // The URI the segment is able to be retrieved from. - Required
    char *uri;

    /* #EXTINF */
    // The total duration of the media in this segment. - Required
    float duration;
    // A human-readable title for the media segment. - Nullable
    char *title;

    /* #EXT-X-BYTERANGE */
    // The length of the sub-range in the URI the media is stored in. - 0 indicates that the whole resource is used
    int subrange_length;
    // The index of the first byte in the sub-range in the URI the media is stored in.
    int subrange_offset;

    /* #EXT-X-DISCONTINUITY */
    // If the segment is discontinuous with the bytestream of the previous segment.
    bool discontinuity;

    /* #EXT-X-KEY */
    // The method of encryption used for the data in the URI
    enum key_method key_method;
    // A URI that specifies "how to obtain the key." - Required if key_method != NONE
    // TODO: Complete implementation of keys
    char *key_uri;
    // 16 8-byte integers representing the IV Vector used with AES.
    int *key_iv;
    // A string specifying how the key is represented in the URI
    char *key_format;
    // A string specifying key versions a particular key_format is compatible with
    char *key_format_versions;

    /* #EXT-X-MAP */
    media_init_section *init_section;

    /* #EXT-X-PROGRAM-DATE-TIME */
    // An absolute date-time associated with the first sample of the segment.
    time_t date_time;

    /* #EXT-X-DATERANGE */
    // TODO: Find a way of accurately representing this info

    /* #EXT-X-MEDIA-SEQUENCE */
    // The Media Sequence Number of the media segment
    int media_sequence;

    void *next_media_segment;
} media_segment;