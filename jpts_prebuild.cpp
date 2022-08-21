#define _CRT_SECURE_NO_WARNINGS
#include "metadesk/md.h"
#include "metadesk/md.c"
#include <stdio.h>

//~ NOTE(jack): Hash table stuff

#define HASH_TABLE_SIZE 1024

typedef struct table_entry_t  {
    struct table_entry_t *next;
    MD_u64 hash;
    MD_String8 key;
} TableEntry;

typedef struct hash_table_t {
    TableEntry *entries[HASH_TABLE_SIZE];
    MD_u64 size = HASH_TABLE_SIZE;
} HashTable;

void HashTableInsert_(HashTable *table, MD_String8 key, TableEntry *entry) 
{
    entry->hash = MD_HashStr(key);
    entry->key = key; 
    
    MD_u64 slot = entry->hash % table->size;
    entry->next = table->entries[slot];
    table->entries[slot] = entry;
}

TableEntry *HashTableLookup_(HashTable *table, MD_String8 key) 
{
    TableEntry *result = 0;
    MD_u64 hash = MD_HashStr(key);
    MD_u64 slot = hash % table->size;
    
    for (TableEntry *entry = table->entries[slot];
         entry;
         entry = entry->next)
    {
        if (entry->hash == hash && MD_S8Match(entry->key, key, 0)) {
            result = entry;
            break;
        }
    }
    
    return result;
}

// NOTE(jack): Hash table object _MUST_ have the TableEntry object as their first member.
#define HashTableInsert(table, key, obj) HashTableInsert_(table, key, (TableEntry *)obj)
#define HashTableLookup(table, key, ObjType) (ObjType *)HashTableLookup_(table, key)

MD_Arena *gLanguageArena;
HashTable gLanguageTable;
HashTable gLanguageFileTable;

//~ NOTE(jack): Types
typedef struct language_information_t {
    TableEntry hash_item;
    
    // NOTE(jack): Used to construct a list of language to iterate when generating the 
    // output code
    struct language_information_t *next;
    
    MD_String8 function_name;
    MD_String8List extensions;
    MD_String8 highlight_query;
} LanguageInformation;

typedef struct language_information_list_t {
    LanguageInformation *first;
    LanguageInformation *last;
} LanguageInformationList;

//~ NOTE(jack): Globals

typedef struct language_file_t {
    TableEntry hash_item;
    
    MD_Node *node;
    MD_String8 extends_from;
} LanguageFile;

//~ NOTE(jack): Functions
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

LanguageInformation *
ParseLanguage(MD_Arena *arena, MD_String8 language_name) 
{
    // NOTE(jack): Look to see if we've already parsed the language, if we have, just return its information
    LanguageInformation *language_info = HashTableLookup(&gLanguageTable, language_name, LanguageInformation);
    if (language_info) {
        printf("Info [ParseLanguage]: language '%.*s' already parsed, returning from hash table.\n", MD_S8VArg(language_name));
        return language_info;
    }
    
    LanguageFile *language_file = HashTableLookup(&gLanguageFileTable, language_name, LanguageFile);
    if (language_file == 0) {
        printf("Error: lanuage '%.*s' does not exist in the language file table.\n", MD_S8VArg(language_name));
        return 0;
    }
    MD_Node *language = language_file->node;
    
    //-
    // NOTE(jack): If this language is an extension on another language we need to concatonate their queries.
    
    MD_String8 parent_highlight_query = { 0 }; 
    if(language_file->extends_from.size > 0)
    {
        LanguageInformation *parent_language = HashTableLookup(&gLanguageTable, language_file->extends_from, LanguageInformation);
        if (parent_language == 0) {
            parent_language = ParseLanguage(arena, language_file->extends_from);
        }
        
        if (parent_language == 0) {
            printf("Error: lanuage '%.*s' extends from non-existant language %.*s\n",
                   MD_S8VArg(language_name), MD_S8VArg(language_file->extends_from));
            return 0;
        }
        
        parent_highlight_query = parent_language->highlight_query;
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
    
    if (parent_highlight_query.size != 0) {
        highlight_query = MD_S8Fmt(arena, "%.*s\n%.*s",
                                   MD_S8VArg(parent_highlight_query),
                                   MD_S8VArg(lang_base_highlight_query));
    } else {
        highlight_query = lang_base_highlight_query;
    }
    
    language_info                  = MD_PushArrayZero(gLanguageArena, LanguageInformation, 1);
    language_info->function_name   = function_name;
    language_info->highlight_query = highlight_query;
    language_info->extensions      = language_extensions;
    
    HashTableInsert(&gLanguageTable, language_name, language_info);
    
    return language_info;
}

MD_Node *ParseWholeFile(MD_Arena *arena, MD_String8 file_name)
{
    MD_ParseResult parse = MD_ParseWholeFile(arena, file_name);
    if (parse.errors.node_count != 0)
    {
        printf("Metadesk Parse Errors:\n");
        for(MD_Message *Message = parse.errors.first;
            Message;
            Message = Message->next)
        {
            printf("\t%.*s\n", MD_S8VArg(Message->string));
        }
        fflush(stdout);
        return MD_NilNode();
    }
    
    return parse.node;
}

//~ Main
int main()
{
    printf("Metadesk Build step:\n");
    gLanguageArena = MD_ArenaAlloc();
    
    MD_Arena *arena = MD_ArenaAlloc();
    MD_Node *common_root = ParseWholeFile(arena, MD_S8Lit("../lang/common.mdesk"));
    if (MD_NodeIsNil(common_root)) {
        printf("Failed to parse common.mdesk");
        return 1; // Failed to parse file, errors logged.
    }
    
    //- TODO(jack): Generate Color mapping table
    //- 
    
    LanguageInformationList language_information_list = { 0 };
    
    MD_Node *languages = MD_ChildFromString(common_root, MD_S8Lit("languages"), 0);
    if (MD_NodeIsNil(languages))
    {
        printf("Common.mdesk does not contain a language_files child\n");
        exit(1);
    }
    
    
    MD_Assert(!MD_NodeIsNil(languages->first_child));
    MD_Node *languages_list = languages->first_child;
    
    //- 
    //NOTE(jack): Parse the language files and put them into the hash table
    for (MD_EachNode(language, languages_list))
    {
        MD_String8 language_name = language->string;
        MD_Assert(!MD_NodeIsNil(language->first_child));
        
        MD_String8 language_file_name = language->first_child->string;
        MD_Node *language_file = ParseWholeFile(arena, language_file_name);
        MD_Assert(!MD_NodeIsNil(language_file));
        
        LanguageFile *file = MD_PushArrayZero(arena, LanguageFile, 1);
        file->node = language_file;
        MD_Node *extends = MD_TagFromString(language, MD_S8Lit("extends"), 0);
        
        if (!MD_NodeIsNil(extends)) {
            MD_Assert(!MD_NodeIsNil(extends->first_child));
            file->extends_from = extends->first_child->string;
        }
        
        HashTableInsert(&gLanguageFileTable, language_name, file);
    }
    
    //- NOTE(jack): Parse the language information out of the node tree
    for (MD_EachNode(language, languages_list))
    {
        MD_String8 language_name = language->string;
        MD_String8 extends_language_name = {}; 
        
        LanguageInformation *language_info = ParseLanguage(arena, language_name);
        if (language_info == 0)
        {
            printf("Failed to parse language: %.*s\n", MD_S8VArg(language_name));
            exit(1);
        }
        MD_QueuePush(language_information_list.first, language_information_list.last, language_info);
        
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
    }
    
    
    //- NOTE(jack): generate the output file 
    MD_String8List output_string_list = { 0 };
    
    // NOTE(jack): Generate the extern "C" function declarations
    {
        MD_String8List line_list = { 0 };
        for (LanguageInformation *info = language_information_list.first; info; info = info->next)
        {
            MD_S8ListPushFmt(arena, &line_list, "TSLanguage *%S();", info->function_name);
        }
        
        MD_StringJoin join = { 0 };
        join.pre = MD_S8Lit("extern \"C\" {\n\t");
        join.mid = MD_S8Lit("\n\t");
        join.post = MD_S8Lit("\n}\n\n");
        
        MD_String8 function_decls = MD_S8ListJoin(arena, line_list, &join);
        MD_S8ListPush(arena, &output_string_list, function_decls);
    }
    
    
    // NOTE(jack): Generate the highlight queries
    {
        for (LanguageInformation *info = language_information_list.first; info; info = info->next)
        {
            MD_S8ListPushFmt(arena, &output_string_list, 
                             "static String_Const_u8 %S_highlight_query = string_u8_litexpr(R\"(%S)\");\n\n",
                             info->hash_item.key, info->highlight_query);
        }
    }
    
    MD_S8ListPush(arena, &output_string_list, MD_S8Lit("function void\njpts_register_languages(Application_Links *app) {\n"));
    MD_S8ListPush(arena, &output_string_list, MD_S8Lit("\tScratch_Block scratch(app);\n"));
    // NOTE(jack): Generate the blocks which register languages
    {
        for (LanguageInformation *info = language_information_list.first; info; info = info->next)
        {
            MD_String8List line_list = { 0 };
            MD_S8ListPush(arena, &line_list, MD_S8Lit("List_String_Const_u8 extension_list = {};"));
            for (MD_String8Node *ext = info->extensions.first; ext; ext = ext->next)
            {
                MD_S8ListPushFmt(arena, &line_list, "string_list_push(scratch, &extension_list, string_u8_litexpr(\"%S\"));",
                                 ext->string);
            }
            MD_S8ListPushFmt(arena, &line_list, "jpts_register_language_group(app, extension_list, %S_highlight_query, %S());",
                             info->hash_item.key, info->function_name);
            
            MD_StringJoin join = { 0 };
            join.pre = MD_S8Lit("\n\t{\n\t\t");
            join.mid = MD_S8Lit("\n\t\t");
            join.post = MD_S8Lit("\n\t}\n");
            
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