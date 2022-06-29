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
// TODO(jack): do something smarter here for queries
// (metadesk build step to generate a header containing these?)
String_Const_u8 TS_CPP_HIGHLIGHT_QUERY = string_u8_litexpr("(call_expression function: ["
                                                           " (identifier) @fleury_color_index_function"
                                                           " (field_expression field: (field_identifier) @fleury_color_index_function)])"
                                                           "(function_declarator"
                                                           " declarator: [(identifier) (field_identifier)] @fleury_color_index_function)"
                                                           
                                                           "(preproc_def"
                                                           " name: (identifier) @fleury_color_index_macro)"
                                                           "(preproc_function_def"
                                                           " name: (identifier) @fleury_color_index_macro)"
                                                           
                                                           "(type_identifier) @fleury_color_index_product_type"
                                                           "(call_expression"
                                                           " function: (parenthesized_expression"
                                                           "  (identifier) @fleury_color_index_product_type))"
                                                           
                                                           "[(primitive_type) (type_qualifier) (storage_class_specifier)"
                                                           " (break_statement) (continue_statement) \"union\" \"return\" \"do\""
                                                           " \"while\" \"for\" \"if\" \"class\" \"struct\" \"enum\" \"sizeof\""
                                                           " \"else\" \"switch\" \"case\"] @defcolor_keyword"
                                                           
                                                           "[(number_literal) (string_literal)] @defcolor_str_constant"
                                                           "[(preproc_directive) \"#define\" \"#if\" \"#elif\" \"#else\" \"#endif\""
                                                           " \"#include\"] @defcolor_preproc"
                                                           "[\"{\" \"}\" \";\" \":\" \",\"] @fleury_color_syntax_crap"
                                                           "(comment) @defcolor_comment");

String_Const_u8 TS_ODIN_HIGHLIGHT_QUERY = string_u8_litexpr("(type_identifier) @fleury_color_index_product_type"
                                                            "(field_identifier) @defcolor_text_default"
                                                            "(identifier) @defcolor_text_default"
                                                            
                                                            "\"any\" @fleury_color_index_product_type"
                                                            "(directive_identifier) @fleury_color_index_constant"
                                                            
                                                            "[ \"?\" \"-\" \"-=\" \":=\" \"!\" \"!=\" \"*\" \"*\" \"*=\" \"/\" \"/=\""
                                                            "  \"&\" \"&&\" \"&=\" \"%\" \"%=\" \"^\" \"+\" \"+=\" \"<-\" \"<\" \"<<\""
                                                            "  \"<<=\" \"<=\" \"=\" \"==\" \">\" \">=\" \">>\" \">>=\" \"|\" \"|=\""
                                                            "  \"||\" \"~\" \"..\" \"..<\" \"..=\" \"::\" ] @fleury_color_operators"
                                                            
                                                            "[ \"auto_cast\" \"cast\" "
                                                            "  \"in\" \"distinct\" \"foreign\" \"transmute\""
                                                            "  \"break\" \"case\" \"continue\" \"defer\" \"else\" \"using\""
                                                            "  \"when\" \"where\" \"fallthrough\" \"for\" \"proc\" \"if\" \"import\""
                                                            "  \"map\" \"package\" \"return\" \"struct\" \"union\" \"enum\" \"switch\""
                                                            "  \"dynamic\" ] @defcolor_keyword"
                                                            
                                                            "[ (interpreted_string_literal) (raw_string_literal) (rune_literal) ] @defcolor_string_constant"
                                                            "(escape_sequence) @escape"
                                                            "[ (int_literal) (float_literal) (imaginary_literal) ] @defcolor_int_constant"
                                                            "[ (true) (false) (nil) (undefined) ] @fleury_color_index_constant"
                                                            "(comment) @defcolor_comment"
                                                            "(call_expression function: (identifier) @fleury_color_index_function)"
                                                            "(call_expression function: (selector_expression "
                                                            " field: (field_identifier) @fleury_color_index_function))"
                                                            
                                                            "(function_declaration name: (identifier) @fleury_color_index_function)"
                                                            " (proc_group (identifier) @fleury_color_index_function)"
                                                            "(const_declaration (identifier) @fleury_color_index_constant)"
                                                            "(const_declaration_with_type (identifier) @fleury_color_index_constant)"
                                                            );

internal void
jpts_register_languages(Application_Links *app)
{
    // NOTE(jack): C / C++
    {
        String_Const_u8 extensions[] =
        {
            string_u8_litexpr("cpp"), string_u8_litexpr("cc"), string_u8_litexpr("c"), string_u8_litexpr("cxx"),
            string_u8_litexpr("C"), string_u8_litexpr("h"), string_u8_litexpr("hpp"),
        };
        
        TSLanguage *language = tree_sitter_cpp();
        // NOTE(jack): Assert that the tree-sitter language is comaptible with this version of the library
        Assert((ts_language_version(language) >= TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION) &&
               (ts_language_version(language) <= TREE_SITTER_LANGUAGE_VERSION));
        
        u32 error_offset;
        TSQueryError query_error;
        TSQuery *language_query = ts_query_new(language, (const char *)TS_CPP_HIGHLIGHT_QUERY.str,
                                               (u32)TS_CPP_HIGHLIGHT_QUERY.size,
                                               &error_offset, &query_error);
        
        if (!language_query)
        {
            char buffer[1024];
            int len = snprintf(buffer, 1024, "Failed to create highlight query error (%d) at offset: %u",
                               query_error, error_offset);
            
            print_message(app, SCu8(buffer, len));
            AssertMessageAlways(failed to create query);
        }
        
        for(int i = 0; i < ArrayCount(extensions); i += 1)
        {
            jpts_register_language(extensions[i], language, language_query);
        }
        
    }
    
    // NOTE(jack): Odin
    {
        TSLanguage *language = tree_sitter_odin();
        // NOTE(jack): Assert that the tree-sitter language is comaptible with this version of the library
        Assert((ts_language_version(language) >= TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION) &&
               (ts_language_version(language) <= TREE_SITTER_LANGUAGE_VERSION));
        
        u32 error_offset;
        TSQueryError query_error;
        TSQuery *language_query = ts_query_new(language, (const char *)TS_ODIN_HIGHLIGHT_QUERY.str,
                                               (u32)TS_ODIN_HIGHLIGHT_QUERY.size,
                                               &error_offset, &query_error);
        
        if (!language_query)
        {
            char buffer[1024];
            int len = snprintf(buffer, 1024, "Failed to create highlight query error (%d) at offset: %u",
                               query_error, error_offset);
            
            print_message(app, SCu8(buffer, len));
            AssertMessageAlways(failed to create query);
        }
        
        jpts_register_language(string_u8_litexpr("odin"), language, language_query);
    }
}

#endif
