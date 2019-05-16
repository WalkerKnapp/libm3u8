#include "hash.h"
#include "m3u8.h"

const HASH hash(const char *str) {
    HASH hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5u) + hash) + c;
    return hash;
}

enum playlist_type get_m3u8_tag_type(HASH hash) {
    switch(hash) {
        case EXTM3U:
        case EXT_X_VERSION:
            return UNKNOWN;
        case EXTINF:
        case EXT_X_BYTERANGE:
        case EXT_X_DISCONTINUITY:
        case EXT_X_KEY:
        case EXT_X_MAP:
        case EXT_X_PROGRAM_DATE_TIME:
        case EXT_X_DATERANGE:
            return MEDIA_PLAYLIST;
        case EXT_X_TARGETDURATION:
        case EXT_X_MEDIA_SEQUENCE:
        case EXT_X_DISCONTINUITY_SEQUENCE:
        case EXT_X_ENDLIST:
        case EXT_X_PLAYLIST_TYPE:
        case EXT_X_I_FRAMES_ONLY:
            return MEDIA_PLAYLIST;
        case EXT_X_MEDIA:
        case EXT_X_STREAM_INF:
        case EXT_X_I_FRAME_STREAM_INF:
        case EXT_X_SESSION_DATA:
        case EXT_X_SESSION_KEY:
            return MASTER_PLAYLIST;
        case EXT_X_INDEPENDENT_SEGMENTS:
        case EXT_X_START:
            return UNKNOWN;
        default:
            return UNKNOWN;
    }
}

bool m3u8_is_tag_custom(HASH hash) {
    switch(hash) {
        case EXTM3U:
        case EXT_X_VERSION:
        case EXTINF:
        case EXT_X_BYTERANGE:
        case EXT_X_DISCONTINUITY:
        case EXT_X_KEY:
        case EXT_X_MAP:
        case EXT_X_PROGRAM_DATE_TIME:
        case EXT_X_DATERANGE:
        case EXT_X_TARGETDURATION:
        case EXT_X_MEDIA_SEQUENCE:
        case EXT_X_DISCONTINUITY_SEQUENCE:
        case EXT_X_ENDLIST:
        case EXT_X_PLAYLIST_TYPE:
        case EXT_X_I_FRAMES_ONLY:
        case EXT_X_MEDIA:
        case EXT_X_STREAM_INF:
        case EXT_X_I_FRAME_STREAM_INF:
        case EXT_X_SESSION_DATA:
        case EXT_X_SESSION_KEY:
        case EXT_X_INDEPENDENT_SEGMENTS:
        case EXT_X_START:
            return false;
        default:
            return true;
    }
}

