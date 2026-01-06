#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "mshX.h"
#include "source.h"
#include "parser.h"
#include "executor.h"

/* extern declaration for exit status */
extern int exit_status;


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
            exit(EXIT_SUCCESS);
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
	struct source_s src;
        src.buffer   = cmd;
        src.buffer_size  = strlen(cmd);
        src.current_pos   = INIT_SRC_POS;
        parse_and_execute(&src);
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

    struct token_s *tok = tokenize(src);

    if(tok == &eof_token)
    {
        return 0;
    }

    while(tok && tok != &eof_token)
    {
        struct node_s *cmd = parse_simple_command(tok);

        if(!cmd)
        {
            break;
        }

        /* Check if command has any children (actual command to run) */
        if(cmd->first_child)
        {
            do_simple_command(cmd);
        }
        free_node_tree(cmd);
        
        /* Skip whitespace and check for logical operators */
        skip_white_spaces(src);
        tok = tokenize(src);
        
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
    }
    return 1;
}