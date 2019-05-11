#pragma once

#include <stdbool.h>

enum variant_playlist_type {
    AUDIO,
    VIDEO,
    SUBTITLES,
    CLOSED_CAPTIONS
};

typedef enum {
    HDCP_LEVEL_NONE,
    HDCP_LEVEL_TYPE_0
} variant_playlist_hdcp_level;

typedef struct {
    char *uri;

    /* EXT-X-MEDIA */
    enum variant_playlist_type type;
    unsigned long group_id_hash;
    char *language;
    char *assoc_lang;
    char *name;
    bool is_default;
    bool is_autoselect;
    bool is_forced;
    char *instream_id;
    char **characteristics;
    char *channels;

    void *next_variant_rendition;
} variant_rendition;

typedef struct {
    // The URI the media playlist will be retrieved from - Required
    char *uri;

    /* EXT-X-STREAM-INF */
    int max_bandwidth;
    int average_bandwidth;
    char *codecs;
    int width;
    int height;
    float framerate;
    variant_playlist_hdcp_level hdcp_level;
    unsigned long rendition_group_id_hash;

    void *next_variant_playlist;
} variant_playlist;

typedef struct {
    // See 4.3.4.3
    char *uri; // Required

    /* EXT-X-I-FRAME-STREAM-INF */
    int max_bandwidth;
    int average_bandwidth;
    char **codecs;
    int width;
    int height;
    int framerate;
    int hdcp_level;
} i_frame_playlist;

typedef struct {
    char *data_id;

    // Should contain value or uri, not both
    char *value;
    char *uri; // Resource must be formatted in JSON

    char *language;

    void *next_session_data;
} session_data;
