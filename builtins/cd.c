#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "../mshX.h"
#include "../symtab/symtab.h"

/*
 * cd builtin command - change the current working directory
 * 
 * Usage:
 *   cd          - change to HOME directory
 *   cd path     - change to specified path
 *   cd -        - change to previous directory (OLDPWD)
 */
int cd(int argc, char **argv)
{
    char *target_dir = NULL;
    char *oldpwd = NULL;
    char cwd[1024];
    
    /* Save current directory for OLDPWD */
    if(getcwd(cwd, sizeof(cwd)) == NULL)
    {
        fprintf(stderr, "cd: error getting current directory: %s\n", strerror(errno));
        return 1;
    }
    
    if(argc == 1)
    {
        /* No argument - go to HOME */
        target_dir = getenv("HOME");
        if(!target_dir || !*target_dir)
        {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    }
    else if(strcmp(argv[1], "-") == 0)
    {
        /* cd - : go to previous directory */
        struct symtab_entry_s *entry = get_symtab_entry("OLDPWD");
        if(entry && entry->val && entry->val[0])
        {
            target_dir = entry->val;
            /* Print the directory we're changing to */
            printf("%s\n", target_dir);
        }
        else
        {
            oldpwd = getenv("OLDPWD");
            if(oldpwd && *oldpwd)
            {
                target_dir = oldpwd;
                printf("%s\n", target_dir);
            }
            else
            {
                fprintf(stderr, "cd: OLDPWD not set\n");
                return 1;
            }
        }
    }
    else
    {
        /* cd path */
        target_dir = argv[1];
    }
    
    /* Change directory */
    if(chdir(target_dir) != 0)
    {
        fprintf(stderr, "cd: %s: %s\n", target_dir, strerror(errno));
        return 1;
    }
    
    /* Update OLDPWD */
    struct symtab_entry_s *entry = get_symtab_entry("OLDPWD");
    if(!entry)
    {
        entry = add_to_symtab("OLDPWD");
    }
    if(entry)
    {
        symtab_entry_setval(entry, cwd);
    }
    
    /* Update PWD */
    if(getcwd(cwd, sizeof(cwd)) != NULL)
    {
        entry = get_symtab_entry("PWD");
        if(!entry)
        {
            entry = add_to_symtab("PWD");
        }
        if(entry)
        {
            symtab_entry_setval(entry, cwd);
        }
    }
    
    return 0;
}
