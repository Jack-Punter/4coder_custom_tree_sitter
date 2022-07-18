// TODO(jack): Currently this code is not correct, if the query has multiple matches for the same node type
// this will highlight whaterver the last entry is, not the most specific
// (i.e. is it a child in a query or just query for that node type)
function void
jpts_highlight_node(Application_Links *app, TSQuery *highlight_query, TSNode top_node, TSQueryCursor * query_cursor, Text_Layout_ID text_layout_id, Range_i64 visible_range)
{
    ProfileScope(app, "ts_highlight_node");
    ts_query_cursor_exec(query_cursor, highlight_query, top_node);
    TSQueryMatch match;
    u32 capture_index;
    
    // TODO(jack): Do these come back in lexicographic order? 
    // We can skip forwards for nodes leading the visible range and early out after the visibel range.
    while (ts_query_cursor_next_capture(query_cursor, &match, &capture_index))
    {
        TSQueryCapture capture = match.captures[capture_index];
        TSNode node = capture.node;
        u32 length;
        const char *tmp = ts_query_capture_name_for_id(highlight_query, capture.index, &length);
        String_Const_u8 capture_name = SCu8((char *)tmp, length);
        
        Range_i64 highlight_range = jpts_node_to_range(node);
        Managed_ID color_id = managed_id_get(app, SCu8("colors"), capture_name);
        if (color_id != 0) {
            paint_text_color_fcolor(app, text_layout_id, highlight_range, fcolor_id(color_id));
        }
    }
}

function void
jpts_draw_node_colors(Application_Links *app, Text_Layout_ID text_layout_id, Buffer_ID buffer)
{
    ProfileScope(app, "ts_draw_cpp_token_colors");
    
    Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    
    Language_Definition *language = jpts_language_from_buffer(app, buffer);
    if (language)
    {
        JPTS_Data *tree_data = scope_attachment(app, scope, ts_data, JPTS_Data);
        // NOTE(jack): the first time the file opens, the Async parser gets triggered, so may not be immediatly ready.
        TSTree *tree = jpts_buffer_get_tree(tree_data);
        if (tree)
        {
            Scratch_Block scratch(app);
            String_Const_u8 visible_range_string = push_buffer_range(app, scratch, buffer, visible_range);
            u64 first_non_whitespace = string_find_first_non_whitespace(visible_range_string);
            
            // TODO(jack): The root node is the file, although this may not be the be true in multi-lang files,
            // but I do _NOT_ want to deal with those yet.
            TSNode root = ts_tree_root_node(tree);
            
            TSNode visible = ts_node_descendant_for_byte_range(root, (u32)visible_range.start, (u32)visible_range.end);
            TSQueryCursor *query_cursor = ts_query_cursor_new();
            TSTreeCursor tree_cursor = ts_tree_cursor_new(visible);
            
            ts_tree_cursor_goto_first_child_for_byte(&tree_cursor, (u32)(visible_range.start + first_non_whitespace));
            b32 cont = true;
            do {
                TSNode node = ts_tree_cursor_current_node(&tree_cursor);
                Range_i64 node_range = jpts_node_to_range(node);
                if (node_range.start > visible_range.end) {
                    break;
                }
                jpts_highlight_node(app, language->highlight_query, node, query_cursor, text_layout_id, visible_range);
                cont =  ts_tree_cursor_goto_next_sibling(&tree_cursor);
            } while(cont);
            
            ts_tree_cursor_delete(&tree_cursor);
            ts_query_cursor_delete(query_cursor);
            ts_tree_delete(tree);
        }
    }
}

function void
jpts_render_buffer(Application_Links *app, View_ID view_id, Face_ID face_id,
                   Buffer_ID buffer, Text_Layout_ID text_layout_id,
                   Rect_f32 rect)
{
    ProfileScope(app, "render buffer");
    
    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);
    Rect_f32 prev_clip = draw_set_clip(app, rect);
    
    Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
    
    // NOTE(allen): Cursor shape
    Face_Metrics metrics = get_face_metrics(app, face_id);
    u64 cursor_roundness_100 = def_get_config_u64(app, vars_save_string_lit("cursor_roundness"));
    f32 cursor_roundness = metrics.normal_advance*cursor_roundness_100*0.01f;
    f32 mark_thickness = (f32)def_get_config_u64(app, vars_save_string_lit("mark_thickness"));
    
    // NOTE(allen): Token colorizing
    Token_Array token_array = get_token_array_from_buffer(app, buffer);
    
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    JPTS_Data*tree_data = scope_attachment(app, scope, ts_data, JPTS_Data);
    TSTree *tree = jpts_buffer_get_tree(tree_data);
    
    // Paint the default color for the entire ranage, specific colors will be overwritten by the tree query
    paint_text_color_fcolor(app, text_layout_id, visible_range, fcolor_id(defcolor_text_default));
    if (tree && token_array.tokens != 0){
        if (use_ts_highlighting) {
            jpts_draw_node_colors(app, text_layout_id, buffer);
        } else {
            draw_cpp_token_colors(app, text_layout_id, &token_array);
        }
        
        // NOTE(allen): Scan for TODOs and NOTEs
        b32 use_comment_keyword = def_get_config_b32(vars_save_string_lit("use_comment_keywords"));
        if (use_comment_keyword){
            ProfileScope(app, "draw_comment_highlights");
            Comment_Highlight_Pair pairs[] = {
                {string_u8_litexpr("NOTE"), finalize_color(defcolor_comment_pop, 0)},
                {string_u8_litexpr("TODO"), finalize_color(defcolor_comment_pop, 1)},
            };
            draw_comment_highlights(app, buffer, text_layout_id, &token_array, pairs, ArrayCount(pairs));
        }
    }
    
    i64 cursor_pos = view_correct_cursor(app, view_id);
    view_correct_mark(app, view_id);
    
    // NOTE(allen): Scope highlight
    b32 use_scope_highlight = def_get_config_b32(vars_save_string_lit("use_scope_highlight"));
    if (use_scope_highlight){
        Color_Array colors = finalize_color_array(defcolor_back_cycle);
        draw_scope_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
    }
    
    b32 use_error_highlight = def_get_config_b32(vars_save_string_lit("use_error_highlight"));
    b32 use_jump_highlight = def_get_config_b32(vars_save_string_lit("use_jump_highlight"));
    if (use_error_highlight || use_jump_highlight){
        // NOTE(allen): Error highlight
        String_Const_u8 name = string_u8_litexpr("*compilation*");
        Buffer_ID compilation_buffer = get_buffer_by_name(app, name, Access_Always);
        if (use_error_highlight){
            draw_jump_highlights(app, buffer, text_layout_id, compilation_buffer,
                                 fcolor_id(defcolor_highlight_junk));
        }
        
        // NOTE(allen): Search highlight
        if (use_jump_highlight){
            Buffer_ID jump_buffer = get_locked_jump_buffer(app);
            if (jump_buffer != compilation_buffer){
                draw_jump_highlights(app, buffer, text_layout_id, jump_buffer,
                                     fcolor_id(defcolor_highlight_white));
            }
        }
    }
    
    // NOTE(allen): Color parens
    b32 use_paren_helper = def_get_config_b32(vars_save_string_lit("use_paren_helper"));
    if (use_paren_helper){
        Color_Array colors = finalize_color_array(defcolor_text_cycle);
        draw_paren_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
    }
    
    // NOTE(allen): Line highlight
    b32 highlight_line_at_cursor = def_get_config_b32(vars_save_string_lit("highlight_line_at_cursor"));
    if (highlight_line_at_cursor && is_active_view){
        i64 line_number = get_line_number_from_pos(app, buffer, cursor_pos);
        draw_line_highlight(app, text_layout_id, line_number, fcolor_id(defcolor_highlight_cursor_line));
    }
    
    // NOTE(allen): Whitespace highlight
    b64 show_whitespace = false;
    view_get_setting(app, view_id, ViewSetting_ShowWhitespace, &show_whitespace);
    if (show_whitespace){
        if (token_array.tokens == 0){
            draw_whitespace_highlight(app, buffer, text_layout_id, cursor_roundness);
        }
        else{
            draw_whitespace_highlight(app, text_layout_id, &token_array, cursor_roundness);
        }
    }
    
    // NOTE(allen): Cursor
    switch (fcoder_mode){
        case FCoderMode_Original:
        {
            draw_original_4coder_style_cursor_mark_highlight(app, view_id, is_active_view, buffer, text_layout_id, cursor_roundness, mark_thickness);
        }break;
        case FCoderMode_NotepadLike:
        {
            draw_notepad_style_cursor_highlight(app, view_id, buffer, text_layout_id, cursor_roundness);
        }break;
    }
    
    // NOTE(allen): Fade ranges
    paint_fade_ranges(app, text_layout_id, buffer);
    
    // NOTE(allen): put the actual text on the actual screen
    {
        ProfileScope(app, "draw_text_layout_default");
        draw_text_layout_default(app, text_layout_id);
    }
    
    {
        ProfileScope(app, "QueryResults");
        Buffer_ID query_buf = get_buffer_by_name(app, SCu8("*query*"), Access_Always);
        i64 count = buffer_get_size(app, query_buf);
        if (tree && count != 0) {
            
            TSNode root = ts_tree_root_node(tree);
            
            /*
            Scratch_Block scratch(app);
            String_Const_u8 query_src = push_buffer_range(app, scratch, query_buf, {0, count});
            u32 error_offest;
            TSQueryError errorType;
            TSQuery *query = ts_query_new(tree_data->language, (char *)query_src.str, (u32)query_src.size,
                                        &error_offest, &errorType);
            */
            TSQuery *query = active_query;
            if (query) {
                TSQueryCursor *query_cursor = ts_query_cursor_new();
                ts_query_cursor_exec(query_cursor, query, root);
                
                TSQueryMatch match;
                u32 capture_index;
                while (ts_query_cursor_next_capture(query_cursor, &match, &capture_index)) {
                    TSQueryCapture capture = match.captures[capture_index];
                    TSNode node = capture.node;
                    u32 length;
                    const char *tmp = ts_query_capture_name_for_id(query, capture.index, &length);
                    String_Const_u8 capture_name = SCu8((char *)tmp, length);
                    u32 color = 0x80000000 | ((u32)string_to_integer(capture_name, 16) & 0x00FFFFFF);
                    
                    u32 start = ts_node_start_byte(node);
                    u32 end = ts_node_end_byte(node);
                    Rect_f32 y1 = text_layout_character_on_screen(app, text_layout_id, start);
                    Rect_f32 y2 = text_layout_character_on_screen(app, text_layout_id, end);
                    draw_rectangle(app, y1, 2, color);
                    draw_rectangle(app, y2, 2, color);
                }
                
                //ts_query_delete(query);
            }
        }
    }
    ts_tree_delete(tree);
    draw_set_clip(app, prev_clip);
}