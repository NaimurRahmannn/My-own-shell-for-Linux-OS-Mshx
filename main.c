#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "mshX.h"
#include "source.h"
#include "parser.h"
#include "executor.h"
#include "builtins/timeline.h"
#include "builtins/history.h"

/* extern declaration for exit status */
extern int exit_status;

/* Check if command starts with timeline and strip it */
static char *check_timeline_flag(char *cmd, int *timeline_requested)
{
    *timeline_requested = 0;
    
    /* Skip leading whitespace */
    char *p = cmd;
    while(*p == ' ' || *p == '\t')
        p++;
    
    /* Check for timeline flag */
    if(strncmp(p, "timeline", 8) == 0 && 
       (p[8] == ' ' || p[8] == '\t'))
    {
        *timeline_requested = 1;
        p += 8;
        
        /* Skip whitespace after timeline */
        while(*p == ' ' || *p == '\t')
            p++;
        
        /* Create new command string without timeline */
        char *new_cmd = malloc(strlen(p) + 1);
        if(new_cmd)
        {
            strcpy(new_cmd, p);
            free(cmd);
            return new_cmd;
        }
    }
    
    return cmd;
}


int main(int argc, char **argv)
{
    char *cmd;

    initsh();
    
    do
    {
        print_prompt1();
        cmd = read_cmd();
        if(!cmd)
        {
            /* Check if this was EOF or an error */
            if(feof(stdin))
            {
                printf("\n");  /* Print newline before exit on Ctrl+D */
                exit(EXIT_SUCCESS);
            }
            /* Clear any error state and continue (e.g., after signal) */
            clearerr(stdin);
            continue;
        }
        if(cmd[0] == '\0' || strcmp(cmd, "\n") == 0)
        {
            free(cmd);
            continue;
        }
        if(strcmp(cmd, "exit\n") == 0)
        {
            free(cmd);
            break;
        }
        
        /* Expand history references (!! and !n) */
        char *expanded = history_expand(cmd);
        if(expanded)
        {
            free(cmd);
            cmd = expanded;
            /* If expansion resulted in empty string, skip execution */
            if(cmd[0] == '\0')
            {
                free(cmd);
                continue;
            }
        }
        
        /* Add command to history (before processing) */
        history_add(cmd);
        
        /* Check for timeline flag */
        int timeline_requested = 0;
        cmd = check_timeline_flag(cmd, &timeline_requested);
        
        /* Enable timeline if requested */
        if(timeline_requested)
        {
            timeline_enable(1);
        }
        
	struct source_s src;
        src.buffer   = cmd;
        src.buffer_size  = strlen(cmd);
        src.current_pos   = INIT_SRC_POS;
        parse_and_execute(&src);
        
        /* Disable timeline after execution */
        if(timeline_requested)
        {
            timeline_enable(0);
        }
        
        free(cmd);
    } while(1);
    exit(EXIT_SUCCESS);
}


char *read_cmd(void)
{
    char buf[1024];
    char *ptr = NULL;
    char ptrlen = 0;
    while(fgets(buf, 1024, stdin))
    {
        int buflen = strlen(buf);
        if(!ptr)
        {
            ptr = malloc(buflen+1);
        }
        else
        {
            char *ptr2 = realloc(ptr, ptrlen+buflen+1);
            if(ptr2)
            {
                ptr = ptr2;
            }
            else
            {
                free(ptr);
                ptr = NULL;
            }
        }
        if(!ptr)
        {
            fprintf(stderr, "error: failed to alloc buffer: %s\n", strerror(errno));
            return NULL;
        }
        strcpy(ptr+ptrlen, buf);
        if(buf[buflen-1] == '\n')
        {
            if(buflen == 1 || buf[buflen-2] != '\\')
            {
                return ptr;
            }
            ptr[ptrlen+buflen-2] = '\0';
            buflen -= 2;
            print_prompt2();
        }
        ptrlen += buflen;
    }
    return ptr;
}


int parse_and_execute(struct source_s *src)
{
    skip_white_spaces(src);

    /* For dry-run mode: check if we have multiple commands (sequence) */
    int has_sequence = 0;
    
    /* First pass for dry-run: check if there's a sequence operator */
    if(current_exec_mode == EXEC_DRY)
    {
        /* Save position BEFORE tokenizing */
        long saved_pos = src->current_pos;
        
        /* Count commands and check for sequence */
        struct token_s *check_tok = tokenize(src);
        while(check_tok && check_tok != &eof_token)
        {
            if(strcmp(check_tok->text, ";") == 0)
            {
                has_sequence = 1;
            }
            free_token(check_tok);
            skip_white_spaces(src);
            check_tok = tokenize(src);
        }
        
        /* Reset position for actual parsing */
        src->current_pos = saved_pos;
        
        if(has_sequence)
        {
            dry_print_sequence();
        }
    }

    struct token_s *tok = tokenize(src);

    if(tok == &eof_token)
    {
        return 0;
    }

    while(tok && tok != &eof_token)
    {
        /* Collect commands for pipeline */
        struct node_s *pipeline[64];  /* max 64 commands in a pipeline */
        int num_cmds = 0;
        
        /* Parse first command */
        struct node_s *cmd = parse_simple_command(tok);
        if(cmd && cmd->first_child)
        {
            pipeline[num_cmds++] = cmd;
        }
        else if(cmd)
        {
            free_node_tree(cmd);
        }
        
        /* Check for pipes and collect all commands in pipeline */
        skip_white_spaces(src);
        tok = tokenize(src);
        
        while(tok && tok != &eof_token && strcmp(tok->text, "|") == 0)
        {
            free_token(tok);
            skip_white_spaces(src);
            tok = tokenize(src);
            
            if(tok == &eof_token)
            {
                fprintf(stderr, "error: expected command after pipe\n");
                break;
            }
            
            cmd = parse_simple_command(tok);
            if(cmd && cmd->first_child && num_cmds < 64)
            {
                pipeline[num_cmds++] = cmd;
            }
            else if(cmd)
            {
                free_node_tree(cmd);
            }
            
            skip_white_spaces(src);
            tok = tokenize(src);
        }
        
        /* Check if this is a background job */
        int run_in_background = 0;
        if(tok && tok != &eof_token && strcmp(tok->text, "&") == 0)
        {
            run_in_background = 1;
        }
        
        /* Execute the pipeline */
        if(num_cmds > 0)
        {
            if(run_in_background)
            {
                do_pipeline_background(pipeline, num_cmds);
            }
            else
            {
                do_pipeline(pipeline, num_cmds);
            }
            
            /* Free all command nodes */
            for(int i = 0; i < num_cmds; i++)
            {
                free_node_tree(pipeline[i]);
            }
        }
        
        if(tok == &eof_token)
        {
            break;
        }
        
        /* Handle logical operators */
        if(strcmp(tok->text, "&&") == 0)
        {
            free_token(tok);
            /* AND operator: execute next command only if previous succeeded (exit_status == 0) */
            if(exit_status != 0)
            {
                /* Skip remaining commands in this AND chain until we hit || or end */
                skip_white_spaces(src);
                tok = tokenize(src);
                while(tok && tok != &eof_token)
                {
                    if(strcmp(tok->text, "||") == 0)
                    {
                        free_token(tok);
                        break;
                    }
                    if(tok->text[0] == '\n')
                    {
                        free_token(tok);
                        return 1;
                    }
                    free_token(tok);
                    tok = tokenize(src);
                }
                if(tok == &eof_token)
                {
                    return 1;
                }
            }
            skip_white_spaces(src);
            tok = tokenize(src);
            continue;
        }
        else if(strcmp(tok->text, "||") == 0)
        {
            free_token(tok);
            /* OR operator: execute next command only if previous failed (exit_status != 0) */
            if(exit_status == 0)
            {
                /* Skip remaining commands in this OR chain until we hit && or end */
                skip_white_spaces(src);
                tok = tokenize(src);
                while(tok && tok != &eof_token)
                {
                    if(strcmp(tok->text, "&&") == 0)
                    {
                        free_token(tok);
                        break;
                    }
                    if(tok->text[0] == '\n')
                    {
                        free_token(tok);
                        return 1;
                    }
                    free_token(tok);
                    tok = tokenize(src);
                }
                if(tok == &eof_token)
                {
                    return 1;
                }
            }
            skip_white_spaces(src);
            tok = tokenize(src);
            continue;
        }
        else if(strcmp(tok->text, ";") == 0)
        {
            /* Command separator: just continue to next command unconditionally */
            free_token(tok);
            skip_white_spaces(src);
            tok = tokenize(src);
            continue;
        }
        else if(strcmp(tok->text, "&") == 0)
        {
            /* Background operator: command already executed, just continue */
            free_token(tok);
            skip_white_spaces(src);
            tok = tokenize(src);
            continue;
        }
    }
    return 1;
}