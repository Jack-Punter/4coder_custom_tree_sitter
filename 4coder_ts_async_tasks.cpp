function void
async_task_cancel_nowait(Application_Links *app, Async_System *async_system, Async_Task task){
    system_mutex_acquire(async_system->mutex);
    Async_Node *node = async_get_pending_node(async_system, task);
    if (node != 0){
        dll_remove(&node->node);
        async_system->task_count -= 1;
        async_free_node(async_system, node);
    }
    else{
        node = async_get_running_node(async_system, task);
        if (node != 0){
            b32 *cancel_signal = &node->thread->cancel_signal;
            atomic_write_b32(cancel_signal, true);
            // async_task_wait__inner(app, async_system, task);
        }
    }
    system_mutex_release(async_system->mutex);
}

#if 0
function const char *
ts_4coder_read(void *payload, uint32_t byte_index, TSPoint position, uint32_t *bytes_read)
{
    Buffer_ID Buffer = *(Buffer_ID *)payload;
}
#endif

function void
ts_parse_async__inner(Async_Context *actx, Buffer_ID buffer)
{
    Application_Links *app = actx->app;
    ProfileBlock(app, "ts_parse_async");
    
    Arena arena = make_arena_system(KB(16));
    TSParser *parser = ts_parser_new();
    // NOTE(jack): Set the parers timeout to 5ms. This will allow us to
    // periodically check if the task has been cancled.
    // TODO(jack): Should the be a different number, 5ms was arbitrary.
    ts_parser_set_timeout_micros(parser, 5000);
    
    acquire_global_frame_mutex(app);
    String_Const_u8 src = push_whole_buffer(app, &arena, buffer);
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    
    // TODO(jack): Apparently its possible for scope_attachment to return a null pointer :|
    // So I need to guard against this, although im not sure what I can do about it.
    JPTS_Data *tree_data = scope_attachment(app, scope, ts_data, JPTS_Data);
    
    // NOTE(jack): Make a (shallow) copy of the tree incrementing ref count.
    TSTree *old_tree = jpts_buffer_get_tree(tree_data);
    String_Const_u8 file_name = push_buffer_file_name(app, &arena, buffer);
    Language_Definition *lang = jpts_language_from_buffer(app, buffer);
    bool lang_set = ts_parser_set_language(parser, lang->ts_language);
    release_global_frame_mutex(app);
    
    if (!lang_set) {
        AssertMessageAlways("This should never happen as we've already set up a "
                            "parser for this language in the buffer begin hook.");
    }
    
    TSTree *new_tree = 0;
    b32 canceled = false;
    {
        ProfileScope(app, "Parse Loop"); 
        for (;;) {
            {
                ProfileScope(app, "ParseLoop ts_parser_parse_string");
                new_tree = ts_parser_parse_string(parser, old_tree, (char *)src.str, (u32)src.size);
            }
            // new_tree = ts_parser_parse(parser, old_tree, (char *)src.str, (u32)src.size);
            if (async_check_canceled(actx)) {
                canceled = true;
                break;
            }
            if (new_tree) {
                break;
            }
        }
    }
    if (!canceled && new_tree) {
        acquire_global_frame_mutex(app);
        
        // NOTE(jack): Copy the original reference to the tree to delete it outside the mutex.
        // if the delete actually free's allocated memory it can be very slow.
        // NOTE: we dont call ts_tree_copy here because we actually want to delete the original
        // reference to the tree
        
        system_mutex_acquire(tree_data->tree_mutex);
        TSTree *old_buffer_tree = tree_data->tree;
        tree_data->tree = new_tree;
        system_mutex_release(tree_data->tree_mutex);
        
        // NOTE(jack): This feels kinda hacky, this is here to trigger
        // the code index update tick. The buffer is also makred  by the
        // async lexer so we will update the index too frequently. We
        // should probably change the lexer to not mark as modified.
        // TODO(jack): Should we instead trigger another async task here to
        // update the code index once this is done?
        buffer_mark_as_modified(buffer);
        
        // NOTE(jack): If 4coder is not updatating it saves power by redrawing every
        // second, however, it doesn't animate when doing this, so we need to force
        // 4coder to animate immediately to highlight
        animate_in_n_milliseconds(app, 0);
        release_global_frame_mutex(app);
        
        // NOTE(jack): This is done outside the mutex, because if it 
        ts_tree_delete(old_buffer_tree);
    }
    
    ts_parser_delete(parser);
    ts_tree_delete(old_tree);
    linalloc_clear(&arena);
}

function void
ts_parse_async(Async_Context *actx, String_Const_u8 data)
{
    if (data.size == sizeof(Buffer_ID))
    {
        Buffer_ID buffer = *(Buffer_ID *)data.str;
        ts_parse_async__inner(actx, buffer);
    }
}
