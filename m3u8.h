/* m3u8.h -- a fast parser for the m3u8 format, following RFC 8216.
 * See https://github.com/WalkerKnapp/m3u8
 */

#pragma once

#include "master-types.h"
#include "media-types.h"

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

typedef void M3U8;

void m3u8_setopt_allow_custom_tags(M3U8* m3u8, bool allowed);
void m3u8_setopt_strict_attribute_lists(M3U8* m3u8, bool strict);

M3U8 *create_m3u8();

playlist *create_playlist(M3U8* m3u8);

void parse_playlist_chunk(M3U8* m3u8, playlist *playlist, const char *buffer, int size);

playlist *parse_playlist(M3U8 *m3u8, const char *in);

variant_playlist *first_variant_playlist(playlist *playlist);
variant_playlist *next_variant_playlist(variant_playlist *variant);
variant_rendition *first_variant_rendition_match(playlist *playlist, variant_playlist *variant);
variant_rendition *next_variant_rendition_match(variant_rendition *rendition);
