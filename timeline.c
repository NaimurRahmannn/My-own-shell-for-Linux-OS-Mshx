#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "timeline.h"

/* Global timeline instance */
timeline_t g_timeline = {
    .event_count = 0,
    .enabled = 1  /* Timeline enabled by default */
};

/* Get current time in milliseconds relative to command start */
static double get_elapsed_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    double start_ms = (double)g_timeline.start_time.tv_sec * 1000.0 +
                      (double)g_timeline.start_time.tv_nsec / 1000000.0;
    double now_ms = (double)now.tv_sec * 1000.0 +
                    (double)now.tv_nsec / 1000000.0;
    
    return now_ms - start_ms;
}

/* Initialize timeline for a new command */
void timeline_init(void)
{
    g_timeline.event_count = 0;
    clock_gettime(CLOCK_MONOTONIC, &g_timeline.start_time);
}

/* Reset/clear the timeline */
void timeline_reset(void)
{
    /* Free any allocated redirect targets */
    for (int i = 0; i < g_timeline.event_count; i++)
    {
        if (g_timeline.events[i].type == EVENT_REDIRECTED &&
            g_timeline.events[i].data.redirect_info.target)
        {
            free(g_timeline.events[i].data.redirect_info.target);
            g_timeline.events[i].data.redirect_info.target = NULL;
        }
    }
    g_timeline.event_count = 0;
}

/* Enable/disable timeline tracking */
void timeline_enable(int enable)
{
    g_timeline.enabled = enable;
}

/* Check if timeline is enabled */
int timeline_is_enabled(void)
{
    return g_timeline.enabled;
}

/* Helper to add a basic event */
static timeline_event_t *add_event(pid_t pid, timeline_event_type_t type)
{
    if (!g_timeline.enabled)
    {
        return NULL;
    }
    
    if (g_timeline.event_count >= MAX_TIMELINE_EVENTS)
    {
        return NULL;
    }
    
    timeline_event_t *event = &g_timeline.events[g_timeline.event_count++];
    event->pid = pid;
    event->type = type;
    event->time_ms = get_elapsed_ms();
    
    /* Zero out the union */
    memset(&event->data, 0, sizeof(event->data));
    
    return event;
}

/* Record fork event */
void timeline_record_fork(pid_t pid)
{
    add_event(pid, EVENT_FORKED);
}

/* Record execve event */
void timeline_record_execve(pid_t pid)
{
    add_event(pid, EVENT_EXECVE);
}

/* Record exit event */
void timeline_record_exit(pid_t pid, int exit_code)
{
    timeline_event_t *event = add_event(pid, EVENT_EXITED);
    if (event)
    {
        event->data.exit_info.exit_code = exit_code;
    }
}

/* Record signaled event */
void timeline_record_signaled(pid_t pid, int signal)
{
    timeline_event_t *event = add_event(pid, EVENT_SIGNALED);
    if (event)
    {
        event->data.signal_info.signal = signal;
    }
}

/* Record stopped event */
void timeline_record_stopped(pid_t pid, int signal)
{
    timeline_event_t *event = add_event(pid, EVENT_STOPPED);
    if (event)
    {
        event->data.signal_info.signal = signal;
    }
}

/* Record continued event */
void timeline_record_continued(pid_t pid)
{
    add_event(pid, EVENT_CONTINUED);
}

/* Record pipe event */
void timeline_record_pipe(pid_t from_pid, pid_t to_pid)
{
    timeline_event_t *event = add_event(to_pid, EVENT_PIPED);
    if (event)
    {
        event->data.pipe_info.from_pid = from_pid;
        event->data.pipe_info.to_pid = to_pid;
    }
}

/* Record redirect event */
void timeline_record_redirect(pid_t pid, const char *target, int direction)
{
    timeline_event_t *event = add_event(pid, EVENT_REDIRECTED);
    if (event)
    {
        event->data.redirect_info.target = target ? strdup(target) : NULL;
        event->data.redirect_info.direction = direction;
    }
}

/* Get event type as string */
const char *timeline_event_type_str(timeline_event_type_t type)
{
    switch (type)
    {
        case EVENT_FORKED:    return "forked";
        case EVENT_EXECVE:    return "execve";
        case EVENT_EXITED:    return "exited";
        case EVENT_SIGNALED:  return "signaled";
        case EVENT_STOPPED:   return "stopped";
        case EVENT_CONTINUED: return "continued";
        case EVENT_PIPED:     return "piped";
        case EVENT_REDIRECTED: return "redirected";
        default:              return "unknown";
    }
}

/* Compare function for qsort - sort events by time */
static int compare_events(const void *a, const void *b)
{
    const timeline_event_t *ea = (const timeline_event_t *)a;
    const timeline_event_t *eb = (const timeline_event_t *)b;
    
    if (ea->time_ms < eb->time_ms) return -1;
    if (ea->time_ms > eb->time_ms) return 1;
    return 0;
}

/* Print the timeline in formatted output */
void timeline_print(void)
{
    if (!g_timeline.enabled || g_timeline.event_count == 0)
    {
        return;
    }
    
    /* Sort events chronologically */
    qsort(g_timeline.events, g_timeline.event_count, 
          sizeof(timeline_event_t), compare_events);
    
    /* Print header */
    printf("\n%-6s %-20s %s\n", "PID", "EVENT", "TIME(ms)");
    
    /* Print separator line */
    printf("------ -------------------- --------\n");
    
    /* Print each event */
    for (int i = 0; i < g_timeline.event_count; i++)
    {
        timeline_event_t *event = &g_timeline.events[i];
        char event_str[64];
        
        switch (event->type)
        {
            case EVENT_PIPED:
                snprintf(event_str, sizeof(event_str), "piped(%d→%d)",
                         event->data.pipe_info.from_pid,
                         event->data.pipe_info.to_pid);
                break;
                
            case EVENT_EXITED:
                snprintf(event_str, sizeof(event_str), "exited(%d)",
                         event->data.exit_info.exit_code);
                break;
                
            case EVENT_SIGNALED:
                snprintf(event_str, sizeof(event_str), "signaled(%d)",
                         event->data.signal_info.signal);
                break;
                
            case EVENT_STOPPED:
                snprintf(event_str, sizeof(event_str), "stopped(%d)",
                         event->data.signal_info.signal);
                break;
                
            case EVENT_REDIRECTED:
                {
                    const char *dir_str;
                    switch (event->data.redirect_info.direction)
                    {
                        case 0:  dir_str = "<"; break;
                        case 1:  dir_str = ">"; break;
                        case 2:  dir_str = ">>"; break;
                        default: dir_str = "?"; break;
                    }
                    snprintf(event_str, sizeof(event_str), "redirected(%s%s)",
                             dir_str,
                             event->data.redirect_info.target ? 
                             event->data.redirect_info.target : "");
                }
                break;
                
            default:
                snprintf(event_str, sizeof(event_str), "%s",
                         timeline_event_type_str(event->type));
                break;
        }
        
        printf("%-6d %-20s %.0f\n", event->pid, event_str, event->time_ms);
    }
    
    printf("\n");
}
