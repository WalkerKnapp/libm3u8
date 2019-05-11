#include "m3u8.h"
#include "m3u8-hash.h"

#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#ifdef M3U8_DEBUG
#define debug_print(msg, args) fprintf(msg, args);
#else // M3U8 not defined
#define debug_print(msg, args)
#endif

#define M3U8_TAG_MAX_SIZE 128
#define M3U8_URI_MAX_SIZE 1024
#define M3U8_ATTRIBUTE_NAME_MAX_SIZE 128
#define M3U8_ATTRIBUTE_MAX_SIZE 128

#define cap_string(size, buf) buf[size] = 0x0; buf = realloc(buf, size + 1)

enum parse_token {
    START,
    TAG,
    TAG_SKIPPING,
    ATTRIBUTE_NAME,
    ATTRIBUTE,
    URI
};

typedef struct {
    bool allow_custom_tags;
    // EXT-X-MEDIA-SEGMENT and EXT-X-DISCONTINUITY-SEQUENCE
    bool force_tag_sequencing;

    // Parsing helpers
    enum parse_token _parse_token;
    char *_parse_buffer;

    unsigned long tag_hash;
    unsigned long attribute_name_hash;
    unsigned long attribute_enum_hash;

    int _parse_index;
    void *_latest_content_token;

    int _attribute_sequence;

    media_init_section *latest_media_init_section;
} M3U8_internal;

int parse_attribute_name(M3U8_internal *m_internal, char *read, unsigned int *readinc, const char *buffer, int buffersize, int reset_attribute_sequence) {
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
int parse_hex_int(M3U8_internal *m_internal, char *read, unsigned int *readinc, const char *buffer, int buffersize, unsigned int** dest, unsigned int max_dest_size, int attribute_sequence_reset) {
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

        if((m_internal->_parse_index - 2)/ (sizeof(int) * 2) >= max_dest_size) {
            fprintf(stderr, "[m3u8] Invalid file format: Hex Integer is too long. Expected to be able to store in %i integers. Offset: %i\n", max_dest_size, *readinc);
            return -1;
        }

        (*dest)[(m_internal->_parse_index - 2)/ (sizeof(int) * 2)] = ((*dest)[(m_internal->_parse_index - 2)/ (sizeof(int) * 2)] << 4) | (byte & 0xF);
        
        (*readinc)++;
        m_internal->_parse_index++;
    }
    return 0;
}

int parse_quoted_string(M3U8_internal *m_internal, char *read, unsigned int *readinc, const char *buffer, int buffersize, char **dest, int attribute_sequence_reset) {
    if (*dest == NULL) {
        *dest = malloc(M3U8_ATTRIBUTE_MAX_SIZE);
    }
    if (m_internal->_parse_index == 0) {
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
        } else if (*read == '"') {
            cap_string(m_internal->_parse_index - 1, *dest);
            m_internal->_parse_index = -1;
            (*readinc)++;
        } else if (m_internal->_parse_index == -1 && *read == ',') {
            m_internal->_attribute_sequence = attribute_sequence_reset;
            m_internal->attribute_name_hash = HASH_START_VALUE;
            (*readinc)++;
            break;
        } else {
            (*dest)[m_internal->_parse_index - 1] = *read;
            m_internal->_parse_index++;
            (*readinc)++;
        }
    }
    return 0;
}

int parse_quoted_string_as_hash(M3U8_internal *m_internal, char *read, unsigned int *readinc, const char *buffer, int buffersize, unsigned long *dest, int attribute_sequence_reset) {
    if (m_internal->_parse_index == 0) {
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

int parse_decimal_integer(M3U8_internal *m_internal, char *read, unsigned int *readinc, const char *buffer, int buffersize, int *dest, int attribute_sequence_reset) {
    while (*readinc < buffersize) {
        *read = buffer[*readinc];
        if(*read == '\n' || *read == '\r') {
            m_internal->_parse_token = START;
            (*readinc)++;
            break;
        } else if (*read == ',') {
            m_internal->_attribute_sequence = attribute_sequence_reset;
            m_internal->attribute_name_hash = HASH_START_VALUE;
            (*readinc)++;
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

int parse_resolution(M3U8_internal *m_internal, char *read, unsigned int *readinc, const char *buffer, int buffersize, int *x_dest, int *y_dest, int attribute_sequence_reset) {
    while (*readinc < buffersize) {
        *read = buffer[*readinc];
        if(*read == '\n' || *read == '\r') {
            if(m_internal->_parse_index != 1) {
                fprintf(stderr, "[m3u8] Invalid file format: Unexpected newline in resolution. Offset: %i\n", *readinc);
                return -1;
            } else {
                m_internal->_parse_token = START;
                (*readinc)++;
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

int parse_decimal_float(M3U8_internal *m_internal, char *read, unsigned int *readinc, const char *buffer,
                        int buffersize, float *dest, int attribute_sequence_reset) {
    while (*readinc < buffersize) {
        *read = buffer[*readinc];
        if(*read == '\n' || *read == '\r') {
            if(m_internal->_parse_index == 0) {
                fprintf(stderr, "[m3u8] Invalid file format: Unexpected newline in resolution. Offset: %i\n", *readinc);
                return -1;
            } else {
                m_internal->_parse_token = START;
                (*readinc)++;
                break;
            }
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
            } else if (m_internal->_parse_index == 1) {
                *dest += ((float)(*read) - '0')/pow(10, m_internal->_parse_index);
                m_internal->_parse_index++;
                (*readinc)++;
            } else {
                fprintf(stderr, "[m3u8] Invalid file format: Invalid parse_index state while parsing resolution: %i. Offset: %i\n", m_internal->_parse_index, *readinc);
                return -1;
            }
        }
    }
    return 0;
}

void m3u8_setopt_allow_custom_tags(M3U8* m3u8, bool allowed) {
    ((M3U8_internal *)m3u8)->allow_custom_tags = allowed;
}

M3U8 *create_m3u8() {
    return malloc(sizeof(M3U8_internal));
}

playlist *create_playlist(M3U8 *m3u8) {
    playlist *pl = (playlist*) malloc(sizeof(playlist));
    ((M3U8_internal *)m3u8)->_parse_token = START;
    return pl;
}

media_segment *create_latest_media_segment(M3U8_internal *m_internal, playlist *playlist) {
    if(m_internal->_latest_content_token == NULL) {
        m_internal->_latest_content_token = malloc(sizeof(media_segment));
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
        m_internal->_latest_content_token = malloc(sizeof(variant_playlist));
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

void parse_playlist_chunk(M3U8 *m3u8, playlist *playlist, const char *buffer, int size) {
    debug_print("[m3u8] Parsing buffer of size %i\n", size);

    M3U8_internal* m_internal = m3u8;

    for (unsigned int i = 0; i < size; i++) {
        char c = buffer[i];

        // See if the playlist is being parsed from the beginning of or partly through a line
        if(m_internal->_parse_token == START) {
            debug_print("[m3u8] Parsing at START parse token. Offset: %i\n", i);
            switch (c) {
                // Ignore newlines, this line is empty
                case '\n':
                case '\t':
                    continue;
                // Line is a tag
                case '#':
                    m_internal->_parse_token = TAG;
                    if(m_internal->allow_custom_tags) {
                        m_internal->_parse_buffer = malloc(M3U8_TAG_MAX_SIZE);
                    }
                    m_internal->tag_hash = HASH_START_VALUE;
                    m_internal->_parse_index = 0;
                    continue;
                // Line is a URI
                default:
                    m_internal->_parse_token = URI;
                    // The latest parsed token *should* be a URI object with an empty buffer as URI, created by EXT-X-STREAM-INF or EXTINF
                    // If not, the m3u8 is invalid or custom.

                    if(m_internal->_latest_content_token == NULL) {
                        fprintf(stderr, "[m3u8] Invalid file format: no EXT-X-STREAM-INF or EXTINF preceding URI. Offset: %i\n", i);
                        return;
                    }

                    if(playlist->type == MASTER_PLAYLIST) {
                        variant_playlist *variant = ((variant_playlist *)m_internal->_latest_content_token);
                        m_internal->_parse_buffer = variant->uri;
                    } else {
                        media_segment *segment = ((media_segment *)m_internal->_latest_content_token);
                        m_internal->_parse_buffer = segment->uri;
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
                    if(c == '\n' || c == '\r') {
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
                                    create_latest_media_segment(m_internal, playlist)->discontinuity = true;
                                    break;
                                    // Media Playlist Tags
                                case EXT_X_ENDLIST:
                                    // TODO: Maybe lock playlist after EXT-X-ENDLIST?
                                case EXT_X_I_FRAMES_ONLY:
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
                            m_internal->_parse_token = START;
                            i++;
                            break;
                        } // Otherwise a very short comment, ignore
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
            while (i < size) {
                c = buffer[i];
                if(c == '\n' || c == '\r') {
                    m_internal->_parse_token = START;
                    break;
                }
                i++;
            }
        } else if (m_internal->_parse_token == ATTRIBUTE) {
            switch (m_internal->tag_hash) {
                // Media Segment Tags
                case EXTINF:
                    // Format: <duration(float/int)>,[<title(non-quoted-str)>]
                    media_segment *inf_segment = create_latest_media_segment(m_internal, playlist);
                    if(m_internal->_attribute_sequence == 0) {
                        while (i < size) {
                            c = buffer[i];
                            if(c == '\n' || c == '\r') {
                                m_internal->_parse_token = START;
                                i++;
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
                    media_segment *key_segment = create_latest_media_segment(m_internal, playlist);
                    while (i < size) {
                        if (m_internal->_attribute_sequence == 0) {
                            // Parse up the attribute name
                            if(parse_attribute_name(m_internal, &c, &i, buffer, size, 1) != -1) {
                                return;
                            }
                        }
                        if (m_internal->_attribute_sequence == 1) {
                            // Parse the attribute value
                            switch (m_internal->attribute_name_hash) {
                                case KEY_ATTRIBUTE_METHOD:
                                    while (i < size) {
                                        c = buffer[i];
                                        if (c == '\n' || c == '\t' || c == ',') {
                                            switch (m_internal->attribute_enum_hash) {
                                                case KEY_ATTRIBUTE_METHOD_NONE:
                                                    key_segment->key_method = KEY_METHOD_NONE;
                                                    break;
                                                case KEY_ATTRIBUTE_METHOD_AES_128:
                                                    key_segment->key_method = KEY_METHOD_AES128;
                                                    break;
                                                case KEY_ATTRIBUTE_METHOD_SAMPLE_AES:
                                                    key_segment->key_method = KEY_METHOD_SAMPLEAES;
                                                    break;
                                                default:
                                                    fprintf(stderr, "[m3u8] Invalid file format: Unexpected 'METHOD' enum value. Offset: %i\n", i);
                                                    return;
                                            }

                                            if (c == ',') {
                                                m_internal->_attribute_sequence = 0;
                                                m_internal->attribute_name_hash = HASH_START_VALUE;
                                                break;
                                            } else {
                                                m_internal->_parse_token = START;
                                                i++;
                                                break;
                                            }
                                        } else {
                                            hash_char(m_internal->attribute_enum_hash, c);
                                            i++;
                                        }
                                    }
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
                                    fprintf(stderr, "[m3u8] Invalid file format: Unknown EXT-X-KEY attribute. Offset: %i\n", i);
                                    return;
                            }
                        }
                    }
                case EXT_X_MAP:
                    if(m_internal->_attribute_sequence == 0) {
                        m_internal->latest_media_init_section = malloc(sizeof(media_init_section));
                        m_internal->_attribute_sequence = 1;
                    }
                    media_init_section *init_section = m_internal->latest_media_init_section;
                    while (i < size) {
                        if (m_internal->_attribute_sequence == 1) {
                            if(parse_attribute_name(m_internal, &c, &i, buffer, size, 2) != -1) {
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
                                    fprintf(stderr, "[m3u8] Invalid file format: Unknown EXT-X-MAP attribute. Offset: %i\n", i);
                                    return;
                            }
                        }
                        if (m_internal->_attribute_sequence == 3) {
                            // Parsing the offset portion of the byterange attribute
                            while (i < size) {
                                c = buffer[i];
                                if(c == '\n' || c == '\r') {
                                    m_internal->_parse_token = START;
                                    i++;
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
                    m_internal->_parse_token = TAG_SKIPPING;
                    // TODO: Are there no good datetime apis for c?
                case EXT_X_DATERANGE:
                    m_internal->_parse_token = TAG_SKIPPING;
                    // TODO: Find a way to accurately represent this data.
                    break;
                // Media Playlist Tags
                case EXT_X_TARGETDURATION:
                    if (m_internal->_attribute_sequence == 0) {
                        playlist->target_duration = 0;
                        m_internal->_attribute_sequence = 1;
                    }
                    while (i < size) {
                        c = buffer[i];
                        if (c == '\n' || c == '\r') {
                            m_internal->_parse_token = START;
                            i++;
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
                    m_internal->_parse_token = TAG_SKIPPING;
                    // TODO: See where this applies
                    break;
                case EXT_X_PLAYLIST_TYPE:
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
                            break;
                        }

                        hash_char(m_internal->attribute_enum_hash, c);
                        i++;
                    }
                // Master Playlist Tags
                case EXT_X_MEDIA:
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
                    while (i < size) {
                        if (m_internal->_attribute_sequence == 1) {
                            // Parse up the attribute name
                            if (parse_attribute_name(m_internal, &c, &i, buffer, size, 2) != -1) {
                                return;
                            }
                        }
                        if (m_internal->_attribute_sequence == 2) {
                            // Parse the attribute value
                            switch (m_internal->attribute_name_hash) {
                                case MEDIA_ATTRIBUTE_TYPE:
                                    while (i < size) {
                                        c = buffer[i];
                                        if (c == '\n' || c == '\t' || c == ',') {
                                            switch (m_internal->attribute_enum_hash) {
                                                case MEDIA_ATTRIBUTE_TYPE_AUDIO:
                                                    media_variant_rendition->type = AUDIO;
                                                    break;
                                                case MEDIA_ATTRIBUTE_TYPE_VIDEO:
                                                    media_variant_rendition->type = VIDEO;
                                                    break;
                                                case MEDIA_ATTRIBUTE_TYPE_SUBTITLES:
                                                    media_variant_rendition->type = SUBTITLES;
                                                    break;
                                                case MEDIA_ATTRIBUTE_TYPE_CLOSED_CAPTIONS:
                                                    media_variant_rendition->type = CLOSED_CAPTIONS;
                                                    break;
                                                default:
                                                    fprintf(stderr, "[m3u8] Invalid file format: Unexpected 'TYPE' enum value. Offset: %i\n", i);
                                                    return;
                                            }

                                            if (c == ',') {
                                                m_internal->_attribute_sequence = 1;
                                                m_internal->attribute_name_hash = HASH_START_VALUE;
                                                break;
                                            } else {
                                                m_internal->_parse_token = START;
                                                i++;
                                                break;
                                            }
                                        } else {
                                            hash_char(m_internal->attribute_enum_hash, c);
                                            i++;
                                        }
                                    }
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
                                    while (i < size) {
                                        c = buffer[i];
                                        if (c == '\n' || c == '\t' || c == ',') {
                                            switch (m_internal->attribute_enum_hash) {
                                                case MEDIA_ATTRIBUTE_YES:
                                                    media_variant_rendition->is_default = true;
                                                    break;
                                                case MEDIA_ATTRIBUTE_NO:
                                                    media_variant_rendition->is_default = false;
                                                    break;
                                                default:
                                                    fprintf(stderr, "[m3u8] Invalid file format: Unexpected 'DEFAULT' enum value. Offset: %i\n", i);
                                                    return;
                                            }

                                            if (c == ',') {
                                                m_internal->_attribute_sequence = 1;
                                                m_internal->attribute_name_hash = HASH_START_VALUE;
                                                break;
                                            } else {
                                                m_internal->_parse_token = START;
                                                i++;
                                                break;
                                            }
                                        } else {
                                            hash_char(m_internal->attribute_enum_hash, c);
                                            i++;
                                        }
                                    }
                                    break;
                                case MEDIA_ATTRIBUTE_AUTOSELECT:
                                    while (i < size) {
                                        c = buffer[i];
                                        if (c == '\n' || c == '\t' || c == ',') {
                                            switch (m_internal->attribute_enum_hash) {
                                                case MEDIA_ATTRIBUTE_YES:
                                                    media_variant_rendition->is_autoselect = true;
                                                    break;
                                                case MEDIA_ATTRIBUTE_NO:
                                                    media_variant_rendition->is_autoselect = false;
                                                    break;
                                                default:
                                                    fprintf(stderr, "[m3u8] Invalid file format: Unexpected 'AUTOSELECT' enum value. Offset: %i\n", i);
                                                    return;
                                            }

                                            if (c == ',') {
                                                m_internal->_attribute_sequence = 1;
                                                m_internal->attribute_name_hash = HASH_START_VALUE;
                                                break;
                                            } else {
                                                m_internal->_parse_token = START;
                                                i++;
                                                break;
                                            }
                                        } else {
                                            hash_char(m_internal->attribute_enum_hash, c);
                                            i++;
                                        }
                                    }
                                    break;
                                case MEDIA_ATTRIBUTE_FORCED:
                                    while (i < size) {
                                        c = buffer[i];
                                        if (c == '\n' || c == '\t' || c == ',') {
                                            switch (m_internal->attribute_enum_hash) {
                                                case MEDIA_ATTRIBUTE_YES:
                                                    media_variant_rendition->is_forced = true;
                                                    break;
                                                case MEDIA_ATTRIBUTE_NO:
                                                    media_variant_rendition->is_forced = false;
                                                    break;
                                                default:
                                                    fprintf(stderr, "[m3u8] Invalid file format: Unexpected 'FORCED' enum value. Offset: %i\n", i);
                                                    return;
                                            }

                                            if (c == ',') {
                                                m_internal->_attribute_sequence = 1;
                                                m_internal->attribute_name_hash = HASH_START_VALUE;
                                                break;
                                            } else {
                                                m_internal->_parse_token = START;
                                                i++;
                                                break;
                                            }
                                        } else {
                                            hash_char(m_internal->attribute_enum_hash, c);
                                            i++;
                                        }
                                    }
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
                                    fprintf(stderr, "[m3u8] Invalid file format: Unknown EXT-X-MEDIA attribute. Offset: %i\n", i);
                                    return;
                            }
                        }
                    }
                    break;
                case EXT_X_STREAM_INF:
                    variant_playlist *streaminf_variant_playlist = create_latest_variant_playlist(m_internal, playlist);
                    while (i < size) {
                        c = buffer[i];
                        if (m_internal->_attribute_sequence == 0) {
                            // Parse up the attribute name
                            if (parse_attribute_name(m_internal, &c, &i, buffer, size, 1) != -1) {
                                return;
                            }
                        }
                        if (m_internal->_attribute_sequence == 1) {
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
                                    while (i < size) {
                                        c = buffer[i];
                                        if (c == '\n' || c == '\t' || c == ',') {
                                            switch (m_internal->attribute_enum_hash) {
                                                case STREAMINF_ATTRIBUTE_HDCP_LEVEL_NONE:
                                                    streaminf_variant_playlist->hdcp_level = HDCP_LEVEL_NONE;
                                                    break;
                                                case STREAMINF_ATTRIBUTE_HDCP_LEVEL_TYPE_0:
                                                    streaminf_variant_playlist->hdcp_level = HDCP_LEVEL_TYPE_0;
                                                    break;
                                                default:
                                                    fprintf(stderr, "[m3u8] Invalid file format: Unexpected 'HDCP-LEVEL' enum value. Offset: %i\n", i);
                                                    return;
                                            }

                                            if (c == ',') {
                                                m_internal->_attribute_sequence = 0;
                                                m_internal->attribute_name_hash = HASH_START_VALUE;
                                                break;
                                            } else {
                                                m_internal->_parse_token = START;
                                                i++;
                                                break;
                                            }
                                        } else {
                                            hash_char(m_internal->attribute_enum_hash, c);
                                            i++;
                                        }
                                    }
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
                                    fprintf(stderr, "[m3u8] Invalid file format: Unknown EXT-X-STREAM_INF attribute. Offset: %i\n", i);
                                    return;

                            }
                        }

                        i++;
                    }
                    break;
                case EXT_X_I_FRAME_STREAM_INF:
                    variant_playlist *istreaminf_variant_playlist = create_latest_variant_playlist(m_internal, playlist);
                    while (i < size) {
                        c = buffer[i];
                        if (m_internal->_attribute_sequence == 0) {
                            // Parse up the attribute name
                            if (parse_attribute_name(m_internal, &c, &i, buffer, size, 1) != -1) {
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
                                    while (i < size) {
                                        c = buffer[i];
                                        if (c == '\n' || c == '\t' || c == ',') {
                                            switch (m_internal->attribute_enum_hash) {
                                                case STREAMINF_ATTRIBUTE_HDCP_LEVEL_NONE:
                                                    istreaminf_variant_playlist->hdcp_level = HDCP_LEVEL_NONE;
                                                    break;
                                                case STREAMINF_ATTRIBUTE_HDCP_LEVEL_TYPE_0:
                                                    istreaminf_variant_playlist->hdcp_level = HDCP_LEVEL_TYPE_0;
                                                    break;
                                                default:
                                                    fprintf(stderr, "[m3u8] Invalid file format: Unexpected 'HDCP-LEVEL' enum value. Offset: %i\n", i);
                                                    return;
                                            }

                                            if (c == ',') {
                                                m_internal->_attribute_sequence = 0;
                                                m_internal->attribute_name_hash = HASH_START_VALUE;
                                                break;
                                            } else {
                                                m_internal->_parse_token = START;
                                                i++;
                                                break;
                                            }
                                        } else {
                                            hash_char(m_internal->attribute_enum_hash, c);
                                            i++;
                                        }
                                    }
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
                                    fprintf(stderr, "[m3u8] Invalid file format: Unknown EXT-X-I-FRAME-STREAM-INF attribute. Offset: %i\n", i);
                                    return;

                            }
                        }

                        i++;
                    }
                    if(m_internal->_parse_token == START) {
                        m_internal->_latest_content_token = NULL;
                    }
                    break;
                case EXT_X_SESSION_DATA:
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
                            if (parse_attribute_name(m_internal, &c, &i, buffer, size, 2) != -1) {
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
                                    fprintf(stderr, "[m3u8] Invalid file format: Unknown EXT-X-SESSION-DATA attribute. Offset: %i\n", i);
                                    return;
                            }
                        }

                        i++;
                    }
                    break;
                case EXT_X_SESSION_KEY:
                    m_internal->_parse_token = TAG_SKIPPING;
                    // TODO: Properly handle keys
                    break;
                // Media or Master Playlist Tags
                case EXT_X_START:
                    while (i < size) {
                        c = buffer[i];
                        if (m_internal->_attribute_sequence == 0) {
                            // Parse up the attribute name
                            if (parse_attribute_name(m_internal, &c, &i, buffer, size, 1) != -1) {
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
                                    while (i < size) {
                                        c = buffer[i];
                                        if (c == '\n' || c == '\t' || c == ',') {
                                            switch (m_internal->attribute_enum_hash) {
                                                case START_ATTRIBUTE_PRECISE_YES:
                                                    playlist->start_precise = true;
                                                    break;
                                                case START_ATTRIBUTE_PRECISE_NO:
                                                    playlist->start_precise = false;
                                                    break;
                                                default:
                                                    fprintf(stderr, "[m3u8] Invalid file format: Unexpected 'PRECISE' enum value. Offset: %i\n", i);
                                                    return;
                                            }

                                            if (c == ',') {
                                                m_internal->_attribute_sequence = 0;
                                                m_internal->attribute_name_hash = HASH_START_VALUE;
                                                break;
                                            } else {
                                                m_internal->_parse_token = START;
                                                i++;
                                                break;
                                            }
                                        } else {
                                            hash_char(m_internal->attribute_enum_hash, c);
                                            i++;
                                        }
                                    }
                                    break;
                                default:
                                    fprintf(stderr, "[m3u8] Invalid file format: Unknown EXT-X-START attribute. Offset: %i\n", i);
                                    return;
                            }
                        }

                        i++;
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
            // TODO: Set NULL Segment/Variant after done parsing
            // TODO:
        }
    }
}