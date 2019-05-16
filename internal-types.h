#pragma once

#include "m3u8.h"
#include "hash.h"

#include <stdio.h>

#define M3U8_DEBUG
#ifdef M3U8_DEBUG
#define debug_print0(msg) fprintf(stderr, msg)
#define debug_print(msg, arg) fprintf(stderr, msg, arg)
#define debug_print2(msg, arg1, arg2) fprintf(stderr, msg, arg1, arg2)
#else // M3U8 not defined
#define debug_print0(msg)
#define debug_print(msg, arg)
#define debug_print2(msg, arg1, arg2)
#endif

enum parse_token {
    START,
    TAG,
    TAG_SKIPPING,
    ATTRIBUTE,
    URI
};

typedef struct {
    bool allow_custom_tags;
    // EXT-X-MEDIA-SEGMENT and EXT-X-DISCONTINUITY-SEQUENCE
    bool strict_attribute_lists;

    // Parsing helpers
    enum parse_token _parse_token;
    char *_parse_buffer;

    HASH tag_hash;
    HASH attribute_name_hash;
    HASH attribute_enum_hash;

    int _parse_index;
    void *_latest_content_token;

    int _attribute_sequence;

    media_init_section *latest_media_init_section;
} M3U8_internal;
