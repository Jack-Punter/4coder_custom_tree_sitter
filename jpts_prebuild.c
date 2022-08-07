#define _CRT_SECURE_NO_WARNINGS
#include "metadesk/md.h"
#include "metadesk/md.c"
#include <stdio.h>

typedef struct language_information_t
{
    struct language_information_t *next;
    MD_u64 hash;
    MD_String8 language_name;
    
    MD_String8 function_name;
    
    MD_String8List extensions;
    
    MD_String8 highlight_query;
    // TODO(jack): color_map
} Language_Information;

typedef struct language_information_list_t {
    Language_Information *first;
    Language_Information *last;
} Language_Information_List;

#define LANG_TABLE_SIZE 1024
MD_Arena *gLanguageArena;
Language_Information *gLanguageTable[LANG_TABLE_SIZE];

Language_Information *
GetLanguage(MD_String8 language) 
{
    Language_Information *result = 0;
    MD_u64 hash = MD_HashStr(language);
    MD_u64 slot = hash % LANG_TABLE_SIZE;
    
    for (Language_Information *info = gLanguageTable[slot]; info; info = info->next)
    {
        if (info->hash == hash && MD_S8Match(info->language_name, language, 0)) {
            result = info;
            break;
        }
    }
    
    return result;
}

MD_String8 GetChildValue(MD_Node *node, MD_String8 child_name)
{
    
    MD_String8 result = { 0 };
    MD_Node *child = MD_ChildFromString(node, child_name, 0);
    if (MD_NodeIsNil(child)) {
        printf("Error: node '%.*s' has no child: '%.*s'.\n",
               MD_S8VArg(node->string), MD_S8VArg(child_name));
        return result;
    }
    
    if (MD_NodeIsNil(child->first_child)) {
        printf("Error: The node %.*s's  child: '%.*s' has no value.\n",
               MD_S8VArg(node->string), MD_S8VArg(child_name));
        return result;
    }
    
    result = child->first_child->string;
    return result;
}

Language_Information *
ParseLangauge(MD_Arena *arena, MD_Node *root, MD_String8 language_name) 
{
    // NOTE(jack): Look to see if we've already parsed the language, if we have, just return its information
    Language_Information *language_info = GetLanguage(language_name);
    if (language_info) {
        printf("Info [ParseLanguage]: language '%.*s' already parsed, returning from hash table.\n", MD_S8VArg(language_name));
        return language_info;
    }
    
    MD_Node *language = MD_ChildFromString(root, language_name, 0);
    if (MD_NodeIsNil(language)) {
        printf("Error: lanuage '%.*s' does not extist in the file.\n", MD_S8VArg(language_name));
        return 0;
    }
    
    MD_String8 Parent_HighlightQuery = { 0 }; 
    
    //-
    // NOTE(jack): If this language is an extension on another language we need to concatonate their highlight queries.
    // TODO(jack): We will also have to "concatonate" their color maps, but IDK how im dealing with those yet.
    if(MD_NodeHasTag(language, MD_S8Lit("extends"), 0))
    {
        
        MD_Node *extends = MD_TagFromString(language, MD_S8Lit("extends"), 0);
        MD_Node *extends_from_node = extends->first_child;
        
        if(MD_NodeIsNil(extends_from_node)) {
            printf("Error: lanuage '%.*s' has an extend tag with no language specified to extend.\n", MD_S8VArg(language_name));
            return 0;
        }
        
        MD_String8 extends_from_language = extends_from_node->string;
        Language_Information *parent_language = ParseLangauge(arena, root, extends_from_language);
        if (parent_language == 0) {
            printf("Error: lanuage '%.*s' extends from non-existant language %.*s\n",
                   MD_S8VArg(language_name), MD_S8VArg(extends_from_language));
            return 0;
            
        }
        
        Parent_HighlightQuery = parent_language->highlight_query;
    }
    //-
    
    
    MD_String8 function_name             = GetChildValue(language, MD_S8Lit("function_name"));
    MD_String8 lang_base_highlight_query = GetChildValue(language, MD_S8Lit("highlight_query"));
    
    MD_String8List language_extensions = { 0 };
    MD_Node *lang_extensions_child = MD_ChildFromString(language, MD_S8Lit("extensions"), 0);
    if (MD_NodeIsNil(lang_extensions_child)) 
    {
        printf("Error: lanuage '%.*s' has no extensions defined.\n", MD_S8VArg(language_name));
    }
    
    for (MD_EachNode(extension, lang_extensions_child->first_child))
    {
        MD_S8ListPush(arena, &language_extensions, extension->string);
    }
    
    MD_String8 highlight_query; 
    
    if (Parent_HighlightQuery.size != 0) {
        highlight_query = MD_S8Fmt(arena, "%.*s\n%.*s",
                                   MD_S8VArg(Parent_HighlightQuery),
                                   MD_S8VArg(lang_base_highlight_query));
    } else {
        highlight_query = lang_base_highlight_query;
    }
    
    language_info                  = MD_PushArrayZero(gLanguageArena, Language_Information, 1);
    language_info->hash            = MD_HashStr(language_name);
    language_info->language_name   = language_name;
    language_info->function_name   = function_name;
    language_info->highlight_query = highlight_query;
    language_info->extensions      = language_extensions;
    // TODO(jack): colormap;
    
    MD_u64 slot = language_info->hash % LANG_TABLE_SIZE;
    language_info->next = gLanguageTable[slot];
    gLanguageTable[slot] = language_info;
    
    return language_info; // TODO(jack): 
}

int main()
{
    printf("Metadesk Build step:\n");
    gLanguageArena = MD_ArenaAlloc();
    MD_Arena *arena = MD_ArenaAlloc();
    MD_ParseResult parse = MD_ParseWholeFile(arena, MD_S8Lit("../lang/languages.mdesk"));
    
    if (parse.errors.node_count != 0)
    {
        printf("Metadesk Parse Errors:\n");
        for(MD_Message *Message = parse.errors.first;
            Message;
            Message = Message->next)
        {
            printf("\t%.*s\n", MD_S8VArg(Message->string));
        }
        exit(1);
    }
    
    MD_Node *root = parse.node;
    Language_Information_List languages_list = { 0 };
    
    for (MD_EachNode(language, root->first_child))
    {
        MD_String8 language_name = language->string;
        Language_Information *language_info = ParseLangauge(arena, root, language_name);
        if (language_info == 0)
        {
            printf("Failed to parse language: %.*s\n", MD_S8VArg(language_name));
            exit(1);
        }
        
#if 0
        printf("Language '%.*s'\n", MD_S8VArg(language_info->language_name));
        printf("\tfunction_name: %.*s\n", MD_S8VArg(language_info->function_name));
        // printf("\thighlight_query: %.*s\n", MD_S8VArg(language_info->highlight_query));
        printf("\tlanguage_extensions: ");
        
        MD_StringJoin join = {
            .mid = MD_S8Lit(", "),
            .post = MD_S8Lit("\n"),
        };
        MD_String8 extension_string = MD_S8ListJoin(arena, language_info->extensions, &join);
        printf("%.*s, ", MD_S8VArg(extension_string));
#endif
        
        Language_Information *list_language = MD_PushArray(arena, Language_Information, 1);
        
        *list_language = *language_info;
        list_language->next = 0;
        MD_QueuePush(languages_list.first, languages_list.last, list_language);
    }
    
    
    printf("=============================================\n");
    MD_String8List output_string_list = { 0 };
    
    // NOTE(jack): Generate the extern "C" function declarations
    {
        MD_String8List line_list = { 0 };
        for (Language_Information *info = languages_list.first; info; info = info->next)
        {
            MD_S8ListPushFmt(arena, &line_list, "TSLanguage *%S();", info->function_name);
            
        }
        
        MD_StringJoin join = {
            .pre = MD_S8Lit("extern \"C\" {\n\t"),
            .mid = MD_S8Lit("\n\t"),
            .post = MD_S8Lit("\n}\n\n")
        };
        MD_String8 function_decls = MD_S8ListJoin(arena, line_list, &join);
        MD_S8ListPush(arena, &output_string_list, function_decls);
    }
    
    
    // NOTE(jack): Generate the highlight queries
    {
        for (Language_Information *info = languages_list.first; info; info = info->next)
        {
            MD_S8ListPushFmt(arena, &output_string_list, 
                             "static String_Const_u8 %S_highlight_query = string_u8_litexpr(R\"(%S)\");\n\n",
                             info->language_name, info->highlight_query);
            
        }
    }
    
    MD_S8ListPush(arena, &output_string_list,MD_S8Lit("function void\njpts_register_languages(Application_Links *app) {\n"));
    MD_S8ListPush(arena, &output_string_list,MD_S8Lit("\tScratch_Block scratch(app);\n"));
    // NOTE(jack): Generate the blocks which register languages
    {
        for (Language_Information *info = languages_list.first; info; info = info->next)
        {
            MD_String8List line_list = { 0 };
            MD_S8ListPush(arena, &line_list, MD_S8Lit("List_String_Const_u8 extension_list = {};"));
            for (MD_String8Node *ext = info->extensions.first; ext; ext = ext->next)
            {
                MD_S8ListPushFmt(arena, &line_list, "string_list_push(scratch, &extension_list, string_u8_litexpr(\"%S\"));",
                                 ext->string);
            }
            MD_S8ListPushFmt(arena, &line_list, "jpts_register_language_group(app, extension_list, %S_highlight_query, %S());",
                             info->language_name, info->function_name);
            
            MD_StringJoin join = {
                .pre = MD_S8Lit("\n\t{\n\t\t"),
                .mid = MD_S8Lit("\n\t\t"),
                .post = MD_S8Lit("\n\t}\n"),
            };
            MD_String8 block_string = MD_S8ListJoin(arena, line_list, &join);
            
            MD_S8ListPush(arena, &output_string_list, block_string);
        }
    }
    
    MD_S8ListPush(arena, &output_string_list, MD_S8Lit("}\n"));
    MD_String8 output_string = MD_S8ListJoin(arena, output_string_list, 0);
    
#if 0
    printf("\n=========Mock Output=========\n");
    printf("%.*s", MD_S8VArg(output_string));
    printf("\n=============================\n");
#endif
    
    FILE *file = fopen("../generated/4coder_tree_sitter_languages_generated.h", "w");
    if (!file) {
        printf("Error, failed to open file '../generated/jpts_languages.h'\n");
    }
    fprintf(file, "%.*s", MD_S8VArg(output_string));
    fclose(file);
    
    
    MD_ArenaClear(arena);
    return 0;
}
