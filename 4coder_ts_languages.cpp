#if !defined(F4_TS_LANGUAGES_CPP)
#define F4_TS_LANGUAGES_CPP

internal void
jpts_register_language(String_Const_u8 name, TSLanguage *tree_sitter_language, TSQuery *highlight_query)
{
    if(jpts_languages.initialized == 0)
    {
        jpts_languages.initialized = 1;
        jpts_languages.arena = make_arena_system(KB(16));
    }
    
    Language_Definition *language = 0;
    u64 hash = table_hash_u8(name.str, name.size);
    u64 slot = hash % ArrayCount(jpts_languages.language_table);
    for(Language_Definition *lang = jpts_languages.language_table[slot];
        lang;
        lang = lang->next)
    {
        if(lang->hash == hash && string_match(lang->name, name))
        {
            language = lang;
            break;
        }
    }
    
    if(language == 0)
    {
        language = push_array(&jpts_languages.arena, Language_Definition, 1);
        language->next = jpts_languages.language_table[slot];
        jpts_languages.language_table[slot] = language;
        language->hash = hash;
        language->name = push_string_copy(&jpts_languages.arena, name);
        language->ts_language = tree_sitter_language;
        // TODO(jack): Do we want create the query here, or is it better to create it outside
        // so that all entries of a given language share teh 
        language->highlight_query = highlight_query;
    }
}

//~ NOTE(jack): @language_registration

internal void
jpts_register_language_group(Application_Links *app, List_String_Const_u8 extensions,
                             String_Const_u8 highlight_query, TSLanguage *ts_language)
{
    // NOTE(jack): Assert that the tree-sitter language is comaptible with this version of the library
    Assert((ts_language_version(ts_language) >= TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION) &&
           (ts_language_version(ts_language) <= TREE_SITTER_LANGUAGE_VERSION));
    
    u32 error_offset;
    TSQueryError query_error;
    TSQuery *language_query = ts_query_new(ts_language, (const char *)highlight_query.str,
                                           (u32)highlight_query.size,
                                           &error_offset, &query_error);
    
    
    if (!language_query)
    {
        char buffer[1024];
        int len = snprintf(buffer, 1024, "Failed to create highlight query error (%d) at offset: %u",
                           query_error, error_offset);
        
        String_Const_u8 error_loc = { highlight_query.str + error_offset, highlight_query.size - error_offset };
        print_message(app, SCu8(buffer, len));
        AssertMessageAlways(failed to create query);
    }
    
    for (Node_String_Const_u8 *node = extensions.first; node; node = node->next)
    {
        jpts_register_language(node->string, ts_language, language_query);
    }
    
}

// NOTE(jack): Includes the jpts_register_languages function as well as the highlight queries
#include "generated/4coder_tree_sitter_languages_generated.h"

#endif
