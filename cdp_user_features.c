/**
 * CDP User Features Module - SAFE VERSION
 * Enhanced user experience features WITHOUT helper injection
 */

#include "cdp_internal.h"
#include "cdp_commands.h"
#include <ctype.h>
#include <sys/stat.h>

// SIMPLIFIED: Removed ShortcutCommand struct and C-side shortcuts array
// All shortcut management now handled by JS Enhanced API in cdp_enhanced.js
// This reduces C-side maintenance and eliminates duplicate command definitions

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
        cdp_log(CDP_LOG_INFO, "SCRIPT", "Executing script: %s (%ld bytes)", filename, st.st_size);
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
        cdp_log(CDP_LOG_INFO, "SCRIPT", "%s", beautified ? beautified : result);
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
    
    // GENERIC SOLUTION: Don't try to re-parse ANY results
    // The wrapper function in Chrome already handles all formatting
    // Just return the result as-is
    return strdup(result);
}

/* SIMPLIFIED: Use JS Enhanced API for command processing */
int cdp_execute_enhanced_command(const char *command, char *output, size_t output_size) {
    if (!command) return -1;
    
    // BEFORE: Complex templates with manual escaping
    // AFTER: Single clean template using QUOTE macro - eliminates escaping complexity
    char escaped_cmd[1024];
    json_escape_safe(escaped_cmd, command, sizeof(escaped_cmd));
    
    char js_call[2048];
    snprintf(js_call, sizeof(js_call), 
        QUOTE(window.CDP_Enhanced ? CDP_Enhanced.exec("%s") : 
              {"ok":false,"data":null,"err":"CDP_Enhanced not loaded"}), 
        escaped_cmd);
    
    return cdp_runtime_eval(js_call, 1, 0, output, output_size, 5000);
}

/* SIMPLIFIED: Get shortcuts from JS Enhanced API */
void cdp_show_shortcuts(void) {
    char help_output[4096];
    
    // Use JS Enhanced API to get help information
    int result = cdp_execute_enhanced_command("dispatcher.help()", help_output, sizeof(help_output));
    
    if (result >= 0) {
        cdp_log(CDP_LOG_INFO, "HELP", "\n=== Available Shortcuts (from Enhanced API) ===");
        cdp_log(CDP_LOG_INFO, "HELP", "%s", help_output);
    } else {
        // Fallback for basic help
        cdp_log(CDP_LOG_INFO, "HELP", "\n=== Enhanced API Help ===");
        cdp_log(CDP_LOG_INFO, "HELP", "DOM: .click, .set, .text, .html, .exists, .count, .visible");
        cdp_log(CDP_LOG_INFO, "HELP", "Batch: .texts, .attrs"); 
        cdp_log(CDP_LOG_INFO, "HELP", "Page: .url, .title, .time, .ua, .screen, .viewport");
        cdp_log(CDP_LOG_INFO, "HELP", "Action: .clear, .reload, .back, .forward");
        cdp_log(CDP_LOG_INFO, "HELP", "Use: CDP_Enhanced.exec('command') or direct JavaScript");
    }
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
        cdp_log(CDP_LOG_INFO, "STATS", "No commands executed yet.");
        return;
    }
    
    time_t session_time = time(NULL) - perf_stats.session_start;
    double avg_time = perf_stats.total_time_ms / perf_stats.total_commands;
    
    cdp_log(CDP_LOG_INFO, "STATS", "\n=== Session Statistics ===");
    cdp_log(CDP_LOG_INFO, "STATS", "Session duration:  %ld seconds", session_time);
    cdp_log(CDP_LOG_INFO, "STATS", "Commands executed: %d", perf_stats.total_commands);
    cdp_log(CDP_LOG_INFO, "STATS", "Average time:      %.2f ms", avg_time);
    cdp_log(CDP_LOG_INFO, "STATS", "Min time:          %.2f ms", perf_stats.min_time_ms);
    cdp_log(CDP_LOG_INFO, "STATS", "Max time:          %.2f ms", perf_stats.max_time_ms);
    cdp_log(CDP_LOG_INFO, "STATS", "Total time:        %.2f ms", perf_stats.total_time_ms);
    
    if (session_time > 0) {
        cdp_log(CDP_LOG_INFO, "STATS", "Commands/second:   %.2f", 
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
    
    // Try JS Enhanced API first (faster and more feature-rich)
    if (input[0] == '.') {
        char js_result[8192];
        int js_rc = cdp_execute_enhanced_command(input, js_result, sizeof(js_result));
        if (js_rc >= 0) {
            // Successfully handled by JS Enhanced API
            strncpy(last_result, js_result, sizeof(last_result)-1);
            last_result[sizeof(last_result)-1] = '\0';
            return last_result;
        }
        // Fall back to C implementation if JS fails
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
    
    // Enhanced performance tracking with timestamps
    struct timespec start_time, js_start, js_end, beautify_start, beautify_end;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    if (verbose) {
        cdp_log(CDP_LOG_DEBUG, "PERF", "Command start: %s", input);
    }
    
    // Execute JavaScript with detailed timing
    clock_gettime(CLOCK_MONOTONIC, &js_start);
    char *result = execute_javascript(input);
    clock_gettime(CLOCK_MONOTONIC, &js_end);
    
    double js_time_ms = (js_end.tv_sec - js_start.tv_sec) * 1000.0 + 
                       (js_end.tv_nsec - js_start.tv_nsec) / 1000000.0;
    
    if (verbose) {
        cdp_log(CDP_LOG_DEBUG, "PERF", "JS execution: %.3f ms", js_time_ms);
    }
    
    // Track beautification time
    clock_gettime(CLOCK_MONOTONIC, &beautify_start);
    
    // Beautify output with timing
    if (result && *result) {
        char *beautified = cdp_beautify_output(result);
        clock_gettime(CLOCK_MONOTONIC, &beautify_end);
        
        double beautify_time_ms = (beautify_end.tv_sec - beautify_start.tv_sec) * 1000.0 + 
                                 (beautify_end.tv_nsec - beautify_start.tv_nsec) / 1000000.0;
        
        if (beautified) {
            str_copy_safe(last_result, beautified, sizeof(last_result));
            if (beautified != result && beautified != last_result) {
                free(beautified);
            }
            
            // Total execution time
            struct timespec end_time;
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            double total_time_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + 
                                  (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
            
            // Track legacy performance
            cdp_perf_track(total_time_ms);
            
            if (verbose) {
                cdp_log(CDP_LOG_DEBUG, "PERF", "Beautification: %.3f ms", beautify_time_ms);
                cdp_log(CDP_LOG_DEBUG, "PERF", "Total execution: %.3f ms", total_time_ms);
            }
            
            return last_result;
        }
    }
    
    // Final timing for non-beautified results
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double total_time_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + 
                          (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
    
    cdp_perf_track(total_time_ms);
    
    if (verbose) {
        cdp_log(CDP_LOG_DEBUG, "PERF", "Total execution: %.3f ms (no beautification)", total_time_ms);
    }
    
    return result;
}

/* DISABLED: Helper injection causes crashes on Windows */
int cdp_inject_helpers(void) {
    // DO NOT INJECT HELPERS - CAUSES CRASHES
    return 0;
}
