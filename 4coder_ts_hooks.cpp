function void
ts_RenderCaller(Application_Links *app, Frame_Info frame_info, View_ID view_id){
    ProfileScope(app, "default render caller");
    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);
    
    Rect_f32 region = draw_background_and_margin(app, view_id, is_active_view);
    Rect_f32 prev_clip = draw_set_clip(app, region);
    
    Buffer_ID buffer = view_get_buffer(app, view_id, Access_Always);
    Face_ID face_id = get_face_id(app, buffer);
    Face_Metrics face_metrics = get_face_metrics(app, face_id);
    f32 line_height = face_metrics.line_height;
    f32 digit_advance = face_metrics.decimal_digit_advance;
    
    // NOTE(allen): file bar
    b64 showing_file_bar = false;
    if (view_get_setting(app, view_id, ViewSetting_ShowFileBar, &showing_file_bar) && showing_file_bar){
        Rect_f32_Pair pair = layout_file_bar_on_top(region, line_height);
        draw_file_bar(app, view_id, buffer, face_id, pair.min);
        region = pair.max;
    }
    
    Buffer_Scroll scroll = view_get_buffer_scroll(app, view_id);
    
    Buffer_Point_Delta_Result delta = delta_apply(app, view_id,
                                                  frame_info.animation_dt, scroll);
    if (!block_match_struct(&scroll.position, &delta.point)){
        block_copy_struct(&scroll.position, &delta.point);
        view_set_buffer_scroll(app, view_id, scroll, SetBufferScroll_NoCursorChange);
    }
    if (delta.still_animating){
        animate_in_n_milliseconds(app, 0);
    }
    
    // NOTE(allen): query bars
    region = default_draw_query_bars(app, region, view_id, face_id);
    
    // NOTE(allen): FPS hud
    if (show_fps_hud){
        Rect_f32_Pair pair = layout_fps_hud_on_bottom(region, line_height);
        draw_fps_hud(app, frame_info, face_id, pair.max);
        region = pair.min;
        animate_in_n_milliseconds(app, 1000);
    }
    
    // NOTE(allen): layout line numbers
    b32 show_line_number_margins = def_get_config_b32(vars_save_string_lit("show_line_number_margins"));
    Rect_f32 line_number_rect = {};
    if (show_line_number_margins){
        Rect_f32_Pair pair = layout_line_number_margin(app, buffer, region, digit_advance);
        line_number_rect = pair.min;
        region = pair.max;
    }
    
    // NOTE(allen): begin buffer render
    Buffer_Point buffer_point = scroll.position;
    Text_Layout_ID text_layout_id = text_layout_create(app, buffer, region, buffer_point);
    
    // NOTE(allen): draw line numbers
    if (show_line_number_margins){
        draw_line_number_margin(app, view_id, buffer, face_id, text_layout_id, line_number_rect);
    }
    
    // NOTE(allen): draw the buffer
    jpts_render_buffer(app, view_id, face_id, buffer, text_layout_id, region);
    
    text_layout_free(app, text_layout_id);
    draw_set_clip(app, prev_clip);
}

BUFFER_HOOK_SIG(ts_BeginBuffer){
    ProfileScope(app, "Tree-Sitter begin buffer");
    Scratch_Block scratch(app);
    Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
    JPTS_Data *tree_data = scope_attachment(app, scope, ts_data, JPTS_Data);
    tree_data->tree_mutex = system_mutex_make();
    
    
    b32 treat_as_code = false;
    String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer_id);
    if (file_name.size > 0){
        String_Const_u8 treat_as_code_string = def_get_config_string(scratch, vars_save_string_lit("treat_as_code"));
        String_Const_u8_Array extensions = parse_extension_line_to_extension_list(app, scratch, treat_as_code_string);
        String_Const_u8 ext = string_file_extension(file_name);
        for (i32 i = 0; i < extensions.count; ++i)
        {
            if (string_match(ext, extensions.strings[i]))
            {
                treat_as_code = true;
                
                
#if 0                
                if (string_match(ext, string_u8_litexpr("cpp")) ||
                    string_match(ext, string_u8_litexpr("h")) ||
                    string_match(ext, string_u8_litexpr("c")) ||
                    string_match(ext, string_u8_litexpr("hpp")) ||
                    string_match(ext, string_u8_litexpr("cc")))
                {
                    print_message(app, SCu8("Language Detected as Cpp\n"));
                    tree_data->language = tree_sitter_cpp();
                }
                
                if (tree_data->language)
                {
                    if (success)
                    {
                        u32 error_offset;
                        TSQueryError query_error;
                        
                        {
                            // TODO(jack): The highlight query should be cached per language this takes
                            // 20ms+ causing a noticable freeze when loading projects (takes about 4s on this custom layer)
                            // Buffer_TS_Data; 
                            ProfileScope(app, "Create Highlight Query");
                            tree_data->highlight_query = ts_query_new(tree_data->language, (char*)TS_CPP_HIGHLIGHT_QUERY.str,
                                                                      (u32)TS_CPP_HIGHLIGHT_QUERY.size,
                                                                      &error_offset, &query_error);
                        }
                        
                        if (tree_data->highlight_query == 0)
                        {
                            print_message(app, SCu8("Failed to create Highlight query\n"));
                        }
                        else
                        {
                            AssertMessageAlways("Failed to set create querty investigate\n");
                        }
                    } else {
                        print_message(app, string_u8_litexpr("Failed to set parser language\n"));
                        ts_parser_delete(tree_data->parser);
                        tree_data->parser = 0;
                        tree_data->language = 0;
                        AssertMessageAlways("Failed to set parser language, Investigate\n");
                    }
                }
#endif
                
                break;
            }
        }
    }
    
    String_ID file_map_id = vars_save_string_lit("keys_file");
    String_ID code_map_id = vars_save_string_lit("keys_code");
    
    Command_Map_ID map_id = (treat_as_code)?(code_map_id):(file_map_id);
    Command_Map_ID *map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID);
    *map_id_ptr = map_id;
    
    Line_Ending_Kind setting = guess_line_ending_kind_from_buffer(app, buffer_id);
    Line_Ending_Kind *eol_setting = scope_attachment(app, scope, buffer_eol_setting, Line_Ending_Kind);
    *eol_setting = setting;
    
    // NOTE(allen): Decide buffer settings
    b32 wrap_lines = true;
    b32 use_lexer = false;
    if (treat_as_code) {
        wrap_lines = def_get_config_b32(vars_save_string_lit("enable_code_wrapping"));
        use_lexer = true;
    }
    
    // NOTE(jack): When a buffer the async lexer completes (triggerd further down in this function)
    // it marks the buffer as modified to trigger the default code index.
    // As we have added the tree-sitter pares to this as well, we dont need to imediately parse the 
    // buffer, this will increase time-to-open on large files, although syntax highlighting may take
    // some time to appear (as the buffer must be fully parsed before the tree-data is added to the
    // buffer attachment);
    
    Language_Definition *language = jpts_language_from_buffer(scratch, app, buffer_id);
    if(language)
    {
        print_message(app, string_u8_litexpr("Starting async parse\n"));
        Async_Task *parse_task = scope_attachment(app, scope, ts_async_parse_task, Async_Task);
        *parse_task = async_task_no_dep(&global_async_system, ts_parse_async, make_data_struct(&buffer_id));
    }
    
    String_Const_u8 buffer_name = push_buffer_base_name(app, scratch, buffer_id);
    if (buffer_name.size > 0 && buffer_name.str[0] == '*' && buffer_name.str[buffer_name.size - 1] == '*'){
        wrap_lines = def_get_config_b32(vars_save_string_lit("enable_output_wrapping"));
    }
    
    if (use_lexer){
        ProfileBlock(app, "begin buffer kick off lexer");
        Async_Task *lex_task_ptr = scope_attachment(app, scope, buffer_lex_task, Async_Task);
        *lex_task_ptr = async_task_no_dep(&global_async_system, do_full_lex_async, make_data_struct(&buffer_id));
    }
    
    {
        b32 *wrap_lines_ptr = scope_attachment(app, scope, buffer_wrap_lines, b32);
        *wrap_lines_ptr = wrap_lines;
    }
    
    if (use_lexer){
        buffer_set_layout(app, buffer_id, layout_virt_indent_index_generic);
    }
    else{
        if (treat_as_code){
            buffer_set_layout(app, buffer_id, layout_virt_indent_literal_generic);
        }
        else{
            buffer_set_layout(app, buffer_id, layout_generic);
        }
    }
    
    // no meaning for return
    return(0);
}

BUFFER_EDIT_RANGE_SIG(ts_BufferEditRange){
    // buffer_id, new_range, original_size
    ProfileScope(app, "ts_BufferEditRange");
    
    Range_i64 old_range = Ii64(old_cursor_range.min.pos, old_cursor_range.max.pos);
    
    buffer_shift_fade_ranges(buffer_id, old_range.max, (new_range.max - old_range.max));
    
    {
        code_index_lock();
        Code_Index_File *file = code_index_get_file(buffer_id);
        if (file != 0){
            code_index_shift(file, old_range, range_size(new_range));
        }
        code_index_unlock();
    }
    i64 insert_size = range_size(new_range);
    i64 text_shift = replace_range_shift(old_range, insert_size);
    
    Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
    Scratch_Block scratch(app);
    
    //- NOTE(jack): Update the active query if the buffer was the queyrbuffer
    if(buffer_id == get_buffer_by_name(app, SCu8("*query*"), Access_Always))
    {
        if (active_query) {
            ts_query_delete(active_query);
        }
        String_Const_u8 query_src = push_whole_buffer(app, scratch, buffer_id);
        u32 error_offset;
        TSQueryError error;
        active_query = ts_query_new(tree_sitter_cpp(), (char*)query_src.str, 
                                    (u32)query_src.size,
                                    &error_offset, &error);
        if (!active_query) {
            print_message(app, SCu8("Failed to create query"));
        }
    }
    //~ My Edit range stuff
    {
        ProfileScope(app, "ts_BufferEditRange ts_tree edit and parser restart");
        JPTS_Data *tree_data = scope_attachment(app, scope, ts_data, JPTS_Data);
        // NOTE(jack): Edit the tree so the nodes that have moved remain aligned the buffer
        // Reparse to add new nodes later in the modified buffer tick
        if (tree_data->tree)
        {
            //- Cancel a running parse task if it exists
            Async_Task *ts_parse_task = scope_attachment(app, scope, ts_async_parse_task, Async_Task);
            if (async_task_is_running_or_pending(&global_async_system, *ts_parse_task)) {
                // NOTE(jack): We dont want to wait for ts_tree delete, as it is slow for large files
                // (noticable spikes when it fires in 4coder_base_types.cpp ~200Kb)
                // And if we restart the next task before it attempts the delete we may be able to
                // avoid the delete entirely as the reference count may have increased from the next
                // parse attempt
                async_task_cancel_nowait(app, &global_async_system, *ts_parse_task);
                *ts_parse_task = 0;
            }
            ProfileScope(app, "ts_BufferEditRange After async task cancel");
            
            print_message(app, SCu8("Performing tree-edit.\n"));
            i64 new_end_line = get_line_number_from_pos(app, buffer_id, new_range.end);
            i64 new_end_pos = new_range.end - get_line_start_pos(app, buffer_id, new_end_line);
            
            TSInputEdit edit;
            edit.start_byte = (u32)old_range.start;
            edit.old_end_byte = (u32)old_range.end;
            edit.new_end_byte = (u32)new_range.end;
            edit.start_point = {(u32)old_cursor_range.start.line, (u32)old_cursor_range.start.col};
            edit.old_end_point = {(u32)old_cursor_range.end.line, (u32)old_cursor_range.end.col};
            // TODO(jack): This seams to act correctly but looks very wrong, verify that it is correct.
            edit.new_end_point = {(u32)new_end_line - 1, (u32)new_end_pos + 1};
            
            ts_tree_edit(tree_data->tree, &edit);
            *ts_parse_task = async_task_no_dep(&global_async_system, ts_parse_async, make_data_struct(&buffer_id));
        }
    }
    
    //~ default things
    // NOTE(jack): Cancel the async lex task if its running.
    // We may get a correct lex to use if not, we'll start a new async lex.
    Async_Task *lex_task_ptr = scope_attachment(app, scope, buffer_lex_task, Async_Task);
    Base_Allocator *allocator = managed_scope_allocator(app, scope);
    b32 do_full_relex = false;
    if (async_task_is_running_or_pending(&global_async_system, *lex_task_ptr)){
        async_task_cancel(app, &global_async_system, *lex_task_ptr);
        buffer_unmark_as_modified(buffer_id);
        do_full_relex = true;
        *lex_task_ptr = 0;
    }
    Token_Array *ptr = scope_attachment(app, scope, attachment_tokens, Token_Array);
    if (ptr != 0 && ptr->tokens != 0){
        ProfileBlockNamed(app, "attempt resync", profile_attempt_resync);
        
        i64 token_index_first = token_relex_first(ptr, old_range.first, 1);
        i64 token_index_resync_guess =
            token_relex_resync(ptr, old_range.one_past_last, 16);
        
        if (token_index_resync_guess - token_index_first >= 4000){
            do_full_relex = true;
        }
        else{
            Token *token_first = ptr->tokens + token_index_first;
            Token *token_resync = ptr->tokens + token_index_resync_guess;
            
            Range_i64 relex_range = Ii64(token_first->pos, token_resync->pos + token_resync->size + text_shift);
            String_Const_u8 partial_text = push_buffer_range(app, scratch, buffer_id, relex_range);
            
            Token_List relex_list = lex_full_input_cpp(scratch, partial_text);
            if (relex_range.one_past_last < buffer_get_size(app, buffer_id)){
                token_drop_eof(&relex_list);
            }
            
            Token_Relex relex = token_relex(relex_list, relex_range.first - text_shift, ptr->tokens, token_index_first, token_index_resync_guess);
            
            ProfileCloseNow(profile_attempt_resync);
            
            if (!relex.successful_resync){
                do_full_relex = true;
            }
            else{
                ProfileBlock(app, "apply resync");
                
                i64 token_index_resync = relex.first_resync_index;
                
                Range_i64 head = Ii64(0, token_index_first);
                Range_i64 replaced = Ii64(token_index_first, token_index_resync);
                Range_i64 tail = Ii64(token_index_resync, ptr->count);
                i64 resynced_count = (token_index_resync_guess + 1) - token_index_resync;
                i64 relexed_count = relex_list.total_count - resynced_count;
                i64 tail_shift = relexed_count - (token_index_resync - token_index_first);
                
                i64 new_tokens_count = ptr->count + tail_shift;
                Token *new_tokens = base_array(allocator, Token, new_tokens_count);
                
                Token *old_tokens = ptr->tokens;
                block_copy_array_shift(new_tokens, old_tokens, head, 0);
                token_fill_memory_from_list(new_tokens + replaced.first, &relex_list, relexed_count);
                for (i64 i = 0, index = replaced.first; i < relexed_count; i += 1, index += 1){
                    new_tokens[index].pos += relex_range.first;
                }
                for (i64 i = tail.first; i < tail.one_past_last; i += 1){
                    old_tokens[i].pos += text_shift;
                }
                block_copy_array_shift(new_tokens, ptr->tokens, tail, tail_shift);
                
                base_free(allocator, ptr->tokens);
                
                ptr->tokens = new_tokens;
                ptr->count = new_tokens_count;
                ptr->max = new_tokens_count;
                
                buffer_mark_as_modified(buffer_id);
            }
        }
    }
    
    if (do_full_relex){
        *lex_task_ptr = async_task_no_dep(&global_async_system, do_full_lex_async,
                                          make_data_struct(&buffer_id));
    }
    
    // no meaning for return
    return(0);
}

BUFFER_HOOK_SIG(ts_EndBuffer){
    Marker_List *list = get_marker_list_for_buffer(buffer_id);
    if (list != 0){
        delete_marker_list(list);
    }
    
    Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
    Async_Task *ts_parse_task = scope_attachment(app, scope, ts_async_parse_task, Async_Task);
    if (async_task_is_running_or_pending(&global_async_system, *ts_parse_task)) {
        async_task_cancel(app, &global_async_system, *ts_parse_task);
    }
    
    JPTS_Data *tree_data = scope_attachment(app, scope, ts_data, JPTS_Data);
    
    ts_tree_delete(tree_data->tree);
    
    system_mutex_free(tree_data->tree_mutex);
    default_end_buffer(app, buffer_id);
    
    return(0);
}

String_Const_char TS_CPP_INDEX_QUERY = SCchar("(_ \"{\" @Start \"}\" @End ) @ScopeNest\n");
TSQuery *cpp_index_query;

// NOTE(jack): This is actually quite janky basically to extract nest structure from our 
// query we use a Doubly Linked list so we can iterate backwards
struct Ts_Code_Index_Nest_DList_Node {
    Ts_Code_Index_Nest_DList_Node *next;
    Ts_Code_Index_Nest_DList_Node *prev;
    
    Ts_Code_Index_Nest_DList_Node *parent;
    Ts_Code_Index_Nest_DList_Node *first_child;
    Ts_Code_Index_Nest_DList_Node *last_child;
    Code_Index_Nest *nest;
};

struct Ts_Code_Index_Nest_DList {
    Ts_Code_Index_Nest_DList_Node *first;
    Ts_Code_Index_Nest_DList_Node *last;
};


function void
ts_code_index_update_tick(Application_Links *app)
{
    Scratch_Block scratch(app);
    if (!cpp_index_query)
    {
        u32 error_offset;
        TSQueryError query_error;
        cpp_index_query = ts_query_new(tree_sitter_cpp(), TS_CPP_INDEX_QUERY.str, (u32)TS_CPP_INDEX_QUERY.size, &error_offset, &query_error);
        if (!cpp_index_query) {
            print_message(app, string_u8_litexpr("Failed to create Index Query\n"));
        }
    }
    
    // NOTE(jack): Iterate the modified buffers
    for (Buffer_Modified_Node *ModifiedNode = global_buffer_modified_set.first;
         ModifiedNode!= 0;
         ModifiedNode = ModifiedNode->next)
    {
        Temp_Memory_Block temp(scratch);
        Buffer_ID buffer_id = ModifiedNode->buffer;
        
        Arena arena = make_arena_system(KB(16));
        Code_Index_File *index = push_array_zero(&arena, Code_Index_File, 1);
        index->buffer = buffer_id;
        Ts_Code_Index_Nest_DList nests = {};
        
        Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
        JPTS_Data *tree_data = scope_attachment(app, scope, ts_data, JPTS_Data);
        TSTree *tree = jpts_buffer_get_tree(tree_data);
        if (tree) {
            TSQueryCursor *query_cursor = ts_query_cursor_new();
            ts_query_cursor_exec(query_cursor, cpp_index_query, ts_tree_root_node(tree));
            TSQueryMatch match;
            
            // NOTE(jack): Take all of the matches, calculate their nest ranges and push them onto the 
            // end of Dlist (list order matches iteration order)
            while (ts_query_cursor_next_match(query_cursor, &match))
            {
                // Query: (_ \"{\" @Start, \"}\" @End )@ScopeNest"
                // ScopeNest
                TSQueryCapture type_capture = match.captures[0];
                TSNode type_node = type_capture.node;
                Range_i64 type_range = jpts_node_to_range(type_node);
                
                // Start
                TSQueryCapture start_capture = match.captures[1];
                TSNode start_node = start_capture.node;
                Range_i64 start_range = jpts_node_to_range(start_node);
                
                // End
                TSQueryCapture end_capture = match.captures[2];
                TSNode end_node = end_capture.node;
                Range_i64 end_range = jpts_node_to_range(end_node);
                end_range.start = end_range.end - 1;
                Ts_Code_Index_Nest_DList_Node *node = push_array_zero(scratch, Ts_Code_Index_Nest_DList_Node, 1);
                node->nest = push_array_zero(&arena, Code_Index_Nest, 1);
                node->nest->kind = CodeIndexNest_Scope;
                node->nest->is_closed = true;
                node->nest->open = start_range;
                node->nest->close = end_range;
                node->nest->file = index;
                zdll_push_back(nests.first, nests.last, node);
                
                //String_Const_u8 message = push_stringf(scratch, "Detected nest between %d and %d\n", start_range.start, end_range.end);
                //print_message(app, message);
            }
            ts_query_cursor_delete(query_cursor);
        }
        ts_tree_delete(tree);
        
        // TODO(jack): Possibly need to do a sort here, as moving forward we assume ranges are stored in close order.
        for(Ts_Code_Index_Nest_DList_Node *node = nests.last;
            node != 0;
            node = node->prev)
        {
            Code_Index_Nest *nest = node->nest;
            for(Ts_Code_Index_Nest_DList_Node *candidate_node = node->next;
                candidate_node != 0;
                candidate_node = candidate_node->parent)
            {
                Code_Index_Nest *candidate_nest = candidate_node->nest;
                Range_i64 candidate_range = {candidate_nest->open.start, candidate_nest->close.end};
                
                if (range_contains(candidate_range, nest->open.start) &&
                    range_contains(candidate_range, nest->close.end)) 
                {
                    nest->parent = candidate_nest;
                    node->parent = candidate_node;
                    
                    // NOTE(jack): Because we are interating backwards, we need to fill candidate children backwards
                    // this is almost idendical to code_index_push_nest(candidate_nest->nest_list, nest);
                    {
                        sll_stack_push(candidate_nest->nest_list.first, nest);
                        candidate_nest->nest_list.count++;
                        // NOTE(jack): Because the sll_stack_push does not update the last element of the list
                        // we need to manually set the end of the list when we add the first nest.
                        if (candidate_nest->nest_list.count == 1) {
                            candidate_nest->nest_list.last = candidate_nest->nest_list.first;
                        }
                    }
                    // Also set the DList node's parent
                    if (!candidate_node->last_child) {
                        candidate_node->last_child = node;
                        candidate_node->first_child = node;
                    } else {
                        candidate_node->first_child = node;
                    }
                    // NOTE(jack): Once we have set a parent we stop iterating so we dont set
                    break;
                }
                else
                {
                    // NOTE(jack): When we skip a node whilst checking for a parent, that means we've filled its list
                    // so we can remove its children from the DList and flatten it.
                    if (candidate_nest->nest_array.count == 0 && candidate_nest->nest_list.count > 0) {
                        candidate_nest->nest_array = code_index_nest_ptr_array_from_list(&arena, &candidate_nest->nest_list);
                        dll_remove_multiple(candidate_node->first_child, candidate_node->last_child);
                    }
                }
            }
        }
        for(Ts_Code_Index_Nest_DList_Node *node = nests.first;
            node != 0;
            node = node->next)
        {
            code_index_push_nest(&index->nest_list, node->nest);
        }
        index->nest_array = code_index_nest_ptr_array_from_list(&arena, &index->nest_list);
        
        code_index_lock();
        code_index_set_file(buffer_id, arena, index);
        code_index_unlock();
        buffer_clear_layout_cache(app, buffer_id);
        // NOTE(jack): We only want to unmark as modified if the index has been updated
        // This may not get hit if tree-sitter hasnt finished parsing the file yet.
        buffer_unmark_as_modified(buffer_id);
    }
    
    //buffer_modified_set_clear();
}


function void
ts_Tick(Application_Links *app, Frame_Info frame_info){
    ////////////////////////////////
    // NOTE(allen): Update code index
    {
        ProfileScope(app, "Code Index Update Tick");
        if (use_ts_code_indexing) {
            ts_code_index_update_tick(app);
        } else {
            code_index_update_tick(app);
        }
    }
    ////////////////////////////////
    // NOTE(allen): Update fade ranges
    
    if (tick_all_fade_ranges(app, frame_info.animation_dt)){
        animate_in_n_milliseconds(app, 0);
    }
    
    ////////////////////////////////
    // NOTE(allen): Clear layouts if virtual whitespace setting changed.
    
    {
        b32 enable_virtual_whitespace = def_get_config_b32(vars_save_string_lit("enable_virtual_whitespace"));
        if (enable_virtual_whitespace != def_enable_virtual_whitespace){
            def_enable_virtual_whitespace = enable_virtual_whitespace;
            clear_all_layouts(app);
        }
    }
}

// BOTTOM

