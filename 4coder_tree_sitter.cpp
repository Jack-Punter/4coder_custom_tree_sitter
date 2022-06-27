/*
4coder_default_bidings.cpp - Supplies the default bindings used for default 4coder behavior.
*/

// TOP

#if !defined(FCODER_DEFAULT_BINDINGS_CPP)
#define FCODER_DEFAULT_BINDINGS_CPP

#include "metadesk/md.h"
#include "metadesk/md.c"

#include "4coder_default_include.cpp"

#include "tree_sitter/api.h"
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

struct Buffer_TS_Data {
    TSTree *tree;
    TSLanguage *language;
    TSParser *parser;
    TSQuery *highlight_query;
    
    // TODO(jack): is this mutex actually needed?
    // ts_tree_copy does an atomic reference count
    System_Mutex tree_mutex;
};

function void
ts_buffer_tree_lock(Buffer_TS_Data *tree_data) {
    system_mutex_acquire(tree_data->tree_mutex);
}

function void
ts_buffer_tree_unlock(Buffer_TS_Data *tree_data) {
    system_mutex_release(tree_data->tree_mutex);
}

function TSTree *
ts_buffer_get_tree(Buffer_TS_Data *tree_data) {
    TSTree *result = 0;
    // ts_buffer_tree_lock(tree_data);
    if (tree_data->tree) {
        result = ts_tree_copy(tree_data->tree);
    }
    // ts_buffer_tree_unlock(tree_data);
    return result;
}

extern "C" {
    TSLanguage *tree_sitter_cpp();
    TSLanguage *tree_sitter_c();
}

TSQuery *active_query;


global b32 use_ts_highlighting = true;

// TODO(jack): ts_code_indexing was a first pass experiment, and currently only
// actually indexes nests
global b32 use_ts_code_indexing = false;

CUSTOM_COMMAND_SIG(toggle_highlight_mode)
CUSTOM_DOC("Toggle highlighting strategy")
{
    use_ts_highlighting = !use_ts_highlighting;
    print_message(app, use_ts_highlighting ? SCu8("Using Tree-Sitter based highlighting\n") :
                  SCu8("Using Default 4coder Highlighting\n"));
}

CUSTOM_COMMAND_SIG(toggle_index_mode)
CUSTOM_DOC("Toggle Code Indexing Mode")
{
    use_ts_code_indexing = !use_ts_code_indexing;
    print_message(app, use_ts_code_indexing ? SCu8("Using Tree-Sitter based code indexing\n") :
                  SCu8("Using Default 4coder based code indexing\n"));
}

// NOTE(allen): Users can declare their own managed IDs here.

//- Tree sitter extension attachements
CUSTOM_ID(attachment, ts_data);
CUSTOM_ID(attachment, ts_async_parse_task); 

//- Colors
CUSTOM_ID(colors, fleury_color_syntax_crap);
CUSTOM_ID(colors, fleury_color_operators);
CUSTOM_ID(colors, fleury_color_inactive_pane_overlay);
CUSTOM_ID(colors, fleury_color_inactive_pane_background);
CUSTOM_ID(colors, fleury_color_file_progress_bar);
CUSTOM_ID(colors, fleury_color_brace_highlight);
CUSTOM_ID(colors, fleury_color_brace_line);
CUSTOM_ID(colors, fleury_color_brace_annotation);
CUSTOM_ID(colors, fleury_color_index_product_type);
CUSTOM_ID(colors, fleury_color_index_sum_type);
CUSTOM_ID(colors, fleury_color_index_function);
CUSTOM_ID(colors, fleury_color_index_macro);
CUSTOM_ID(colors, fleury_color_index_constant);
CUSTOM_ID(colors, fleury_color_index_comment_tag);
CUSTOM_ID(colors, fleury_color_cursor_macro);
CUSTOM_ID(colors, fleury_color_cursor_power_mode);
CUSTOM_ID(colors, fleury_color_cursor_inactive);
CUSTOM_ID(colors, fleury_color_plot_cycle);
CUSTOM_ID(colors, fleury_color_token_highlight);
CUSTOM_ID(colors, fleury_color_token_minor_highlight);
CUSTOM_ID(colors, fleury_color_lego_grab);
CUSTOM_ID(colors, fleury_color_lego_splat);
CUSTOM_ID(colors, fleury_color_error_annotation);
CUSTOM_ID(colors, fleury_color_comment_user_name);

function void
jp_write_ts_tree_to_buffer__inner(Application_Links *app, Arena *arena, Buffer_ID buffer_id,
                                  TSNode cur_node, i32 level = 0, const char *field="")
{
    char prefix_buffer[1024] = { 0 };
    for (int i = 0; i < level*2; i+=2)
    {
        prefix_buffer[i] = ' ';
        prefix_buffer[i+1] = ' ';
    }
    TSPoint start = ts_node_start_point(cur_node);
    TSPoint end = ts_node_end_point(cur_node);
    // + 1 on ts positions becuase the first line/column are zero in treesitter, but 4coder displayes as
    // 1 indexed in the filebar.
    String_Const_u8 string = push_stringf(arena, "%s%s: %s [%d, %d] - [%d, %d]\n",
                                          prefix_buffer, field, ts_node_type(cur_node),
                                          start.row + 1, start.column + 1,
                                          end.row + 1, end.column + 1);
    
    buffer_replace_range(app, buffer_id, Ii64(buffer_get_size(app, buffer_id)), string);
    
    u32 child_count = ts_node_child_count(cur_node);
    for (u32 i = 0; i < child_count; ++i)
    {
        TSNode child = ts_node_child(cur_node, i);
        if (ts_node_is_named(child))
        {
            field = ts_node_field_name_for_child(cur_node, i);
            if (!field) {
                field = "";
            }
            jp_write_ts_tree_to_buffer__inner(app, arena, buffer_id, child, level + 1, field);
        }
    }
}

function void
jp_write_ts_tree_to_buffer(Application_Links *app, Buffer_ID buffer_id, TSTree *tree)
{
    ProfileScope(app, "write_TS_Tree");
    Scratch_Block scratch(app);
    clear_buffer(app, buffer_id);
    
    TSNode root = ts_tree_root_node(tree);
    jp_write_ts_tree_to_buffer__inner(app, scratch, buffer_id, root);
}
function Range_i64
ts_node_to_range(TSNode node) {
    Range_i64 result = {ts_node_start_byte(node), ts_node_end_byte(node)};
    return result;
}

#include "4coder_ts_async_parse.cpp"
#include "4coder_ts_render.cpp"
#include "4coder_ts_hooks.cpp"

#if !defined(META_PASS)
#include "generated/managed_id_metadata.cpp"
#endif

void custom_layer_init(Application_Links *app)
{
    Thread_Context *tctx = get_thread_context(app);
    
    default_framework_init(app);
    mapping_init(tctx, &framework_mapping);
    
    // NOTE(rjf): Set up hooks.
    {
        set_all_default_hooks(app);
        //t $          ($  , $                             , $                     );
        set_custom_hook(app, HookID_Tick,                    ts_Tick);
        set_custom_hook(app, HookID_RenderCaller,            ts_RenderCaller);
        set_custom_hook(app, HookID_BeginBuffer,             ts_BeginBuffer);
        set_custom_hook(app, HookID_EndBuffer,               ts_EndBuffer);
        // set_custom_hook(app, HookID_Layout,                  F4_Layout);
        // set_custom_hook(app, HookID_WholeScreenRenderCaller, F4_WholeScreenRender);
        set_custom_hook(app, HookID_BufferEditRange,         ts_BufferEditRange);
        // set_custom_hook_memory_size(app, HookID_DeltaRule, delta_ctx_size(sizeof(Vec2_f32)));
    }
    
    Scratch_Block scratch(app);
    String_Const_u8 bindings_file_name = string_u8_litexpr("bindings.4coder");
    String_Const_u8 mapping = def_get_config_string(scratch, vars_save_string_lit("mapping"));
    
    if (string_match(mapping, string_u8_litexpr("mac-default"))){
        bindings_file_name = string_u8_litexpr("mac-bindings.4coder");
    }
    else if (OS_MAC && string_match(mapping, string_u8_litexpr("choose"))){
        bindings_file_name = string_u8_litexpr("mac-bindings.4coder");
    }
    
    // TODO(allen): cleanup
    String_ID global_map_id = vars_save_string_lit("keys_global");
    String_ID file_map_id = vars_save_string_lit("keys_file");
    String_ID code_map_id = vars_save_string_lit("keys_code");
    
    if (dynamic_binding_load_from_file(app, &framework_mapping, bindings_file_name)){
        setup_essential_mapping(&framework_mapping, global_map_id, file_map_id, code_map_id);
    }
    else{
        setup_built_in_mapping(app, mapping, &framework_mapping, global_map_id, file_map_id, code_map_id);
    }
    
    Buffer_ID buffer = create_buffer(app, string_u8_litexpr("*tree*"),
                                     BufferCreate_NeverAttachToFile |
                                     BufferCreate_AlwaysNew);
    buffer_set_setting(app, buffer, BufferSetting_Unimportant, true);
    buffer_set_setting(app, buffer, BufferSetting_ReadOnly, true);
    
    buffer = create_buffer(app, string_u8_litexpr("*query*"),
                           BufferCreate_NeverAttachToFile |
                           BufferCreate_AlwaysNew);
    buffer_set_setting(app, buffer, BufferSetting_Unimportant, true);
    
    buffer = create_buffer(app, string_u8_litexpr("*highlight query*"),
                           BufferCreate_NeverAttachToFile |
                           BufferCreate_AlwaysNew);
    buffer_set_setting(app, buffer, BufferSetting_Unimportant, true);
    buffer_set_setting(app, buffer, BufferSetting_ReadOnly, true);
    
    print_message(app, string_u8_litexpr("init_complete"));
    
    TSLanguage *language_c = tree_sitter_c();
    (void)language_c;
}

#endif //FCODER_DEFAULT_BINDINGS

// BOTTOM

