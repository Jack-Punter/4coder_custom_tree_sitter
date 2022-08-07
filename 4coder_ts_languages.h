/* date = June 29th 2022 11:11 am */

#ifndef JPTS_LANGUAGES_H
#define JPTS_LANGUAGES_H

struct Language_Definition
{
    Language_Definition *next;
    u64 hash;
    String_Const_u8 name;
    
    TSLanguage *ts_language;
    TSQuery *highlight_query;
};

global struct JPTS_Languages
{
    b32 initialized;
    Arena arena;
    Language_Definition *language_table[4096];
} jpts_languages;

internal Language_Definition *
jpts_language_from_string(String_Const_u8 name)
{
    Language_Definition *result = 0;
    if(jpts_languages.initialized)
    {
        u64 hash = table_hash_u8(name.str, name.size);
        u64 slot = hash % ArrayCount(jpts_languages.language_table);
        for(Language_Definition *l = jpts_languages.language_table[slot]; l; l = l->next)
        {
            if(l->hash == hash && string_match(l->name, name))
            {
                result = l;
                break;
            }
        }
    }
    return result;
}

internal Language_Definition *
jpts_language_from_buffer(Arena *arena, Application_Links *app, Buffer_ID buffer)
{
    Language_Definition *language = 0;
    String_Const_u8 file_name = push_buffer_file_name(app, arena, buffer);
    String_Const_u8 extension = string_file_extension(file_name);
    language = jpts_language_from_string(extension);
    return language;
}

internal Language_Definition *
jpts_language_from_buffer(Application_Links *app, Buffer_ID buffer)
{
    Scratch_Block scratch(app);
    Language_Definition *language = jpts_language_from_buffer(scratch, app, buffer);
    return language;
} 

#endif // JPTS_LANGUAGES_H
