#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "history.h"

/* In-memory history storage */
static char *history[HISTORY_MAX_SIZE];
static int history_start = 0;    /* Index of oldest entry */
static int history_end = 0;      /* Index where next entry will go */
static int history_total = 0;    /* Total commands ever added (for numbering) */
static int history_size = 0;     /* Current number of entries */

/* Initialize the history system */
void history_init(void)
{
    for (int i = 0; i < HISTORY_MAX_SIZE; i++)
    {
        history[i] = NULL;
    }
    history_start = 0;
    history_end = 0;
    history_total = 0;
    history_size = 0;
}

/* Add a command to history */
void history_add(const char *cmd)
{
    if (!cmd || cmd[0] == '\0')
    {
        return;
    }
    
    /* Skip if command is only whitespace or newline */
    const char *p = cmd;
    while (*p && (isspace((unsigned char)*p)))
    {
        p++;
    }
    if (*p == '\0')
    {
        return;
    }
    
    /* Don't add duplicate of last command */
    if (history_size > 0)
    {
        int last_idx = (history_end - 1 + HISTORY_MAX_SIZE) % HISTORY_MAX_SIZE;
        if (history[last_idx] && strcmp(history[last_idx], cmd) == 0)
        {
            return;
        }
    }
    
    /* Free old entry if circular buffer is full */
    if (history[history_end])
    {
        free(history[history_end]);
    }
    
    /* Store new command (strip trailing newline) */
    size_t len = strlen(cmd);
    if (len > 0 && cmd[len - 1] == '\n')
    {
        len--;
    }
    
    history[history_end] = malloc(len + 1);
    if (history[history_end])
    {
        strncpy(history[history_end], cmd, len);
        history[history_end][len] = '\0';
    }
    
    /* Update circular buffer indices */
    history_end = (history_end + 1) % HISTORY_MAX_SIZE;
    history_total++;
    
    if (history_size < HISTORY_MAX_SIZE)
    {
        history_size++;
    }
    else
    {
        /* Buffer is full, move start forward */
        history_start = (history_start + 1) % HISTORY_MAX_SIZE;
    }
}

/* Get command at index (1-based) */
char *history_get(int index)
{
    if (index < 1)
    {
        return NULL;
    }
    
    /* Calculate the first valid index number */
    int first_num = history_total - history_size + 1;
    
    if (index < first_num || index > history_total)
    {
        return NULL;
    }
    
    /* Convert to array index */
    int offset = index - first_num;
    int array_idx = (history_start + offset) % HISTORY_MAX_SIZE;
    
    return history[array_idx];
}

/* Get the last command */
char *history_get_last(void)
{
    if (history_size == 0)
    {
        return NULL;
    }
    
    int last_idx = (history_end - 1 + HISTORY_MAX_SIZE) % HISTORY_MAX_SIZE;
    return history[last_idx];
}

/* Get total number of commands in history */
int history_count(void)
{
    return history_size;
}

/* Clear all history */
void history_clear(void)
{
    for (int i = 0; i < HISTORY_MAX_SIZE; i++)
    {
        if (history[i])
        {
            free(history[i]);
            history[i] = NULL;
        }
    }
    history_start = 0;
    history_end = 0;
    history_total = 0;
    history_size = 0;
}

/* Print all history (builtin command) */
int history_builtin(int argc, char **argv)
{
    int count = history_size;
    
    /* Check for -c (clear) option */
    if (argc > 1 && strcmp(argv[1], "-c") == 0)
    {
        history_clear();
        return 0;
    }
    
    /* Check for count argument */
    if (argc > 1)
    {
        int n = atoi(argv[1]);
        if (n > 0)
        {
            if (n < count)
            {
                count = n;
            }
        }
        else if (n == 0 && argv[1][0] == '0')
        {
            /* Explicit 0 - show nothing */
            count = 0;
        }
        /* Negative or invalid: show all (count unchanged) */
    }
    
    /* Print history entries */
    int first_num = history_total - history_size + 1;
    int start_offset = history_size - count;
    
    for (int i = start_offset; i < history_size; i++)
    {
        int array_idx = (history_start + i) % HISTORY_MAX_SIZE;
        int cmd_num = first_num + i;
        
        if (history[array_idx])
        {
            printf("%5d  %s\n", cmd_num, history[array_idx]);
        }
    }
    
    return 0;
}

/* Expand history references like !! and !n in command string */
/* Returns: expanded string, or NULL if no expansion needed, or empty string "" on error */
char *history_expand(const char *cmd)
{
    if (!cmd || !strchr(cmd, '!'))
    {
        return NULL;  /* No expansion needed */
    }
    
    /* Allocate result buffer */
    size_t result_size = HISTORY_MAX_CMD_LEN;
    char *result = malloc(result_size);
    if (!result)
    {
        return NULL;
    }
    
    size_t result_len = 0;
    const char *p = cmd;
    int expanded = 0;
    int error = 0;
    char error_token[64] = "";
    
    while (*p && !error)
    {
        if (*p == '!' && *(p + 1))
        {
            char *expansion = NULL;
            int skip = 0;
            int is_history_ref = 0;
            
            if (*(p + 1) == '!')
            {
                /* !! - last command */
                is_history_ref = 1;
                expansion = history_get_last();
                skip = 2;
                if (!expansion)
                {
                    strcpy(error_token, "!!");
                }
            }
            else if (*(p + 1) == '-' && isdigit((unsigned char)*(p + 2)))
            {
                /* !-n - nth previous command */
                is_history_ref = 1;
                int n = atoi(p + 2);
                int target = history_total - n;
                expansion = history_get(target);
                skip = 2;
                while (isdigit((unsigned char)*(p + skip)))
                {
                    skip++;
                }
                if (!expansion)
                {
                    snprintf(error_token, sizeof(error_token), "!-%d", n);
                }
            }
            else if (isdigit((unsigned char)*(p + 1)))
            {
                /* !n - command number n */
                is_history_ref = 1;
                int n = atoi(p + 1);
                expansion = history_get(n);
                skip = 1;
                while (isdigit((unsigned char)*(p + skip)))
                {
                    skip++;
                }
                if (!expansion)
                {
                    snprintf(error_token, sizeof(error_token), "!%d", n);
                }
            }
            
            if (is_history_ref)
            {
                if (expansion)
                {
                    size_t exp_len = strlen(expansion);
                    if (result_len + exp_len >= result_size - 1)
                    {
                        /* Need more space */
                        result_size = result_len + exp_len + HISTORY_MAX_CMD_LEN;
                        char *new_result = realloc(result, result_size);
                        if (!new_result)
                        {
                            free(result);
                            return NULL;
                        }
                        result = new_result;
                    }
                    strcpy(result + result_len, expansion);
                    result_len += exp_len;
                    p += skip;
                    expanded = 1;
                    continue;
                }
                else
                {
                    /* History reference failed - event not found */
                    error = 1;
                    break;
                }
            }
        }
        
        /* Copy regular character */
        if (result_len < result_size - 1)
        {
            result[result_len++] = *p;
        }
        p++;
    }
    
    result[result_len] = '\0';
    
    if (error)
    {
        /* Print error and return empty string to prevent execution */
        fprintf(stderr, "%s: event not found\n", error_token);
        result[0] = '\0';
        return result;  /* Return empty string, not NULL */
    }
    
    if (!expanded)
    {
        free(result);
        return NULL;
    }
    
    /* Print expanded command */
    printf("%s", result);
    
    return result;
}
