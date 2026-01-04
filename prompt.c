#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include "shell.h"
#include "symtab/symtab.h"


void print_prompt1(void)
{
    char cwd[1024];
    char *home = getenv("HOME");
    char *display_path = cwd;
    
    /* Get current working directory */
    if(getcwd(cwd, sizeof(cwd)) != NULL)
    {
        /* Replace HOME with ~ for shorter display */
        if(home && strncmp(cwd, home, strlen(home)) == 0)
        {
            /* Check if it's exactly HOME or a subdirectory of HOME */
            if(cwd[strlen(home)] == '\0' || cwd[strlen(home)] == '/')
            {
                static char short_path[1024];
                snprintf(short_path, sizeof(short_path), "~_~%s", cwd + strlen(home));
                display_path = short_path;
            }
        }
        fprintf(stderr, "%s:$", display_path);
    }
    else
    {
        /* Fallback to PS1 or default */
        struct symtab_entry_s *entry = get_symtab_entry("PS1");
        if(entry && entry->val)
        {
            fprintf(stderr, "%s", entry->val);
        }
        else
        {
            fprintf(stderr, "$ ");
        }
    }
}


void print_prompt2(void)
{
    struct symtab_entry_s *entry = get_symtab_entry("PS2");

    if(entry && entry->val)
    {
        fprintf(stderr, "%s", entry->val);
    }
    else
    {
        fprintf(stderr, "> ");
    }
}