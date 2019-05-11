#pragma once

#include "m3u8-mastertypes.h"
#include "m3u8-mediatypes.h"

enum playlist_type {
    UNKNOWN,
    MASTER_PLAYLIST,
    MEDIA_PLAYLIST
};

typedef struct {
    enum playlist_type type;

    /* EXT-X-INDEPENDENT-SEGMENTS */
    bool independent_segments;
    /* EXT-X-START */
    // The time a playlist should start at. Positive is from the start of the segments, Negative is from the end of the segments. If |time| > total duration, start at end if positive or beginning if negative.
    float start_time_offset;
    // If true, should "cut up" media segments to start precisely. If not, start at the start of the closest segment.
    bool start_precise;


    // Only present when type == MASTER_PLAYLIST
    variant_playlist *first_variant_playlist;
    /* EXT-X-SESSION-DATA */
    session_data *first_session_data;
    /* EXT-X-SESSION-KEY */
    // TODO: Complete implementation of keys
    /* EXT-X-MEDIA */
    variant_rendition *first_variant_rendition;


    // Only present when type == MEDIA_PLAYLIST
    media_segment *first_media_segment;
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
} playlist;
