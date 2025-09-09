/**
 * CDP User Features Module - SAFE VERSION
 * Enhanced user experience features WITHOUT helper injection
 */

#include "cdp_internal.h"
#include <ctype.h>
#include <sys/stat.h>

/* Shortcut command structure */
typedef struct {
    const char *alias;
    const char *javascript;
    const char *description;
} ShortcutCommand;

/* Built-in shortcuts */
static ShortcutCommand shortcuts[] = {
    {".clear", "console.clear()", "Clear console"},
    {".url", "location.href", "Show current URL"},
    {".title", "document.title", "Show page title"},
    {".cookies", "document.cookie", "Show cookies"},
    {".reload", "location.reload()", "Reload page"},
    {".back", "history.back()", "Go back"},
    {".forward", "history.forward()", "Go forward"},
    {".ls", "Object.keys(window).slice(0,20).join(', ')", "List global objects (first 20)"},
    {".time", "new Date().toLocaleString()", "Current time"},
    {".timestamp", "Date.now()", "Unix timestamp"},
    {".random", "Math.random()", "Random number"},
    {".ua", "navigator.userAgent", "User agent"},
    {".screen", "JSON.stringify({width:screen.width,height:screen.height})", "Screen size"},
    {".viewport", "JSON.stringify({width:innerWidth,height:innerHeight})", "Viewport size"},
    {".scroll", "JSON.stringify({x:scrollX,y:scrollY})", "Scroll position"},
    {NULL, NULL, NULL}
};

/* Performance statistics */
typedef struct {
    int total_commands;
    double total_time_ms;
    double min_time_ms;
    double max_time_ms;
    time_t session_start;
} PerfStats;

static PerfStats perf_stats = {
    .total_commands = 0,
    .total_time_ms = 0,
    .min_time_ms = 999999,
    .max_time_ms = 0,
    .session_start = 0
};

/* Execute JavaScript from file */
int cdp_execute_script_file(const char *filename) {
    struct stat st;
    if (stat(filename, &st) != 0) {
        return cdp_error_push(CDP_ERR_INVALID_ARGS, 
                             "Script file not found: %s", filename);
    }
    
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return cdp_error_push(CDP_ERR_INVALID_ARGS,
                             "Cannot open script file: %s", filename);
    }
    
    if (verbose) {
        printf("Executing script: %s (%ld bytes)\n", filename, st.st_size);
    }
    
    // Read entire file for multi-line support
    char *script = malloc(st.st_size + 1);
    if (!script) {
        fclose(fp);
        return cdp_error_push(CDP_ERR_MEMORY, "Out of memory");
    }
    
    size_t read_size = fread(script, 1, st.st_size, fp);
    script[read_size] = '\0';
    fclose(fp);
    
    // Execute as single block for proper scope
    char *result = execute_javascript(script);
    
    if (result) {
        // Beautify output if it's an object
        char *beautified = cdp_beautify_output(result);
        printf("%s\n", beautified ? beautified : result);
        if (beautified != result) {
            free(beautified);
        }
    }
    
    free(script);
    return 0;
}

/* Beautify JavaScript output */
char* cdp_beautify_output(const char *result) {
    if (!result) return NULL;
    
    // Check if it's an object representation
    if (strstr(result, "[object ") || 
        (result[0] == '{' && result[strlen(result)-1] == '}') ||
        (result[0] == '[' && result[strlen(result)-1] == ']')) {
        
        // Try to get pretty JSON
        char js_expr[8192];
        snprintf(js_expr, sizeof(js_expr),
                "try { JSON.stringify(%s, null, 2) } catch(e) { String(%s) }",
                result, result);
        
        char *pretty = execute_javascript(js_expr);
        if (pretty && strcmp(pretty, result) != 0) {
            return pretty;
        }
    }
    
    // For simple values, return as-is
    return strdup(result);
}

/* Check if command is a shortcut */
const char* cdp_get_shortcut(const char *command) {
    if (!command || command[0] != '.') {
        return NULL;
    }
    
    for (int i = 0; shortcuts[i].alias; i++) {
        if (strcmp(command, shortcuts[i].alias) == 0) {
            return shortcuts[i].javascript;
        }
    }
    
    return NULL;
}

/* Show available shortcuts */
void cdp_show_shortcuts(void) {
    printf("\n=== Available Shortcuts ===\n");
    printf("%-15s %s\n", "Command", "Description");
    printf("%-15s %s\n", "-------", "-----------");
    
    for (int i = 0; shortcuts[i].alias; i++) {
        printf("%-15s %s\n", shortcuts[i].alias, shortcuts[i].description);
    }
    
    printf("\nUse .help for general help\n");
}

/* Initialize performance tracking */
void cdp_perf_init(void) {
    perf_stats.session_start = time(NULL);
    perf_stats.total_commands = 0;
    perf_stats.total_time_ms = 0;
    perf_stats.min_time_ms = 999999;
    perf_stats.max_time_ms = 0;
}

/* Track command execution time */
void cdp_perf_track(double time_ms) {
    perf_stats.total_commands++;
    perf_stats.total_time_ms += time_ms;
    
    if (time_ms < perf_stats.min_time_ms) {
        perf_stats.min_time_ms = time_ms;
    }
    if (time_ms > perf_stats.max_time_ms) {
        perf_stats.max_time_ms = time_ms;
    }
}

/* Show performance statistics */
void cdp_show_stats(void) {
    if (perf_stats.total_commands == 0) {
        printf("No commands executed yet.\n");
        return;
    }
    
    time_t session_time = time(NULL) - perf_stats.session_start;
    double avg_time = perf_stats.total_time_ms / perf_stats.total_commands;
    
    printf("\n=== Session Statistics ===\n");
    printf("Session duration:  %ld seconds\n", session_time);
    printf("Commands executed: %d\n", perf_stats.total_commands);
    printf("Average time:      %.2f ms\n", avg_time);
    printf("Min time:          %.2f ms\n", perf_stats.min_time_ms);
    printf("Max time:          %.2f ms\n", perf_stats.max_time_ms);
    printf("Total time:        %.2f ms\n", perf_stats.total_time_ms);
    
    if (session_time > 0) {
        printf("Commands/second:   %.2f\n", 
               (double)perf_stats.total_commands / session_time);
    }
}

/* Process user command with enhancements */
char* cdp_process_user_command(const char *input) {
    static char last_result[8192];
    
    // Safety check
    if (!input || !*input) {
        return NULL;
    }
    
    // Check for shortcuts
    const char *shortcut = cdp_get_shortcut(input);
    if (shortcut) {
        if (verbose) {
            printf("Expanding shortcut: %s -> %s\n", input, shortcut);
        }
        input = shortcut;
    }
    
    // Special commands
    if (strcmp(input, ".help") == 0) {
        cdp_show_shortcuts();
        return NULL;
    }
    if (strcmp(input, ".stats") == 0) {
        cdp_show_stats();
        return NULL;
    }
    
    // Track execution time
    clock_t start = clock();
    
    // Execute JavaScript
    char *result = execute_javascript(input);
    
    // Calculate execution time
    clock_t end = clock();
    double time_ms = ((double)(end - start)) / CLOCKS_PER_SEC * 1000;
    cdp_perf_track(time_ms);
    
    // Show execution time in verbose mode
    if (verbose && result) {
        printf("[Executed in %.2f ms]\n", time_ms);
    }
    
    // Beautify output
    if (result && *result) {
        char *beautified = cdp_beautify_output(result);
        if (beautified) {
            str_copy_safe(last_result, beautified, sizeof(last_result));
            if (beautified != result && beautified != last_result) {
                free(beautified);
            }
            return last_result;
        }
    }
    
    return result;
}

/* DISABLED: Helper injection causes crashes on Windows */
int cdp_inject_helpers(void) {
    // DO NOT INJECT HELPERS - CAUSES CRASHES
    return 0;
}