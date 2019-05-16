#include "../include/internal-types.h"

#include <stdlib.h>

variant_playlist *first_variant_playlist(playlist *playlist) {
    return playlist->first_variant_playlist;
}
variant_playlist *next_variant_playlist(variant_playlist *variant) {
    return variant->next_variant_playlist;
}
variant_rendition *first_variant_rendition_match(playlist *playlist, variant_playlist *variant) {
    variant_rendition *rendition = playlist->first_variant_rendition;
    while (rendition != NULL) {
        if(rendition->group_id_hash == variant->rendition_group_id_hash) {
            return rendition;
        }
        rendition = rendition->next_variant_rendition;
    }
    return NULL;
}
variant_rendition *next_variant_rendition_match(variant_rendition *orig_rendition) {
    variant_rendition *rendition = orig_rendition->next_variant_rendition;
    while (rendition != NULL) {
        if(rendition->group_id_hash == orig_rendition->group_id_hash) {
            return rendition;
        }
        rendition = rendition->next_variant_rendition;
    }
    return NULL;
}

void m3u8_setopt_allow_custom_tags(M3U8* m3u8, bool allowed) {
    ((M3U8_internal *)m3u8)->allow_custom_tags = allowed;
}

void m3u8_setopt_strict_attribute_lists(M3U8* m3u8, bool strict) {
    ((M3U8_internal *)m3u8)->strict_attribute_lists = strict;
}

M3U8 *create_m3u8() {
    debug_print("[m3u8] Created M3U8 instance.%i\n", 0);
    return calloc(1, sizeof(M3U8_internal));
}

playlist *create_playlist(M3U8 *m3u8) {
    playlist *pl = (playlist*) calloc(1, sizeof(playlist));
    pl->type = UNKNOWN;

    ((M3U8_internal *)m3u8)->_parse_token = START;
    //((M3U8_internal *)m3u8)->_parse_index = 0;
    ((M3U8_internal *)m3u8)->tag_hash = HASH_START_VALUE;
    ((M3U8_internal *)m3u8)->attribute_name_hash = HASH_START_VALUE;
    ((M3U8_internal *)m3u8)->attribute_enum_hash = HASH_START_VALUE;
    debug_print("[m3u8] Created playlist instance.%i\n", 0);
    return pl;
}
