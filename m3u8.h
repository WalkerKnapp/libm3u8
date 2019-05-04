/* m3u8.h -- a fast parser for the m3u8 format, following RFC 8216.
 * See https://github.com/WalkerKnapp/m3u8
 */

#pragma once

#include <stdbool.h>
#include <corecrt.h>

enum playlist_type {
    MASTER_PLAYLIST,
    MEDIA_PLAYLIST
};

/* Master Playlists */

enum variant_playlist_type {
    AUDIO,
    VIDEO,
    SUBTITLES,
    CLOSED_CAPTIONS
};

struct variant_rendition {
    char *uri;

    /* EXT-X-MEDIA */
    enum variant_playlist_type type;
    char *group_id;
    char *language;
    char *assoc_lang;
    char *name;
    bool is_default;
    bool is_autoselect;
    bool is_forced;
    char *instream_id;
    char **characteristics;
    int channels;
};

struct variant_playlist {
    // The URI the media playlist will be retrieved from - Required
    char *uri;

    /* EXT-X-STREAM-INF */
    int max_bandwidth;
    int average_bandwidth;
    char **codecs;
    int width;
    int height;
    int framerate;
    int hdcp_level;
    struct variant_rendition **renditions;
};

struct i_frame_playlist {
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
};

struct session_data {
    char *data_id;

    // Should contain value or uri, not both
    char *value;
    char *uri; // Resource must be formatted in JSON

    char *language;
};

/* Media Segment Playlists */

enum key_method {
    NONE,
    AES128,
    SAMPLEAES
};

enum media_playlist_type {
    EVENT, // Media Segments can be added onto the end of the playlist
    VOD // The Media Playlist cannot change
};

struct media_init_section {
    char *uri;

    int subrange_length;
    int subrange_offset;
};

struct media_segment {
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

    /* #EXT-X-MAP */
    struct media_init_section *init_section;

    /* #EXT-X-PROGRAM-DATE-TIME */
    // An absolute date-time associated with the first sample of the segment.
    time_t date_time;

    /* #EXT-X-DATERANGE */
    // TODO: Find a way of accurately representing this info

    /* #EXT-X-MEDIA-SEQUENCE */
    // The Media Sequence Number of the media segment
    int media_sequence;
};

struct playlist {
    enum playlist_type type;

    /* EXT-X-INDEPENDENT-SEGMENTS */
    bool independent_segments;
    /* EXT-X-START */
    // The time a playlist should start at. Positive is from the start of the segments, Negative is from the end of the segments. If |time| > total duration, start at end if positive or beginning if negative.
    float start_time_offset;
    // If true, should "cut up" media segments to start precisely. If not, start at the start of the closest segment.
    bool start_precise;


    // Only present when type == MASTER_PLAYLIST
    struct variant_playlist *variant_playlists;
    /* EXT-X-SESSION-DATA */
    struct session_data *session_data;
    /* EXT-X-SESSION-KEY */
    // TODO: Complete implementation of keys


    // Only present when type == MEDIA_PLAYLIST
    struct media_segment *media_segments;
    /* EXT-X-TARGETDURATION */
    // A maximum length of each media segment, in seconds, rounded to the nearest integer.
    int target_duration;
    /* EXT-X-DISCONTINUITY-SEQUENCE */
    // TODO: See 6.2.1 and 6.2.2
    //TODO: Stop parsing on EXT-X-ENDLIST
    /* EXT-X-PLAYLIST-TYPE*/
    enum media_playlist_type media_type;
    /* EXT-X-I-FRAMES-ONLY */
    bool i_frames_only;


    // Parsing helpers
    int _parse_token;
    int _parse_index;
    void *_latest_token;
};

struct playlist *create_playlist();

void parse_playlist_chunk(struct playlist *playlist, const char *buffer, int size);
