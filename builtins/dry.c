#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../mshX.h"
#include "../source.h"
#include "../parser.h"
#include "../executor.h"

/*
 * dry builtin command - execute commands in dry-run mode
 * 
 * This command parses, expands, and builds the AST identically to real
 * execution, but prints what would happen instead of actually executing.
 * 
 * No side effects in dry mode:
 *   - No fork
 *   - No exec*
 *   - No pipe
 *   - No dup*
 *   - No open
 *   - No file creation or modification
 * 
 * Usage:
 *   dry <command> [args...]
 *   dry ls -l
 *   dry echo *.c
 *   dry ls | grep txt
 *   dry cat file.txt > out.txt
 */
int dry(int argc, char **argv)
{
    if(argc < 2)
    {
        fprintf(stderr, "dry: usage: dry <command> [args...]\n");
        return 1;
    }
    
    /* Build the command string from arguments */
    size_t total_len = 0;
    for(int i = 1; i < argc; i++)
    {
        total_len += strlen(argv[i]) + 1; /* +1 for space or null */
    }
    
    char *cmd_line = malloc(total_len + 2); /* +2 for newline and null */
    if(!cmd_line)
    {
        fprintf(stderr, "dry: memory allocation error\n");
        return 1;
    }
    
    cmd_line[0] = '\0';
    for(int i = 1; i < argc; i++)
    {
        strcat(cmd_line, argv[i]);
        if(i < argc - 1)
        {
            strcat(cmd_line, " ");
        }
    }
    strcat(cmd_line, "\n"); /* Add newline like normal input */
    
    /* Save the current execution mode and switch to dry-run mode */
    enum exec_mode saved_mode = current_exec_mode;
    current_exec_mode = EXEC_DRY;
    
    /* Parse and execute in dry-run mode using the same parser */
    struct source_s src;
    src.buffer = cmd_line;
    src.buffer_size = strlen(cmd_line);
    src.current_pos = INIT_SRC_POS;
    
    parse_and_execute(&src);
    
    /* Restore the execution mode */
    current_exec_mode = saved_mode;
    
    free(cmd_line);
    return 0;
}
