/**
 * CDP Process Management Module Implementation
 * Enhanced Chrome process lifecycle management with multi-instance support
 */

#include "cdp_process.h"
#include "cdp_internal.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>

/* Global registry */
cdp_chrome_registry_t g_chrome_registry = {0};
static int registry_initialized = 0;
static cdp_health_callback_t health_callback = NULL;
static void* health_callback_data = NULL;

/* Forward declarations */
static int allocate_instance_id(void);
static int allocate_debug_port(void);
static int launch_chrome_process(const cdp_chrome_config_t* config, cdp_chrome_instance_t* instance);
static void update_instance_status(cdp_chrome_instance_t* instance, cdp_chrome_status_t status);
static int cleanup_chrome_temp_files(const char* user_data_dir);

/* Initialize Chrome registry */
int cdp_init_chrome_registry(void) {
    if (registry_initialized) {
        return CDP_PROCESS_SUCCESS;
    }
    
    memset(&g_chrome_registry, 0, sizeof(g_chrome_registry));
    
    if (pthread_mutex_init(&g_chrome_registry.registry_mutex, NULL) != 0) {
        return CDP_PROCESS_ERR_MEMORY;
    }
    
    g_chrome_registry.next_instance_id = 1;
    g_chrome_registry.next_debug_port = 9222;
    g_chrome_registry.health_check_interval = CDP_DEFAULT_HEALTH_CHECK_INTERVAL;
    g_chrome_registry.auto_cleanup_enabled = 1;
    
    /* Set default temp directory */
    const char* temp_dir = getenv("TMPDIR");
    if (!temp_dir) temp_dir = getenv("TMP");
    if (!temp_dir) temp_dir = "/tmp";
    
    snprintf(g_chrome_registry.temp_dir, sizeof(g_chrome_registry.temp_dir), 
             "%s/cdp_chrome", temp_dir);
    
    /* Create temp directory if it doesn't exist */
    mkdir(g_chrome_registry.temp_dir, 0755);
    
    registry_initialized = 1;
    return CDP_PROCESS_SUCCESS;
}

/* Cleanup Chrome registry */
int cdp_cleanup_chrome_registry(void) {
    if (!registry_initialized) {
        return CDP_PROCESS_SUCCESS;
    }
    
    pthread_mutex_lock(&g_chrome_registry.registry_mutex);
    
    /* Kill all running instances */
    for (int i = 0; i < g_chrome_registry.instance_count; i++) {
        cdp_chrome_instance_t* instance = &g_chrome_registry.instances[i];
        if (instance->status == CDP_CHROME_STATUS_RUNNING) {
            cdp_kill_chrome_instance(instance->instance_id, 1);
        }
        pthread_mutex_destroy(&instance->mutex);
    }
    
    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
    pthread_mutex_destroy(&g_chrome_registry.registry_mutex);
    
    registry_initialized = 0;
    return CDP_PROCESS_SUCCESS;
}

/* Create default Chrome configuration */
int cdp_create_chrome_config(cdp_chrome_config_t* config) {
    if (!config) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }
    
    memset(config, 0, sizeof(cdp_chrome_config_t));
    
    /* Set default values */
    config->debug_port = 0;  /* Will be auto-assigned */
    config->window_width = 1280;
    config->window_height = 720;
    config->headless = 1;  /* Default to headless for automation */
    config->incognito = 0;
    config->disable_gpu = 1;
    config->no_sandbox = 1;  /* Often needed in containerized environments */
    config->disable_dev_shm_usage = 1;
    config->memory_limit_mb = 512;
    config->timeout_sec = CDP_PROCESS_TIMEOUT_SEC;
    config->auto_restart = 1;
    config->max_restart_attempts = CDP_MAX_RESTART_ATTEMPTS;
    config->created_time = time(NULL);
    
    /* Set default user agent */
    snprintf(config->user_agent, sizeof(config->user_agent),
             "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
             "Chrome/120.0.0.0 Safari/537.36 CDP-Client/1.0");
    
    return CDP_PROCESS_SUCCESS;
}

/* Validate Chrome configuration */
int cdp_validate_chrome_config(const cdp_chrome_config_t* config) {
    if (!config) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }
    
    /* Check port range */
    if (config->debug_port != 0 && (config->debug_port < 1024 || config->debug_port > 65535)) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }
    
    /* Check window dimensions */
    if (config->window_width < 100 || config->window_width > 4096 ||
        config->window_height < 100 || config->window_height > 4096) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }
    
    /* Check memory limit */
    if (config->memory_limit_mb < 64 || config->memory_limit_mb > 8192) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }
    
    /* Check timeout */
    if (config->timeout_sec < 5 || config->timeout_sec > 300) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }
    
    /* Validate paths if specified */
    if (strlen(config->profile_dir) > 0) {
        struct stat st;
        if (stat(config->profile_dir, &st) == 0 && !S_ISDIR(st.st_mode)) {
            return CDP_PROCESS_ERR_INVALID_CONFIG;
        }
    }
    
    return CDP_PROCESS_SUCCESS;
}

/* Launch Chrome instance */
int cdp_launch_chrome_instance(const cdp_chrome_config_t* config, cdp_chrome_instance_t* instance) {
    if (!config || !instance) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }
    
    if (!registry_initialized) {
        int ret = cdp_init_chrome_registry();
        if (ret != CDP_PROCESS_SUCCESS) {
            return ret;
        }
    }
    
    /* Validate configuration */
    int ret = cdp_validate_chrome_config(config);
    if (ret != CDP_PROCESS_SUCCESS) {
        return ret;
    }
    
    pthread_mutex_lock(&g_chrome_registry.registry_mutex);
    
    /* Check instance limit */
    if (g_chrome_registry.instance_count >= CDP_MAX_CHROME_INSTANCES) {
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_ERR_INSTANCE_LIMIT_REACHED;
    }
    
    /* Find available slot */
    cdp_chrome_instance_t* new_instance = NULL;
    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        if (g_chrome_registry.instances[i].status == CDP_CHROME_STATUS_UNKNOWN) {
            new_instance = &g_chrome_registry.instances[i];
            break;
        }
    }
    
    if (!new_instance) {
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_ERR_INSTANCE_LIMIT_REACHED;
    }
    
    /* Initialize instance */
    memset(new_instance, 0, sizeof(cdp_chrome_instance_t));
    new_instance->instance_id = allocate_instance_id();
    new_instance->debug_port = (config->debug_port > 0) ? config->debug_port : allocate_debug_port();
    new_instance->config = *config;
    new_instance->start_time = time(NULL);
    new_instance->auto_restart_enabled = config->auto_restart;
    
    if (pthread_mutex_init(&new_instance->mutex, NULL) != 0) {
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_ERR_MEMORY;
    }
    
    /* Launch Chrome process */
    ret = launch_chrome_process(config, new_instance);
    if (ret != CDP_PROCESS_SUCCESS) {
        pthread_mutex_destroy(&new_instance->mutex);
        memset(new_instance, 0, sizeof(cdp_chrome_instance_t));
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return ret;
    }
    
    g_chrome_registry.instance_count++;
    
    /* Copy instance data to output */
    *instance = *new_instance;
    
    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
    
    return CDP_PROCESS_SUCCESS;
}

/* Allocate unique instance ID */
static int allocate_instance_id(void) {
    return g_chrome_registry.next_instance_id++;
}

/* Allocate available debug port */
static int allocate_debug_port(void) {
    int port = g_chrome_registry.next_debug_port;
    
    /* Check if port is already in use */
    while (port < 65535) {
        int in_use = 0;
        for (int i = 0; i < g_chrome_registry.instance_count; i++) {
            if (g_chrome_registry.instances[i].debug_port == port) {
                in_use = 1;
                break;
            }
        }
        
        if (!in_use) {
            g_chrome_registry.next_debug_port = port + 1;
            return port;
        }
        port++;
    }
    
    return 9222;  /* Fallback to default */
}

/* Update instance status with thread safety */
static void update_instance_status(cdp_chrome_instance_t* instance, cdp_chrome_status_t status) {
    if (!instance) return;
    
    pthread_mutex_lock(&instance->mutex);
    instance->status = status;
    instance->last_activity = time(NULL);
    pthread_mutex_unlock(&instance->mutex);
}

/* Get Chrome status string */
const char* cdp_chrome_status_to_string(cdp_chrome_status_t status) {
    switch (status) {
        case CDP_CHROME_STATUS_UNKNOWN: return "unknown";
        case CDP_CHROME_STATUS_STARTING: return "starting";
        case CDP_CHROME_STATUS_RUNNING: return "running";
        case CDP_CHROME_STATUS_STOPPING: return "stopping";
        case CDP_CHROME_STATUS_STOPPED: return "stopped";
        case CDP_CHROME_STATUS_CRASHED: return "crashed";
        case CDP_CHROME_STATUS_FAILED: return "failed";
        default: return "invalid";
    }
}

/* Launch Chrome process implementation */
static int launch_chrome_process(const cdp_chrome_config_t* config, cdp_chrome_instance_t* instance) {
    /* Find Chrome executable */
    extern char* find_chrome_executable(void);  /* From cdp_chrome.c */
    char* chrome_path = find_chrome_executable();
    if (!chrome_path || strlen(chrome_path) == 0) {
        snprintf(instance->last_error, sizeof(instance->last_error),
                "Chrome executable not found");
        return CDP_PROCESS_ERR_LAUNCH_FAILED;
    }

    /* Create user data directory if specified */
    if (strlen(config->user_data_dir) > 0) {
        if (mkdir(config->user_data_dir, 0755) != 0 && errno != EEXIST) {
            snprintf(instance->last_error, sizeof(instance->last_error),
                    "Failed to create user data directory: %s", strerror(errno));
            return CDP_PROCESS_ERR_LAUNCH_FAILED;
        }
        strncpy(instance->user_data_dir, config->user_data_dir, sizeof(instance->user_data_dir) - 1);
    } else {
        /* Create temporary user data directory */
        snprintf(instance->user_data_dir, sizeof(instance->user_data_dir),
                "%s/chrome_instance_%d", g_chrome_registry.temp_dir, instance->instance_id);
        if (mkdir(instance->user_data_dir, 0755) != 0 && errno != EEXIST) {
            snprintf(instance->last_error, sizeof(instance->last_error),
                    "Failed to create temp user data directory: %s", strerror(errno));
            return CDP_PROCESS_ERR_LAUNCH_FAILED;
        }
    }

    /* Build Chrome command line arguments */
    char* args[64];
    int argc = 0;

    args[argc++] = chrome_path;

    /* Debug port */
    static char debug_port_arg[64];
    snprintf(debug_port_arg, sizeof(debug_port_arg), "--remote-debugging-port=%d", instance->debug_port);
    args[argc++] = debug_port_arg;

    /* User data directory */
    static char user_data_arg[CDP_MAX_PATH_LENGTH + 32];
    snprintf(user_data_arg, sizeof(user_data_arg), "--user-data-dir=%s", instance->user_data_dir);
    args[argc++] = user_data_arg;

    /* Window size */
    static char window_size_arg[64];
    snprintf(window_size_arg, sizeof(window_size_arg), "--window-size=%d,%d",
             config->window_width, config->window_height);
    args[argc++] = window_size_arg;

    /* Common flags for automation */
    if (config->headless) {
        args[argc++] = "--headless";
    }
    if (config->no_sandbox) {
        args[argc++] = "--no-sandbox";
    }
    if (config->disable_gpu) {
        args[argc++] = "--disable-gpu";
    }
    if (config->disable_dev_shm_usage) {
        args[argc++] = "--disable-dev-shm-usage";
    }
    if (config->incognito) {
        args[argc++] = "--incognito";
    }

    /* Additional flags */
    args[argc++] = "--disable-background-timer-throttling";
    args[argc++] = "--disable-backgrounding-occluded-windows";
    args[argc++] = "--disable-renderer-backgrounding";
    args[argc++] = "--disable-features=TranslateUI";
    args[argc++] = "--disable-ipc-flooding-protection";
    args[argc++] = "--no-first-run";
    args[argc++] = "--no-default-browser-check";

    /* Proxy settings */
    static char proxy_arg[512];
    if (strlen(config->proxy_server) > 0) {
        snprintf(proxy_arg, sizeof(proxy_arg), "--proxy-server=%s", config->proxy_server);
        args[argc++] = proxy_arg;
    }

    /* User agent */
    static char user_agent_arg[768];
    if (strlen(config->user_agent) > 0) {
        snprintf(user_agent_arg, sizeof(user_agent_arg), "--user-agent=%s", config->user_agent);
        args[argc++] = user_agent_arg;
    }

    /* Memory limit */
    static char memory_arg[64];
    snprintf(memory_arg, sizeof(memory_arg), "--max_old_space_size=%d", config->memory_limit_mb);
    args[argc++] = memory_arg;

    /* Extra flags from config */
    static char flags_copy[CDP_MAX_FLAGS_LENGTH];
    if (strlen(config->extra_flags) > 0) {
        /* Simple parsing - split by spaces */
        strncpy(flags_copy, config->extra_flags, sizeof(flags_copy) - 1);
        flags_copy[sizeof(flags_copy) - 1] = '\0';
        char* token = strtok(flags_copy, " ");
        while (token && argc < 60) {  /* Leave room for NULL terminator */
            args[argc++] = token;
            token = strtok(NULL, " ");
        }
    }

    /* Default page */
    args[argc++] = "about:blank";
    args[argc] = NULL;

    /* Update status */
    update_instance_status(instance, CDP_CHROME_STATUS_STARTING);

    /* Fork and exec Chrome */
    pid_t pid = fork();
    if (pid == -1) {
        snprintf(instance->last_error, sizeof(instance->last_error),
                "Failed to fork process: %s", strerror(errno));
        return CDP_PROCESS_ERR_LAUNCH_FAILED;
    }

    if (pid == 0) {
        /* Child process - exec Chrome */
        execv(chrome_path, args);
        /* If we get here, exec failed */
        fprintf(stderr, "Failed to exec Chrome: %s\n", strerror(errno));
        _exit(1);
    }

    /* Parent process */
    instance->pid = pid;
    update_instance_status(instance, CDP_CHROME_STATUS_RUNNING);

    /* Wait a moment for Chrome to start */
    usleep(500000);  /* 500ms */

    /* Verify Chrome is still running */
    if (!cdp_is_chrome_running(pid)) {
        snprintf(instance->last_error, sizeof(instance->last_error),
                "Chrome process exited immediately after launch");
        update_instance_status(instance, CDP_CHROME_STATUS_FAILED);
        return CDP_PROCESS_ERR_LAUNCH_FAILED;
    }

    return CDP_PROCESS_SUCCESS;
}

/* Check if Chrome process is running */
int cdp_is_chrome_running(pid_t pid) {
    if (pid <= 0) return 0;

    /* Send signal 0 to check if process exists */
    return (kill(pid, 0) == 0);
}

/* Kill Chrome instance */
int cdp_kill_chrome_instance(int instance_id, int force) {
    if (!registry_initialized) {
        return CDP_PROCESS_ERR_INSTANCE_NOT_FOUND;
    }

    pthread_mutex_lock(&g_chrome_registry.registry_mutex);

    cdp_chrome_instance_t* instance = NULL;
    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        if (g_chrome_registry.instances[i].instance_id == instance_id) {
            instance = &g_chrome_registry.instances[i];
            break;
        }
    }

    if (!instance || instance->status == CDP_CHROME_STATUS_UNKNOWN) {
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_ERR_INSTANCE_NOT_FOUND;
    }

    pthread_mutex_lock(&instance->mutex);

    if (instance->status == CDP_CHROME_STATUS_STOPPED) {
        pthread_mutex_unlock(&instance->mutex);
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_SUCCESS;
    }

    update_instance_status(instance, CDP_CHROME_STATUS_STOPPING);

    int result = CDP_PROCESS_SUCCESS;

    if (instance->pid > 0) {
        /* Try graceful shutdown first */
        if (!force && kill(instance->pid, SIGTERM) == 0) {
            /* Wait up to 10 seconds for graceful shutdown */
            for (int i = 0; i < 100; i++) {
                if (!cdp_is_chrome_running(instance->pid)) {
                    break;
                }
                usleep(100000);  /* 100ms */
            }
        }

        /* Force kill if still running */
        if (cdp_is_chrome_running(instance->pid)) {
            if (kill(instance->pid, SIGKILL) != 0) {
                snprintf(instance->last_error, sizeof(instance->last_error),
                        "Failed to kill Chrome process: %s", strerror(errno));
                result = CDP_PROCESS_ERR_KILL_FAILED;
            }
        }

        /* Wait for process to exit */
        int status;
        waitpid(instance->pid, &status, 0);
    }

    /* Cleanup resources */
    cleanup_chrome_temp_files(instance->user_data_dir);

    update_instance_status(instance, CDP_CHROME_STATUS_STOPPED);

    pthread_mutex_unlock(&instance->mutex);

    /* Remove from registry */
    if (result == CDP_PROCESS_SUCCESS) {
        pthread_mutex_destroy(&instance->mutex);
        memset(instance, 0, sizeof(cdp_chrome_instance_t));
        g_chrome_registry.instance_count--;
    }

    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);

    return result;
}

/* Cleanup Chrome temporary files */
static int cleanup_chrome_temp_files(const char* user_data_dir) {
    if (!user_data_dir || strlen(user_data_dir) == 0) {
        return CDP_PROCESS_SUCCESS;
    }

    /* Only cleanup temp directories we created */
    if (strstr(user_data_dir, g_chrome_registry.temp_dir) != user_data_dir) {
        return CDP_PROCESS_SUCCESS;
    }

    /* Simple recursive directory removal */
    char command[CDP_MAX_PATH_LENGTH + 32];
    snprintf(command, sizeof(command), "rm -rf \"%s\"", user_data_dir);

    int result = system(command);
    return (result == 0) ? CDP_PROCESS_SUCCESS : CDP_PROCESS_ERR_CLEANUP_FAILED;
}

/* List Chrome instances */
int cdp_list_chrome_instances(cdp_chrome_instance_t** instances, int* count) {
    if (!instances || !count) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }

    if (!registry_initialized) {
        *instances = NULL;
        *count = 0;
        return CDP_PROCESS_SUCCESS;
    }

    pthread_mutex_lock(&g_chrome_registry.registry_mutex);

    /* Count active instances */
    int active_count = 0;
    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        if (g_chrome_registry.instances[i].status != CDP_CHROME_STATUS_UNKNOWN) {
            active_count++;
        }
    }

    if (active_count == 0) {
        *instances = NULL;
        *count = 0;
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_SUCCESS;
    }

    /* Allocate result array */
    cdp_chrome_instance_t* result = malloc(active_count * sizeof(cdp_chrome_instance_t));
    if (!result) {
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_ERR_MEMORY;
    }

    /* Copy active instances */
    int result_index = 0;
    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        if (g_chrome_registry.instances[i].status != CDP_CHROME_STATUS_UNKNOWN) {
            result[result_index++] = g_chrome_registry.instances[i];
        }
    }

    *instances = result;
    *count = active_count;

    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
    return CDP_PROCESS_SUCCESS;
}

/* Get instance status */
int cdp_get_instance_status(int instance_id, cdp_chrome_instance_t* status) {
    if (!status) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }

    if (!registry_initialized) {
        return CDP_PROCESS_ERR_INSTANCE_NOT_FOUND;
    }

    pthread_mutex_lock(&g_chrome_registry.registry_mutex);

    cdp_chrome_instance_t* instance = NULL;
    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        if (g_chrome_registry.instances[i].instance_id == instance_id) {
            instance = &g_chrome_registry.instances[i];
            break;
        }
    }

    if (!instance || instance->status == CDP_CHROME_STATUS_UNKNOWN) {
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_ERR_INSTANCE_NOT_FOUND;
    }

    pthread_mutex_lock(&instance->mutex);
    *status = *instance;
    pthread_mutex_unlock(&instance->mutex);

    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
    return CDP_PROCESS_SUCCESS;
}

/* Check instance health */
int cdp_check_instance_health(int instance_id) {
    if (!registry_initialized) {
        return CDP_PROCESS_ERR_INSTANCE_NOT_FOUND;
    }

    pthread_mutex_lock(&g_chrome_registry.registry_mutex);

    cdp_chrome_instance_t* instance = NULL;
    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        if (g_chrome_registry.instances[i].instance_id == instance_id) {
            instance = &g_chrome_registry.instances[i];
            break;
        }
    }

    if (!instance || instance->status == CDP_CHROME_STATUS_UNKNOWN) {
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_ERR_INSTANCE_NOT_FOUND;
    }

    pthread_mutex_lock(&instance->mutex);

    int result = CDP_PROCESS_SUCCESS;

    /* Check if process is still running */
    if (!cdp_is_chrome_running(instance->pid)) {
        update_instance_status(instance, CDP_CHROME_STATUS_CRASHED);
        instance->health_check_failures++;

        /* Trigger health callback if set */
        if (health_callback) {
            health_callback(instance, health_callback_data);
        }

        result = CDP_PROCESS_ERR_HEALTH_CHECK_FAILED;
    } else {
        /* Process is running - update last health check time */
        instance->last_health_check = time(NULL);
        instance->health_check_failures = 0;
    }

    pthread_mutex_unlock(&instance->mutex);
    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);

    return result;
}

/* Set health check callback */
int cdp_set_health_check_callback(cdp_health_callback_t callback, void* user_data) {
    health_callback = callback;
    health_callback_data = user_data;
    return CDP_PROCESS_SUCCESS;
}

/* Emergency cleanup all Chrome processes */
int cdp_emergency_cleanup_chrome_processes(void) {
    if (!registry_initialized) {
        return CDP_PROCESS_SUCCESS;
    }

    pthread_mutex_lock(&g_chrome_registry.registry_mutex);

    int cleaned_count = 0;

    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        cdp_chrome_instance_t* instance = &g_chrome_registry.instances[i];

        if (instance->status != CDP_CHROME_STATUS_UNKNOWN &&
            instance->status != CDP_CHROME_STATUS_STOPPED) {

            /* Force kill the process */
            if (instance->pid > 0 && cdp_is_chrome_running(instance->pid)) {
                kill(instance->pid, SIGKILL);
                waitpid(instance->pid, NULL, 0);
            }

            /* Cleanup temp files */
            cleanup_chrome_temp_files(instance->user_data_dir);

            /* Reset instance */
            pthread_mutex_destroy(&instance->mutex);
            memset(instance, 0, sizeof(cdp_chrome_instance_t));
            cleaned_count++;
        }
    }

    g_chrome_registry.instance_count = 0;

    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);

    return cleaned_count;
}

/* Copy Chrome configuration */
int cdp_copy_chrome_config(const cdp_chrome_config_t* src, cdp_chrome_config_t* dst) {
    if (!src || !dst) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }

    *dst = *src;
    return CDP_PROCESS_SUCCESS;
}

/* Get registry statistics */
int cdp_get_registry_stats(int* total_instances, int* running_instances, int* failed_instances) {
    if (!total_instances || !running_instances || !failed_instances) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }

    if (!registry_initialized) {
        *total_instances = 0;
        *running_instances = 0;
        *failed_instances = 0;
        return CDP_PROCESS_SUCCESS;
    }

    pthread_mutex_lock(&g_chrome_registry.registry_mutex);

    int total = 0, running = 0, failed = 0;

    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        cdp_chrome_instance_t* instance = &g_chrome_registry.instances[i];
        if (instance->status != CDP_CHROME_STATUS_UNKNOWN) {
            total++;
            if (instance->status == CDP_CHROME_STATUS_RUNNING) {
                running++;
            } else if (instance->status == CDP_CHROME_STATUS_CRASHED ||
                      instance->status == CDP_CHROME_STATUS_FAILED) {
                failed++;
            }
        }
    }

    *total_instances = total;
    *running_instances = running;
    *failed_instances = failed;

    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);

    return CDP_PROCESS_SUCCESS;
}

/* Get process error string */
const char* cdp_process_error_to_string(cdp_process_error_t error) {
    switch (error) {
        case CDP_PROCESS_SUCCESS: return "success";
        case CDP_PROCESS_ERR_INVALID_CONFIG: return "invalid configuration";
        case CDP_PROCESS_ERR_LAUNCH_FAILED: return "launch failed";
        case CDP_PROCESS_ERR_INSTANCE_NOT_FOUND: return "instance not found";
        case CDP_PROCESS_ERR_INSTANCE_LIMIT_REACHED: return "instance limit reached";
        case CDP_PROCESS_ERR_KILL_FAILED: return "kill failed";
        case CDP_PROCESS_ERR_HEALTH_CHECK_FAILED: return "health check failed";
        case CDP_PROCESS_ERR_CLEANUP_FAILED: return "cleanup failed";
        case CDP_PROCESS_ERR_PORT_CONFLICT: return "port conflict";
        case CDP_PROCESS_ERR_PERMISSION_DENIED: return "permission denied";
        case CDP_PROCESS_ERR_TIMEOUT: return "timeout";
        case CDP_PROCESS_ERR_MEMORY: return "memory error";
        default: return "unknown error";
    }
}
