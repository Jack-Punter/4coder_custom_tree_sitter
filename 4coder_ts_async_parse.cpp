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


function void
ts_parse_async__inner(Async_Context *actx, Buffer_ID buffer)
{
    Application_Links *app = actx->app;
    ProfileBlock(app, "tree_sitter async parse");
    
    Arena arena = make_arena_system(KB(16));
    TSParser *parser = ts_parser_new();
    // NOTE(jack): Set the parers timeout to 5ms. This will allow us to
    // periodically check if the task has been cancled.
    // TODO(jack): Should the be a different number, 5ms was arbitrary.
    ts_parser_set_timeout_micros(parser, 5000);
    
    
    acquire_global_frame_mutex(app);
    String_Const_u8 src = push_whole_buffer(app, &arena, buffer);
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    Buffer_TS_Data *tree_data = scope_attachment(app, scope, ts_data, Buffer_TS_Data);
    // NOTE(jack): Make a (shallow) copy of the tree incrementing ref count.
    TSTree *old_tree = ts_buffer_get_tree(tree_data);
    bool lang_set = ts_parser_set_language(parser, tree_data->language);
    release_global_frame_mutex(app);
    
    if (!lang_set) {
        AssertMessageAlways("This should never happen as we've already set up a "
                            "parser for this language in the buffer begin hook.");
    }
    
    TSTree *new_tree = 0;
    b32 canceled = false; 
    for (;;) {
        new_tree = ts_parser_parse_string(parser, old_tree, (char *)src.str, (u32)src.size);
        if (async_check_canceled(actx)) {
            canceled = true;
            break;
        }
        if (new_tree) {
            break;
        }
    }
    
    if (!canceled && new_tree) {
        acquire_global_frame_mutex(app);
        
        // NOTE(jack): Copy the old pointer to delete it outside the mutex.
        ts_buffer_tree_lock(tree_data);
        TSTree *old_buffer_tree = tree_data->tree;
        tree_data->tree = new_tree;
        ts_buffer_tree_unlock(tree_data);
        
        // NOTE(jack): This feels kinda hacky, this is here to trigger
        // the code index update tick. The buffer is also makred  by the
        // async lexer so we will update the index too frequently. We
        // should probably change the lexer to not mark as modified.
        // TODO(jack): Should we instead trigger another async task here to
        // update the code index once this is done?
        buffer_mark_as_modified(buffer);
        
        // NOTE(jack): If 4coder is not updatating it saves power by redrawing every
        // second, however, it doesn't necissariy have to call custom layer to update
        // so we need to animate on the next frame to cause a redraw of the buffer
        // after the parse is complete.
        animate_in_n_milliseconds(app, 0);
        release_global_frame_mutex(app);
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
