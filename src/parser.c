#include "../include/internal-types.h"

#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#define M3U8_TAG_MAX_SIZE 128
#define M3U8_URI_MAX_SIZE 1024
#define M3U8_ATTRIBUTE_MAX_SIZE 128

//TODO: Figure out why this is broken
#define cap_string(size, buf) buf[size] = 0x0; //buf = realloc(buf, size + 1)

#define PARSE_ENUM_START while (i < size) { \
                                        c = buffer[i]; \
                                        if (c == '\n' || c == '\r' || c == ',') { \
                                            switch (m_internal->attribute_enum_hash) { \
                                                case
#define PARSE_ENUM_BREAK break; \
                                                case
#define PARSE_ENUM_FINISH(sequence_reset) break; \
                                                default: \
                                                    fprintf(stderr, "[m3u8] Invalid file format: Unexpected enum value, h:%lu. Offset: %i\n", m_internal->attribute_enum_hash, i); \
                                                    return; \
                                            } \
\
                                            if (c == ',') { \
                                                m_internal->_attribute_sequence = sequence_reset; \
                                                m_internal->attribute_name_hash = HASH_START_VALUE; \
                                                i++; \
                                                break; \
                                            } else { \
                                                m_internal->_parse_token = START; \
                                                m_internal->_parse_index = 0; \
                                                break; \
                                            } \
                                        } else { \
                                            hash_char(m_internal->attribute_enum_hash, c); \
                                            i++; \
                                        } \
                                    }

media_segment *create_latest_media_segment(M3U8_internal *m_internal, playlist *playlist) {
    if(m_internal->_latest_content_token == NULL) {
        m_internal->_latest_content_token = calloc(1, sizeof(media_segment));
        // TODO: Make this more efficient
        media_segment *prev = playlist->first_media_segment;
        if (prev == NULL) {
            playlist->first_media_segment = m_internal->_latest_content_token;
        } else {
            while(prev->next_media_segment != NULL) {
                prev = prev->next_media_segment;
            }
            prev->next_media_segment = m_internal->_latest_content_token;
            ((media_segment *)m_internal->_latest_content_token)->media_sequence = prev->media_sequence + 1;
        }
        return m_internal->_latest_content_token;
    } else {
        return m_internal->_latest_content_token;
    }
}

variant_playlist *create_latest_variant_playlist(M3U8_internal *m_internal, playlist *playlist) {
    if(m_internal->_latest_content_token == NULL) {
        m_internal->_latest_content_token = calloc(1, sizeof(variant_playlist));
        ((variant_playlist *)m_internal->_latest_content_token)->max_bandwidth = 0;
        ((variant_playlist *)m_internal->_latest_content_token)->average_bandwidth = 0;
        // TODO: Make this more efficient
        variant_playlist *prev = playlist->first_variant_playlist;
        if (prev == NULL) {
            playlist->first_variant_playlist = m_internal->_latest_content_token;
        } else {
            while(prev->next_variant_playlist != NULL) {
                prev = prev->next_variant_playlist;
            }
            prev->next_variant_playlist = m_internal->_latest_content_token;
        }
        return m_internal->_latest_content_token;
    } else {
        return m_internal->_latest_content_token;
    }
}

int verify_playlist_type(playlist *playlist, enum playlist_type type, int i) {
    if(playlist->type == UNKNOWN) {
        debug_print("[m3u8] Setting playlist type. Offset: %i\n", i);
        playlist->type = type;
    } else if (playlist->type != type) {
        if(type == MEDIA_PLAYLIST){
            fprintf(stderr, "[m3u8] Parsed a media playlist only tag in a master playlist. Offset: %i\n", i);
        } else {
            fprintf(stderr, "[m3u8] Parsed a master playlist only tag in a media playlist. Offset: %i\n", i);
        }
        return -1;
    }
    return 0;
}

int parse_attribute_name(M3U8_internal *m_internal, char *read, int *readinc, const char *buffer, int buffersize, int reset_attribute_sequence) {
    while (*readinc < buffersize) {
        *read = buffer[*readinc];
        if (*read == '\n' || *read == '\r') {
            fprintf(stderr, "[m3u8] Invalid file format: Unexpected newline in attribute name. Offset: %i\n", *readinc);
            return -1;
        } else if (*read == '=') {
            m_internal->_attribute_sequence = reset_attribute_sequence;
            m_internal->attribute_enum_hash = HASH_START_VALUE;
            m_internal->_parse_index = 0;
            (*readinc)++;
            break;
        } else if (*read == ',') {
            fprintf(stderr, "[m3u8] Invalid file format: Unexpected ',' while parsing attribute name. Offset: %i\n", *readinc);
            return -1;
        } else {
            hash_char(m_internal->attribute_name_hash, *read);
            (*readinc)++;
        }
    }
    return 0;
}

// max_dest_size is in integers
int parse_hex_int(M3U8_internal *m_internal, char *read, int *readinc, const char *buffer, int buffersize, unsigned int** dest, unsigned int max_dest_size, int attribute_sequence_reset) {
    if (m_internal->_parse_index == 0) {
        if(*read != '0') {
            fprintf(stderr, "[m3u8] Invalid file format: Hex Integer does not start with \"0x\". Offset: %i\n", *readinc);
            return -1;
        }
        (*readinc)++;
        *read = buffer[*readinc];
        m_internal->_parse_index = 1;
    }
    if (m_internal->_parse_index == 1) {
        if(*read != 'x') {
            fprintf(stderr, "[m3u8] Invalid file format: Hex Integer does not start with \"0x\". Offset: %i\n", *readinc);
            return -1;
        }
        (*readinc)++;
        *read = buffer[*readinc];
        m_internal->_parse_index = 2;

        *dest = malloc(sizeof(int) * max_dest_size);
    }
    while ((*readinc) < buffersize) {
        *read = buffer[*readinc];
        uint8_t byte = (uint8_t) *read;
        if(*read == '\n' || *read == '\r') {
            (*readinc)++;
            m_internal->_parse_token = START;
            m_internal->_parse_index = 0;
            return 0;
        } else if (*read == ',') {
            m_internal->_attribute_sequence = attribute_sequence_reset;
            m_internal->attribute_name_hash = HASH_START_VALUE;
            (*readinc)++;
            break;
        } else if (isdigit(*read)) {
            byte = byte - '0';
        } else if (byte >= 'A' && byte <='F') {
            byte = byte - 'A' + 10;
        } else {
            fprintf(stderr, "[m3u8] Invalid file format: Hex Integer contains non-hex-digit character. Offset: %i\n", *readinc);
            return -1;
        }

        int i = (m_internal->_parse_index - 2)/ (sizeof(int) * 2);

        if(i >= max_dest_size) {
            fprintf(stderr, "[m3u8] Invalid file format: Hex Integer is too long. Expected to be able to store in %i integers. Offset: %i\n", max_dest_size, *readinc);
            return -1;
        }

        (*dest)[i] = ((*dest)[i] << 4u) | (byte & 0xFu);

        (*readinc)++;
        m_internal->_parse_index++;
    }
    return 0;
}

int parse_quoted_string(M3U8_internal *m_internal, char *read, int *readinc, const char *buffer, int buffersize, char **dest, int attribute_sequence_reset) {
    if (m_internal->_parse_index == 0) {
        *read = buffer[*readinc];
        if(*read != '"') {
            fprintf(stderr, "[m3u8] Invalid file format: Quoted string does not start with '\"', instead starts with '%c'. Offset: %i\n", *read, *readinc);
            return -1;
        } else {
            (*readinc)++;
            m_internal->_parse_index = 1;
        }

        *dest = malloc(M3U8_ATTRIBUTE_MAX_SIZE);
    }
    while ((*readinc) < buffersize) {
        *read = buffer[*readinc];
        //debug_print2("[m3u8] Reading character '%c'. Offset: %i\n", *read, *readinc);
        if(*read == '\n' || *read == '\r') {
            if(m_internal->_parse_index != -1) {
                fprintf(stderr, "[m3u8] Invalid file format: Unexpected newline in quoted string. Offset: %i\n",
                        *readinc);
                return -1;
            }
            (*readinc)++;
            m_internal->_parse_token = START;
            m_internal->_parse_index = 0;
            return 0;
        } else if (*read == '"') {
            cap_string(m_internal->_parse_index - 1, (*dest));
            m_internal->_parse_index = -1;
            (*readinc)++;
            *read = buffer[*readinc];
            if(*read == ',') {
                m_internal->_attribute_sequence = attribute_sequence_reset;
                m_internal->attribute_name_hash = HASH_START_VALUE;
                (*readinc)++;
                break;
            } else {
                fprintf(stderr, "[m3u8] Invalid file format: Expected ',' following '\"', instead got '%c' Offset: %i\n", *read, *readinc);
                return -1;
            }
        } else {
            (*dest)[m_internal->_parse_index - 1] = *read;
            m_internal->_parse_index++;
            (*readinc)++;
        }
    }
    return 0;
}

int parse_quoted_string_as_hash(M3U8_internal *m_internal, char *read, int *readinc, const char *buffer, int buffersize, unsigned long *dest, int attribute_sequence_reset) {
    if (m_internal->_parse_index == 0) {
        *read = buffer[*readinc];
        if(*read != '"') {
            fprintf(stderr, "[m3u8] Invalid file format: Quoted string does not start with '\"' Offset: %i\n", *readinc);
            return -1;
        } else {
            (*readinc)++;
            m_internal->_parse_index = 1;
        }
    }
    while ((*readinc) < buffersize) {
        *read = buffer[*readinc];
        if(*read == '\n' || *read == '\r') {
            if(m_internal->_parse_index != -1) {
                fprintf(stderr, "[m3u8] Invalid file format: Unexpected newline in quoted string. Offset: %i\n",
                        *readinc);
                return -1;
            }
            (*readinc)++;
            m_internal->_parse_token = START;
            m_internal->_parse_index = 0;
            break;
        } else if (*read == '"') {
            m_internal->_parse_index = -1;
            (*readinc)++;
        } else if (m_internal->_parse_index == -1 && *read == ',') {
            m_internal->_attribute_sequence = attribute_sequence_reset;
            m_internal->attribute_name_hash = HASH_START_VALUE;
            (*readinc)++;
            break;
        } else {
            hash_char(*dest, *read);
            m_internal->_parse_index++;
            (*readinc)++;
        }
    }
    return 0;
}

int parse_decimal_integer(M3U8_internal *m_internal, char *read, int *readinc, const char *buffer, int buffersize, long *dest, int attribute_sequence_reset) {
    // TODO: WTF?
    while (*readinc < buffersize) {
        *read = buffer[*readinc];
        debug_print2("[m3u8] Parsed character '%c'. Offset: %i\n", *read, *readinc);
        if(*read == '\n' || *read == '\r') {
            m_internal->_parse_token = START;
            (*readinc)++;
            m_internal->_parse_index = 0;
            break;
        } else if (*read == ',') {
            m_internal->_attribute_sequence = attribute_sequence_reset;
            m_internal->attribute_name_hash = HASH_START_VALUE;
            (*readinc)++;
            break;
        } else if (!isdigit(*read)) {
            fprintf(stderr, "[m3u8] Invalid file format: Non-digit found in decimal integer. Offset: %i\n", *readinc);
            return -1;
        } else {
            *dest *= 10;
            *dest += *read - '0';
            (*readinc)++;
        }
    }
    return 0;
}

int parse_resolution(M3U8_internal *m_internal, char *read, int *readinc, const char *buffer, int buffersize, int *x_dest, int *y_dest, int attribute_sequence_reset) {
    while (*readinc < buffersize) {
        *read = buffer[*readinc];
        if(*read == '\n' || *read == '\r') {
            if(m_internal->_parse_index != 1) {
                fprintf(stderr, "[m3u8] Invalid file format: Unexpected newline in resolution. Offset: %i\n", *readinc);
                return -1;
            } else {
                m_internal->_parse_token = START;
                (*readinc)++;
                m_internal->_parse_index = 0;
                break;
            }
        } else if (*read == ',') {
            if(m_internal->_parse_index != 1) {
                fprintf(stderr, "[m3u8] Invalid file format: Unexpected ',' in resolution. Offset: %i\n", *readinc);
                return -1;
            } else {
                m_internal->_attribute_sequence = attribute_sequence_reset;
                m_internal->attribute_name_hash = HASH_START_VALUE;
                (*readinc)++;
                break;
            }
        } else if (*read == 'x' || *read == 'X') {
            if(m_internal->_parse_index == 0) {
                m_internal->_parse_index = 1;
                (*readinc)++;
            } else {
                fprintf(stderr, "[m3u8] Invalid file format: Unexpected 'x' after initial separator in resolution. Offset: %i\n", *readinc);
                return -1;
            }
        } else if (!isdigit(*read)) {
            fprintf(stderr, "[m3u8] Invalid file format: Non-digit found in resolution. Offset: %i\n", *readinc);
            return -1;
        } else {
            if(m_internal->_parse_index == 0) {
                *x_dest *= 10;
                *x_dest += *read - '0';
                (*readinc)++;
            } else if (m_internal->_parse_index == 1) {
                *y_dest *= 10;
                *y_dest += *read - '0';
                (*readinc)++;
            } else {
                fprintf(stderr, "[m3u8] Invalid file format: Invalid parse_index state while parsing resolution: %i. Offset: %i\n", m_internal->_parse_index, *readinc);
                return -1;
            }
        }
    }

    return 0;
}

int parse_decimal_float(M3U8_internal *m_internal, char *read, int *readinc, const char *buffer,
                        int buffersize, float *dest, int attribute_sequence_reset) {
    while (*readinc < buffersize) {
        *read = buffer[*readinc];
        if(*read == '\n' || *read == '\r') {
            m_internal->_attribute_sequence = attribute_sequence_reset;
            m_internal->_parse_token = START;
            (*readinc)++;
            m_internal->_parse_index = 0;
            return 0;
        } else if (*read == ',') {
            m_internal->_attribute_sequence = attribute_sequence_reset;
            m_internal->attribute_name_hash = HASH_START_VALUE;
            (*readinc)++;
            break;
        } else if (*read == '.') {
            if(m_internal->_parse_index == 0) {
                m_internal->_parse_index = 1;
                (*readinc)++;
            } else {
                fprintf(stderr, "[m3u8] Invalid file format: Unexpected '.' after initial separator in float. Offset: %i\n", *readinc);
                return -1;
            }
        } else if (!isdigit(*read)) {
            fprintf(stderr, "[m3u8] Invalid file format: Non-digit found in float. Offset: %i\n", *readinc);
            return -1;
        } else {
            if(m_internal->_parse_index == 0) {
                *dest *= 10;
                *dest += *read - '0';
                (*readinc)++;
            } else {
                *dest += ((float)(*read) - '0')/pow(10, m_internal->_parse_index);
                m_internal->_parse_index++;
                (*readinc)++;
            }
        }
    }
    return 0;
}


int skip_attribute(M3U8_internal *m_internal, char *read, int *readinc, const char *buffer, int buffersize, int attribute_sequence_reset) {
    debug_print2("Skipping unknown attribute h:%lu. Offset: %i\n", m_internal->attribute_name_hash, *readinc);
    while ((*readinc) < buffersize) {
        *read = buffer[*readinc];
        if(*read == '\n' || *read == '\r') {
            (*readinc)++;
            m_internal->_parse_token = START;
            m_internal->_parse_index = 0;
            break;
        } else if (*read == ',') {
            m_internal->_attribute_sequence = attribute_sequence_reset;
            m_internal->attribute_name_hash = HASH_START_VALUE;
            (*readinc)++;
            break;
        } else {
            (*readinc)++;
        }
    }
    return 0;
}

void parse_playlist_chunk(M3U8 *m3u8, playlist *playlist, const char *buffer, int size) {
    debug_print("[m3u8] Parsing buffer of size %i\n", size);

    M3U8_internal* m_internal = m3u8;

    for (int i = 0; i < size; i++) {
        char c = buffer[i];

        // See if the playlist is being parsed from the beginning of or partly through a line
        if(m_internal->_parse_token == START) {
            debug_print("[m3u8] Parsing at START parse token. Offset: %i\n", i);
            switch (c) {
                // Ignore newlines, this line is empty
                case '\n':
                case '\t':
                    debug_print("[m3u8] Skipping, newline. Offset: %i\n", i);
                    break;
                    // Line is a tag
                case '#':
                    debug_print("[m3u8] Reading as a tag. Offset: %i\n", i);
                    m_internal->_parse_token = TAG;
                    if(m_internal->allow_custom_tags) {
                        m_internal->_parse_buffer = malloc(M3U8_TAG_MAX_SIZE);
                    }
                    m_internal->tag_hash = HASH_START_VALUE;
                    m_internal->_parse_index = 0;
                    break;
                    // Line is a URI
                default:
                    debug_print("[m3u8] Reading as a URI. Offset: %i\n", i);
                    m_internal->_parse_token = URI;
                    // The latest parsed token *should* be a URI object with an empty buffer as URI, created by EXT-X-STREAM-INF or EXTINF
                    // If not, the m3u8 is invalid or custom.

                    if(m_internal->_latest_content_token == NULL) {
                        fprintf(stderr, "[m3u8] Invalid file format: no EXT-X-STREAM-INF or EXTINF preceding URI. Offset: %i\n", i);
                        return;
                    }

                    if(playlist->type == MASTER_PLAYLIST && m_internal->_latest_content_token != NULL) {
                        m_internal->_parse_buffer = malloc(M3U8_URI_MAX_SIZE);
                        ((variant_playlist *)m_internal->_latest_content_token)->uri = m_internal->_parse_buffer;
                        debug_print("[m3u8] Parsing URI as a variant_playlist. Offset: %i\n", i);
                    } else if (playlist->type == MEDIA_PLAYLIST && m_internal->_latest_content_token != NULL) {
                        m_internal->_parse_buffer = malloc(M3U8_URI_MAX_SIZE);
                        ((media_segment *)m_internal->_latest_content_token)->uri = m_internal->_parse_buffer;
                        debug_print("[m3u8] Parsing URI as a media_segment. Offset: %i\n", i);
                    } else {
                        fprintf(stderr, "[m3u8] Invalid file format: Parsed URI before EXTINF or EXT-X-STREAM-INF tag. Offset: %i\n", i);
                        return;
                    }
                    m_internal->_parse_buffer[0] = c;
                    m_internal->_parse_index = 1;
            }
        } else if (m_internal->_parse_token == TAG) {
            debug_print("[m3u8] Parsing at TAG parse token. Offset: %i\n", i);
            if(m_internal->allow_custom_tags){
                /*while(i < size) {
                    c = buffer[i];
                    if(c == '\n' || c == '\r') {
                        // Tag is over. Assign it to its location
                        if(m_internal->_parse_index > 3) {

                        } // Otherwise a very short comment, ignore
                    } else if (c == ':') {

                    } else {
                        m_internal->_parse_buffer[m_internal->_parse_index] = c;
                        m_internal->tag_hash = ((m_internal->tag_hash << 5) + m_internal->tag_hash) + c;
                        m_internal->_parse_index++;
                        i++;

                        if(m_internal->_parse_index == 3 && m_internal->tag_hash != EXT) {
                            debug_print("[m3u8] Skipping comment. Offset: %i\n", i);
                            m_internal->_parse_token = TAG_SKIPPING;
                            break;
                        }
                    }
                }*/
                // TODO: Custom Tags Implementation
            } else {
                while(i < size) {
                    c = buffer[i];
                    //debug_print("[m3u8] Parsing character %c as a tag.\n", c);
                    if(c == '\n' || c == '\r') {
                        debug_print("[m3u8] Tag is over. Offset: %i\n", i);
                        // Tag is over. Assign it to its location
                        if(m_internal->_parse_index > 3) {
                            enum playlist_type tag_type = get_m3u8_tag_type(m_internal->tag_hash);
                            if(tag_type == MASTER_PLAYLIST && playlist->type == MEDIA_PLAYLIST) {
                                fprintf(stderr, "[m3u8] Invalid file format: Parsed tag %lu, a master playlist tag, when parsing a media playlist. Offset: %i\n", m_internal->tag_hash, i);
                                return;
                            } else if (tag_type == MEDIA_PLAYLIST && playlist->type == MASTER_PLAYLIST) {
                                fprintf(stderr, "[m3u8] Invalid file format: Parsed tag %lu, a media playlist tag, when parsing a master playlist. Offset: %i\n", m_internal->tag_hash, i);
                                return;
                            }

                            // Handle tags that have no parameters now
                            switch (m_internal->tag_hash) {
                                // General Tags
                                case EXTM3U:
                                    break;
                                    // Media Segment Tags
                                case EXT_X_DISCONTINUITY:
                                    verify_playlist_type(playlist, MEDIA_PLAYLIST, i);
                                    create_latest_media_segment(m_internal, playlist)->discontinuity = true;
                                    break;
                                    // Media Playlist Tags
                                case EXT_X_ENDLIST:
                                    verify_playlist_type(playlist, MEDIA_PLAYLIST, i);
                                    // TODO: Maybe lock playlist after EXT-X-ENDLIST?
                                    break;
                                case EXT_X_I_FRAMES_ONLY:
                                    verify_playlist_type(playlist, MEDIA_PLAYLIST, i);
                                    playlist->i_frames_only = true;
                                    break;
                                    // Master Playlist Tags
                                case EXT_X_INDEPENDENT_SEGMENTS:
                                    playlist->independent_segments = true;
                                    break;
                                default:
                                    // Tag is a custom tag with no parameters
                                    break;
                            }
                        } // Otherwise a very short comment, ignore
                        m_internal->_parse_token = START;
                        m_internal->_parse_index = 0;
                        debug_print("[m3u8] Finished tag parsing and assignment. Offset: %i\n", i);
                        break;
                    } else if (c == ':') {

                        enum playlist_type tag_type = get_m3u8_tag_type(m_internal->tag_hash);
                        if(tag_type == MASTER_PLAYLIST && playlist->type == MEDIA_PLAYLIST) {
                            fprintf(stderr, "[m3u8] Invalid file format: Parsed tag %lu, a master playlist tag, when parsing a media playlist. Offset: %i\n", m_internal->tag_hash, i);
                            return;
                        } else if (tag_type == MEDIA_PLAYLIST && playlist->type == MASTER_PLAYLIST) {
                            fprintf(stderr, "[m3u8] Invalid file format: Parsed tag %lu, a media playlist tag, when parsing a master playlist. Offset: %i\n", m_internal->tag_hash, i);
                            return;
                        }

                        if(m3u8_is_tag_custom(m_internal->tag_hash)) {
                            debug_print2("[m3u8] Skipping tag h:%lu. Offset: %i\n", m_internal->tag_hash, i);
                            m_internal->_parse_token = TAG_SKIPPING;
                            // TODO: Finish Custom Tags implementation
                        } else {
                            m_internal->_attribute_sequence = 0;
                            m_internal->attribute_name_hash = HASH_START_VALUE;
                            m_internal->_parse_token = ATTRIBUTE;
                        }
                        break;
                    } else {
                        hash_char(m_internal->tag_hash, c);
                        m_internal->_parse_index++;
                        i++;

                        if(m_internal->_parse_index == 3 && m_internal->tag_hash != EXT) {
                            debug_print("[m3u8] Skipping comment. Offset: %i\n", i);
                            m_internal->_parse_token = TAG_SKIPPING;
                            break;
                        }
                    }
                }
            }
        } else if (m_internal->_parse_token == TAG_SKIPPING) {
            debug_print("[m3u8] Parsing at TAG_SKIPPING parse token. Offset: %i\n", i);
            while (i < size) {
                c = buffer[i];
                if(c == '\n' || c == '\r') {
                    m_internal->_parse_token = START;
                    m_internal->_parse_index = 0;
                    break;
                }
                i++;
            }
        } else if (m_internal->_parse_token == ATTRIBUTE) {
            debug_print("[m3u8] Parsing at ATTRIBUTE parse token. Offset: %i\n", i);
            switch (m_internal->tag_hash) {
                // Media Segment Tags
                case EXTINF:
                    // Format: <duration(float/int)>,[<title(non-quoted-str)>]
                    verify_playlist_type(playlist, MEDIA_PLAYLIST, i);
                    media_segment *inf_segment = create_latest_media_segment(m_internal, playlist);
                    if(m_internal->_attribute_sequence == 0) {
                        while (i < size) {
                            c = buffer[i];
                            if(c == '\n' || c == '\r') {
                                m_internal->_parse_token = START;
                                i++;
                                m_internal->_parse_index = 0;
                                break;
                            } else if (c == ',') {
                                inf_segment->title = malloc(M3U8_ATTRIBUTE_MAX_SIZE);
                                m_internal->_parse_index = 0;
                                m_internal->_attribute_sequence = 2;
                                i++;
                                break;
                            } else if (c == '.') {
                                m_internal->_parse_index = 1;
                                m_internal->_attribute_sequence = 1;
                                i++;
                                break;
                            } else if (!isdigit(c)) {
                                fprintf(stderr, "[m3u8] Invalid file format: Non-digit found in duration of EXTINF. Offset: %i\n", i);
                                return;
                            } else {
                                inf_segment->duration *= 10;
                                inf_segment->duration += c - '0';
                                i++;
                            }
                        }
                    }
                    if (m_internal->_attribute_sequence == 1) {
                        while (i < size) {
                            c = buffer[i];
                            if (c == '\n' || c == '\r') {
                                m_internal->_parse_token = START;
                                i++;
                                m_internal->_parse_index = 0;
                                break;
                            } else if (c == ',') {
                                inf_segment->title = malloc(M3U8_ATTRIBUTE_MAX_SIZE);
                                m_internal->_parse_index = 0;
                                m_internal->_attribute_sequence = 2;
                                i++;
                                break;
                            } else if (!isdigit(c)) {
                                fprintf(stderr, "[m3u8] Invalid file format: Non-digit found in duration of EXTINF. Offset: %i\n", i);
                                return;
                            }

                            inf_segment->duration += ((float)c - '0')/pow(10, m_internal->_parse_index);

                            m_internal->_parse_index++;
                            i++;
                        }
                    }
                    if (m_internal->_attribute_sequence == 2) {
                        while(i < size) {
                            c = buffer[i];
                            if (c == '\n' || c == '\r') {
                                cap_string(m_internal->_parse_index, inf_segment->title);
                                m_internal->_parse_token = START;
                                m_internal->_parse_index = 0;
                                i++;
                                break;
                            }
                            inf_segment->title[m_internal->_parse_index] = c;

                            i++;
                            m_internal->_parse_index++;
                        }
                    }
                    break;
                case EXT_X_KEY:
                    verify_playlist_type(playlist, MEDIA_PLAYLIST, i);
                    media_segment *key_segment = create_latest_media_segment(m_internal, playlist);
                    while (i < size) {
                        if (m_internal->_attribute_sequence == 0) {
                            // Parse up the attribute name
                            if(parse_attribute_name(m_internal, &c, &i, buffer, size, 1) != 0) {
                                return;
                            }
                        }
                        if (m_internal->_attribute_sequence == 1) {
                            // Parse the attribute value
                            switch (m_internal->attribute_name_hash) {
                                case KEY_ATTRIBUTE_METHOD:
                                    PARSE_ENUM_START
                                                    KEY_ATTRIBUTE_METHOD_NONE:
                                                    key_segment->key_method = KEY_METHOD_NONE;
                                                    PARSE_ENUM_BREAK
                                                    KEY_ATTRIBUTE_METHOD_AES_128:
                                                    key_segment->key_method = KEY_METHOD_AES128;
                                                    PARSE_ENUM_BREAK
                                                    KEY_ATTRIBUTE_METHOD_SAMPLE_AES:
                                                    key_segment->key_method = KEY_METHOD_SAMPLEAES;
                                    PARSE_ENUM_FINISH(0)
                                    break;
                                case KEY_ATTRIBUTE_URI:
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &key_segment->key_uri, 0) != 0) {
                                        return;
                                    }
                                    break;
                                case KEY_ATTRIBUTE_IV:
                                    if(parse_hex_int(m_internal, &c, &i, buffer, size, &key_segment->key_iv, (128/8)/sizeof(int), 0) != 0) {
                                        return;
                                    }
                                    break;
                                case KEY_ATTRIBUTE_KEYFORMAT:
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &key_segment->key_format, 0) != 0) {
                                        return;
                                    }
                                    break;
                                case KEY_ATTRIBUTE_KEYFORMATVERSIONS:
                                    if (parse_quoted_string(m_internal, &c, &i, buffer, size, &key_segment->key_format_versions, 0) != 0) {
                                        return;
                                    }
                                    break;
                                default:
                                    if(m_internal->strict_attribute_lists) {
                                        fprintf(stderr,
                                                "[m3u8] Invalid file format: Unknown EXT-X-KEY attribute. Offset: %i\n",
                                                i);
                                        return;
                                    } else {
                                        skip_attribute(m_internal, &c, &i, buffer, size, 0);
                                    }
                            }
                        }
                    }
                case EXT_X_MAP:
                    verify_playlist_type(playlist, MEDIA_PLAYLIST, i);
                    if(m_internal->_attribute_sequence == 0) {
                        m_internal->latest_media_init_section = malloc(sizeof(media_init_section));
                        m_internal->_attribute_sequence = 1;
                    }
                    media_init_section *init_section = m_internal->latest_media_init_section;
                    while (i < size) {
                        if (m_internal->_attribute_sequence == 1) {
                            if(parse_attribute_name(m_internal, &c, &i, buffer, size, 2) != 0) {
                                return;
                            }
                        }
                        if (m_internal->_attribute_sequence == 2) {
                            switch (m_internal->attribute_name_hash) {
                                case MAP_ATTRIBUTE_URI:
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &init_section->uri, 1) != 0) {
                                        return;
                                    }
                                    break;
                                case MAP_ATTRIBUTE_BYTERANGE:
                                    while (i < size) {
                                        c = buffer[i];
                                        if(c == '\n' || c == '\r') {
                                            m_internal->_parse_token = START;
                                            i++;
                                            m_internal->_parse_index = 0;
                                            break;
                                        } else if (c == '@') {
                                            m_internal->_attribute_sequence = 3;
                                            i++;
                                            break;
                                        } else if (!isdigit(c)) {
                                            fprintf(stderr, "[m3u8] Invalid file format: Non-digit found in byterange of EXT-X-MAP. Offset: %i\n", i);
                                            return;
                                        } else {
                                            init_section->subrange_length *= 10;
                                            init_section->subrange_length += c - '0';
                                            i++;
                                        }
                                    }
                                    break;
                                default:
                                    if(m_internal->strict_attribute_lists) {
                                        fprintf(stderr,
                                                "[m3u8] Invalid file format: Unknown EXT-X-MAP attribute. Offset: %i\n",
                                                i);
                                        return;
                                    } else {
                                        skip_attribute(m_internal, &c, &i, buffer, size, 0);
                                    }
                            }
                        }
                        if (m_internal->_attribute_sequence == 3) {
                            // Parsing the offset portion of the byterange attribute
                            while (i < size) {
                                c = buffer[i];
                                if(c == '\n' || c == '\r') {
                                    m_internal->_parse_token = START;
                                    i++;
                                    m_internal->_parse_index = 0;
                                    break;
                                } else if (!isdigit(c)) {
                                    fprintf(stderr, "[m3u8] Invalid file format: Non-digit found in byterange of EXT-X-MAP. Offset: %i\n", i);
                                    return;
                                } else {
                                    init_section->subrange_offset *= 10;
                                    init_section->subrange_offset += c - '0';
                                    i++;
                                }
                            }
                        }
                    }
                    break;
                case EXT_X_PROGRAM_DATE_TIME:
                    verify_playlist_type(playlist, MEDIA_PLAYLIST, i);
                    m_internal->_parse_token = TAG_SKIPPING;
                    // TODO: Are there no good datetime apis for c?
                case EXT_X_DATERANGE:
                    verify_playlist_type(playlist, MEDIA_PLAYLIST, i);
                    m_internal->_parse_token = TAG_SKIPPING;
                    // TODO: Find a way to accurately represent this data.
                    break;
                    // Media Playlist Tags
                case EXT_X_TARGETDURATION:
                    verify_playlist_type(playlist, MEDIA_PLAYLIST, i);
                    if (m_internal->_attribute_sequence == 0) {
                        playlist->target_duration = 0;
                        m_internal->_attribute_sequence = 1;
                    }
                    while (i < size) {
                        c = buffer[i];
                        if (c == '\n' || c == '\r') {
                            m_internal->_parse_token = START;
                            i++;
                            m_internal->_parse_index = 0;
                            break;
                        } else if (!isdigit(c)) {
                            fprintf(stderr, "[m3u8] Invalid file format: Non-digit found in EXT-X-TARGETDURATION. Offset: %i\n", i);
                            return;
                        }

                        playlist->target_duration *= 10;
                        playlist->target_duration += c - '0';

                        i++;
                    }
                    break;
                case EXT_X_MEDIA_SEQUENCE:
                    verify_playlist_type(playlist, MEDIA_PLAYLIST, i);
                    media_segment *m_sequence_segment = playlist->first_media_segment;
                    if(m_internal->_attribute_sequence == 0) {
                        if (m_sequence_segment != NULL) {
                            fprintf(stderr, "[m3u8] Invalid file format: EXT-X-MEDIA-SEQUENCE appears after the first media segment. Offset: %i\n", i);
                            return;
                        }
                        m_sequence_segment = malloc(sizeof(media_segment));
                        m_internal->_latest_content_token = m_sequence_segment;
                        playlist->first_media_segment = m_sequence_segment;
                        m_internal->_attribute_sequence = 1;
                    }
                    while (i < size) {
                        c = buffer[i];
                        if (c == '\n' || c == '\r') {
                            m_internal->_parse_token = START;
                            i++;
                            m_internal->_parse_index = 0;
                            break;
                        } else if (!isdigit(c)) {
                            fprintf(stderr, "[m3u8] Invalid file format: Non-digit found in EXT-X-MEDIA-SEQUENCE. Offset: %i\n", i);
                            return;
                        }

                        m_sequence_segment->media_sequence *= 10;
                        m_sequence_segment += c - '0';

                        i++;
                    }
                    break;
                case EXT_X_DISCONTINUITY_SEQUENCE:
                    verify_playlist_type(playlist, MEDIA_PLAYLIST, i);
                    m_internal->_parse_token = TAG_SKIPPING;
                    // TODO: See where this applies
                    break;
                case EXT_X_PLAYLIST_TYPE:
                    verify_playlist_type(playlist, MEDIA_PLAYLIST, i);
                    while (i < size) {
                        c = buffer[i];
                        if (c == '\n' || c == '\r') {
                            switch (m_internal->attribute_enum_hash) {
                                case PLAYLIST_TYPE_ATTRIBUTE_EVENT:
                                    playlist->media_type = EVENT;
                                    break;
                                case PLAYLIST_TYPE_ATTRIBUTE_VOD:
                                    playlist->media_type = VOD;
                                    break;
                                default:
                                    fprintf(stderr, "[m3u8] Invalid file format: Could not interpret token h:%lu as EXT-X-PLAYLIST-TYPE. Offset: %i\n", m_internal->attribute_enum_hash, i);
                                    return;
                            }

                            m_internal->_parse_token = START;
                            i++;
                            m_internal->_parse_index = 0;
                            break;
                        }

                        hash_char(m_internal->attribute_enum_hash, c);
                        i++;
                    }
                    // Master Playlist Tags
                case EXT_X_MEDIA:
                    debug_print("[m3u8] Parsing EXT-X-MEDIA tag attributes. Offset: %i\n", i);
                    verify_playlist_type(playlist, MASTER_PLAYLIST, i);
                    variant_rendition *media_variant_rendition;
                    if(m_internal->_attribute_sequence == 0) {
                        media_variant_rendition = malloc(sizeof(media_variant_rendition));
                        media_variant_rendition->group_id_hash = HASH_START_VALUE;
                        m_internal->_attribute_sequence = 1;
                    } else {
                        // TODO: be more efficient with this
                        media_variant_rendition = playlist->first_variant_rendition;
                        while(media_variant_rendition->next_variant_rendition != NULL) {
                            media_variant_rendition = media_variant_rendition->next_variant_rendition;
                        }
                    }
                    while (i < size && m_internal->_parse_token == ATTRIBUTE) {
                        if (m_internal->_attribute_sequence == 1) {
                            debug_print("[m3u8] Parsing EXT-X-MEDIA tag attribute name. Offset: %i\n", i);
                            // Parse up the attribute name
                            if (parse_attribute_name(m_internal, &c, &i, buffer, size, 2) != 0) {
                                return;
                            }
                            //debug_print("[m3u8] Finished parsing EXT-X-MEDIA tag attribute name. Offset: %i\n", i);
                        }
                        if (m_internal->_attribute_sequence == 2) {
                            debug_print("[m3u8] Parsing EXT-X-MEDIA tag attribute value. Offset: %i\n", i);
                            // Parse the attribute value
                            switch (m_internal->attribute_name_hash) {
                                case MEDIA_ATTRIBUTE_TYPE:
                                    PARSE_ENUM_START
                                                    MEDIA_ATTRIBUTE_TYPE_AUDIO:
                                                    media_variant_rendition->type = AUDIO;
                                                    PARSE_ENUM_BREAK
                                                    MEDIA_ATTRIBUTE_TYPE_VIDEO:
                                                    media_variant_rendition->type = VIDEO;
                                                    PARSE_ENUM_BREAK
                                                    MEDIA_ATTRIBUTE_TYPE_SUBTITLES:
                                                    media_variant_rendition->type = SUBTITLES;
                                                    PARSE_ENUM_BREAK
                                                    MEDIA_ATTRIBUTE_TYPE_CLOSED_CAPTIONS:
                                                    media_variant_rendition->type = CLOSED_CAPTIONS;
                                    PARSE_ENUM_FINISH(1)
                                    break;
                                case MEDIA_ATTRIBUTE_URI:
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &media_variant_rendition->uri, 1) != 0) {
                                        return;
                                    }
                                    break;
                                case MEDIA_ATTRIBUTE_GROUP_ID:
                                    if(parse_quoted_string_as_hash(m_internal, &c, &i, buffer, size, &media_variant_rendition->group_id_hash, 1) != 0) {
                                        return;
                                    }
                                    break;
                                case MEDIA_ATTRIBUTE_LANGUAGE:
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &media_variant_rendition->language, 1) != 0) {
                                        return;
                                    }
                                    break;
                                case MEDIA_ATTRIBUTE_ASSOC_LANGUAGE:
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &media_variant_rendition->assoc_lang, 1) != 0) {
                                        return;
                                    }
                                    break;
                                case MEDIA_ATTRIBUTE_NAME:
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &media_variant_rendition->name, 1) != 0) {
                                        return;
                                    }
                                    break;
                                case MEDIA_ATTRIBUTE_DEFAULT:
                                    PARSE_ENUM_START
                                                    MEDIA_ATTRIBUTE_YES:
                                                    media_variant_rendition->is_default = true;
                                                    PARSE_ENUM_BREAK
                                                    MEDIA_ATTRIBUTE_NO:
                                                    media_variant_rendition->is_default = false;
                                    PARSE_ENUM_FINISH(1)
                                    break;
                                case MEDIA_ATTRIBUTE_AUTOSELECT:
                                    PARSE_ENUM_START
                                                    MEDIA_ATTRIBUTE_YES:
                                                    media_variant_rendition->is_autoselect = true;
                                                    PARSE_ENUM_BREAK
                                                    MEDIA_ATTRIBUTE_NO:
                                                    media_variant_rendition->is_autoselect = false;
                                    PARSE_ENUM_FINISH(1)
                                    break;
                                case MEDIA_ATTRIBUTE_FORCED:
                                    PARSE_ENUM_START
                                                    MEDIA_ATTRIBUTE_YES:
                                                    media_variant_rendition->is_forced = true;
                                                    PARSE_ENUM_BREAK
                                                    MEDIA_ATTRIBUTE_NO:
                                                    media_variant_rendition->is_forced = false;
                                    PARSE_ENUM_FINISH(1)
                                    break;
                                case MEDIA_ATTRIBUTE_INSTREAM_ID:
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &media_variant_rendition->instream_id, 1) != 0) {
                                        return;
                                    }
                                    break;
                                case MEDIA_ATTRIBUTE_CHARACTERISTICS:
                                    // TODO: Parse as a list instead of a string
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, media_variant_rendition->characteristics, 1) != 0) {
                                        return;
                                    }
                                    break;
                                case MEDIA_ATTRIBUTE_CHANNELS:
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &media_variant_rendition->channels, 1) != 0) {
                                        return;
                                    }
                                    break;
                                default:
                                    if(m_internal->strict_attribute_lists) {
                                        fprintf(stderr,
                                                "[m3u8] Invalid file format: Unknown EXT-X-MEDIA attribute. Offset: %i\n",
                                                i);
                                        return;
                                    } else {
                                        skip_attribute(m_internal, &c, &i, buffer, size, 1);
                                    }
                            }
                        }
                        if(m_internal->_parse_token != ATTRIBUTE) {
                            break;
                        }
                    }
                    break;
                case EXT_X_STREAM_INF:
                    debug_print("[m3u8] Parsing EXT-X-STREAM-INF tag attributes. Offset: %i\n", i);
                    verify_playlist_type(playlist, MASTER_PLAYLIST, i);
                    variant_playlist *streaminf_variant_playlist = create_latest_variant_playlist(m_internal, playlist);
                    while (i < size) {
                        c = buffer[i];
                        if (m_internal->_attribute_sequence == 0) {
                            debug_print("[m3u8] Parsing EXT-X-STREAM-INF tag attribute name. Offset: %i\n", i);
                            // Parse up the attribute name
                            if (parse_attribute_name(m_internal, &c, &i, buffer, size, 1) != 0) {
                                return;
                            }
                        }
                        if (m_internal->_attribute_sequence == 1) {
                            debug_print("[m3u8] Parsing EXT-X-STREAM-INF tag attribute value. Offset: %i\n", i);
                            // Parse the attribute value
                            switch (m_internal->attribute_name_hash) {
                                case STREAMINF_ATTRIBUTE_BANDWIDTH:
                                    if(parse_decimal_integer(m_internal, &c, &i, buffer, size, &streaminf_variant_playlist->max_bandwidth, 0) != 0) {
                                        return;
                                    }
                                    break;
                                case STREAMINF_ATTRIBUTE_AVERAGE_BANDWIDTH:
                                    if(parse_decimal_integer(m_internal, &c, &i, buffer, size, &streaminf_variant_playlist->average_bandwidth, 0) != 0) {
                                        return;
                                    }
                                    break;
                                case STREAMINF_ATTRIBUTE_CODECS:
                                    // TODO: Parse this as a list instead of as a string
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &streaminf_variant_playlist->codecs, 0) != 0) {
                                        return;
                                    }
                                    break;
                                case STREAMINF_ATTRIBUTE_RESOLUTION:
                                    if(parse_resolution(m_internal, &c, &i, buffer, size, &streaminf_variant_playlist->width, &streaminf_variant_playlist->height, 0) != 0) {
                                        return;
                                    }
                                    break;
                                case STREAMINF_ATTRIBUTE_FRAME_RATE:
                                    if(parse_decimal_float(m_internal, &c, &i, buffer, size,
                                                           &streaminf_variant_playlist->framerate, 0) != 0) {
                                        return;
                                    }
                                    break;
                                case STREAMINF_ATTRIBUTE_HDCP_LEVEL:
                                    PARSE_ENUM_START
                                                    STREAMINF_ATTRIBUTE_HDCP_LEVEL_NONE:
                                                    streaminf_variant_playlist->hdcp_level = HDCP_LEVEL_NONE;
                                                    PARSE_ENUM_BREAK
                                                    STREAMINF_ATTRIBUTE_HDCP_LEVEL_TYPE_0:
                                                    streaminf_variant_playlist->hdcp_level = HDCP_LEVEL_TYPE_0;
                                    PARSE_ENUM_FINISH(0)
                                    break;
                                case STREAMINF_ATTRIBUTE_AUDIO:
                                case STREAMINF_ATTRIBUTE_VIDEO:
                                case STREAMINF_ATTRIBUTE_SUBTITLES:
                                case STREAMINF_ATTRIBUTE_CLOSED_CAPTIONS:
                                    if(parse_quoted_string_as_hash(m_internal, &c, &i, buffer, size, &streaminf_variant_playlist->rendition_group_id_hash, 0) != 0) {
                                        return;
                                    }
                                    break;
                                default:
                                    if(m_internal->strict_attribute_lists) {
                                        fprintf(stderr,
                                                "[m3u8] Invalid file format: Unknown EXT-X-STREAM-INF attribute. Offset: %i\n",
                                                i);
                                        return;
                                    } else {
                                        skip_attribute(m_internal, &c, &i, buffer, size, 0);
                                    }

                            }
                        }
                        if(m_internal->_parse_token != ATTRIBUTE) {
                            break;
                        }
                    }
                    break;
                case EXT_X_I_FRAME_STREAM_INF:
                    verify_playlist_type(playlist, MASTER_PLAYLIST, i);
                    variant_playlist *istreaminf_variant_playlist = create_latest_variant_playlist(m_internal, playlist);
                    while (i < size) {
                        c = buffer[i];
                        if (m_internal->_attribute_sequence == 0) {
                            // Parse up the attribute name
                            if (parse_attribute_name(m_internal, &c, &i, buffer, size, 1) != 0) {
                                return;
                            }
                        }
                        if (m_internal->_attribute_sequence == 1) {
                            // Parse the attribute value
                            switch (m_internal->attribute_name_hash) {
                                case STREAMINF_ATTRIBUTE_BANDWIDTH:
                                    if(parse_decimal_integer(m_internal, &c, &i, buffer, size, &istreaminf_variant_playlist->max_bandwidth, 0) != 0) {
                                        return;
                                    }
                                    break;
                                case STREAMINF_ATTRIBUTE_AVERAGE_BANDWIDTH:
                                    if(parse_decimal_integer(m_internal, &c, &i, buffer, size, &istreaminf_variant_playlist->average_bandwidth, 0) != 0) {
                                        return;
                                    }
                                    break;
                                case STREAMINF_ATTRIBUTE_CODECS:
                                    // TODO: Parse this as a list instead of as a string
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &istreaminf_variant_playlist->codecs, 0) != 0) {
                                        return;
                                    }
                                    break;
                                case STREAMINF_ATTRIBUTE_RESOLUTION:
                                    if(parse_resolution(m_internal, &c, &i, buffer, size, &istreaminf_variant_playlist->width, &istreaminf_variant_playlist->height, 0) != 0) {
                                        return;
                                    }
                                    break;
                                case STREAMINF_ATTRIBUTE_HDCP_LEVEL:
                                    PARSE_ENUM_START
                                                    STREAMINF_ATTRIBUTE_HDCP_LEVEL_NONE:
                                                    streaminf_variant_playlist->hdcp_level = HDCP_LEVEL_NONE;
                                                    PARSE_ENUM_BREAK
                                                    STREAMINF_ATTRIBUTE_HDCP_LEVEL_TYPE_0:
                                                    streaminf_variant_playlist->hdcp_level = HDCP_LEVEL_TYPE_0;
                                    PARSE_ENUM_FINISH(0)
                                    break;
                                case ISTREAMINF_ATTRIBUTE_URI:
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &istreaminf_variant_playlist->uri, 0) != 0) {
                                        return;
                                    }
                                    break;
                                case STREAMINF_ATTRIBUTE_VIDEO:
                                    if(parse_quoted_string_as_hash(m_internal, &c, &i, buffer, size, &streaminf_variant_playlist->rendition_group_id_hash, 0) != 0) {
                                        return;
                                    }
                                    break;
                                default:
                                    if(m_internal->strict_attribute_lists) {
                                        fprintf(stderr, "[m3u8] Invalid file format: Unknown EXT-X-I-FRAME-STREAM-INF attribute. Offset: %i\n", i);
                                        return;
                                    } else {
                                        skip_attribute(m_internal, &c, &i, buffer, size, 0);
                                    }
                            }
                        }
                        if(m_internal->_parse_token != ATTRIBUTE) {
                            break;
                        }
                    }
                    if(m_internal->_parse_token == START) {
                        m_internal->_latest_content_token = NULL;
                    }
                    break;
                case EXT_X_SESSION_DATA:
                    verify_playlist_type(playlist, MASTER_PLAYLIST, i);
                    session_data *sd_session_data;
                    if(m_internal->_attribute_sequence == 0) {
                        sd_session_data = malloc(sizeof(sd_session_data));
                        m_internal->_attribute_sequence = 1;

                        // TODO: be more efficient with this
                        session_data *prev_session_data = playlist->first_session_data;
                        if(prev_session_data != NULL) {
                            while(prev_session_data->next_session_data != NULL) {
                                prev_session_data = prev_session_data->next_session_data;
                            }
                            prev_session_data->next_session_data = sd_session_data;
                        } else {
                            playlist->first_session_data = sd_session_data;
                        }
                    } else {
                        // TODO: be more efficient with this
                        sd_session_data = playlist->first_session_data;
                        while(sd_session_data->next_session_data != NULL) {
                            sd_session_data = sd_session_data->next_session_data;
                        }
                    }
                    while (i < size) {
                        c = buffer[i];
                        if (m_internal->_attribute_sequence == 1) {
                            // Parse up the attribute name
                            if (parse_attribute_name(m_internal, &c, &i, buffer, size, 2) != 0) {
                                return;
                            }
                        }
                        if (m_internal->_attribute_sequence == 2) {
                            // Parse the attribute value
                            switch (m_internal->attribute_name_hash) {
                                case SESSION_DATA_ATTRIBUTE_DATA_ID:
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &sd_session_data->data_id, 1) != 0) {
                                        return;
                                    }
                                    break;
                                case SESSION_DATA_ATTRIBUTE_VALUE:
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &sd_session_data->value, 1) != 0) {
                                        return;
                                    }
                                    break;
                                case SESSION_DATA_ATTRIBUTE_URI:
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &sd_session_data->uri, 1) != 0) {
                                        return;
                                    }
                                    break;
                                case SESSION_DATA_ATTRIBUTE_LANGUAGE:
                                    if(parse_quoted_string(m_internal, &c, &i, buffer, size, &sd_session_data->language, 1) != 0) {
                                        return;
                                    }
                                    break;
                                default:
                                    if(m_internal->strict_attribute_lists) {
                                        fprintf(stderr, "[m3u8] Invalid file format: Unknown EXT-X-SESSION-DATA attribute. Offset: %i\n", i);
                                        return;
                                    } else {
                                        skip_attribute(m_internal, &c, &i, buffer, size, 1);
                                    }
                            }
                        }
                        if(m_internal->_parse_token != ATTRIBUTE) {
                            break;
                        }
                    }
                    break;
                case EXT_X_SESSION_KEY:
                    verify_playlist_type(playlist, MASTER_PLAYLIST, i);
                    m_internal->_parse_token = TAG_SKIPPING;
                    // TODO: Properly handle keys
                    break;
                    // Media or Master Playlist Tags
                case EXT_X_START:
                    while (i < size) {
                        c = buffer[i];
                        if (m_internal->_attribute_sequence == 0) {
                            // Parse up the attribute name
                            if (parse_attribute_name(m_internal, &c, &i, buffer, size, 1) != 0) {
                                return;
                            }
                        }
                        if (m_internal->_attribute_sequence == 1) {
                            // Parse the attribute value
                            switch (m_internal->attribute_name_hash) {
                                case START_ATTRIBUTE_TIME_OFFSET:
                                    if(parse_decimal_float(m_internal, &c, &i, buffer, size, &playlist->start_time_offset, 1) != 0) {
                                        return;
                                    }
                                    break;
                                case START_ATTRIBUTE_PRECISE:
                                    PARSE_ENUM_START
                                                    START_ATTRIBUTE_PRECISE_YES:
                                                    playlist->start_precise = true;
                                                    PARSE_ENUM_BREAK
                                                    START_ATTRIBUTE_PRECISE_NO:
                                                    playlist->start_precise = false;
                                    PARSE_ENUM_FINISH(0)
                                    break;
                                default:
                                    if(m_internal->strict_attribute_lists) {
                                        fprintf(stderr, "[m3u8] Invalid file format: Unknown EXT-X-START attribute. Offset: %i\n", i);
                                        return;
                                    } else {
                                        skip_attribute(m_internal, &c, &i, buffer, size, 0);
                                    }
                            }
                        }
                        if(m_internal->_parse_token != ATTRIBUTE) {
                            break;
                        }
                    }
                    break;
                default:
                    // Custom Tag.
                    if(!m_internal->allow_custom_tags){
                        fprintf(stderr, "[m3u8] Invalid file format: Parsed tag h:%lu, and went to attributes routine as a custom tag. Offset: %i\n", m_internal->tag_hash, i);
                        return;
                    }
                    break;
            }
        } else if (m_internal->_parse_token == URI) {
            debug_print("[m3u8] Parsing at URI parse token. Offset: %i\n", i);
            while (i < size) {
                c = buffer[i];
                if(c == '\n' || c == '\r') {
                    // Url is done!
                    m_internal->_latest_content_token = NULL;

                    m_internal->_parse_token = START;
                    m_internal->_parse_index = 0;
                    break;
                } else {
                    m_internal->_parse_buffer[m_internal->_parse_index] = c;
                    m_internal->_parse_index++;
                    i++;
                }
            }
        }
    }
}

playlist *parse_playlist(M3U8 *m3u8, const char *in) {
    playlist *pl = create_playlist(m3u8);
    debug_print("[m3u8] Parsing single chunk...%i\n", 0);
    parse_playlist_chunk(m3u8, pl, in, strlen(in));
    return pl;
}
