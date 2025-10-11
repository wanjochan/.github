// mod_curl.c - libcurl HTTP client module for cosmorun
// Provides HTTP client functionality via dynamic loading of libcurl

#define NULL (void*)0

#ifndef RTLD_LAZY
#define RTLD_LAZY 1
#endif
#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 256
#endif

#include "mod_curl.h"

/* libcurl global init flags */
#define CURL_GLOBAL_ALL 0x03

/* ==================== Helper Functions ==================== */

static int curl_preferred_dlopen_flags(void) {
#if defined(_WIN32) || defined(_WIN64)
    return RTLD_LAZY;
#else
    return RTLD_LAZY | RTLD_GLOBAL;
#endif
}

static void *curl_try_dlopen(const char *path) {
    if (!path || !*path) {
        return NULL;
    }

    int flags = curl_preferred_dlopen_flags();
    void *handle = dlopen(path, flags);
    if (handle) {
        return handle;
    }

    return dlopen(path, 0);
}

static void *curl_dlopen_auto(const char *requested_path) {
    static const char *const candidates[] = {
#if defined(_WIN32) || defined(_WIN64)
        "lib/libcurl.dll",
        "lib/curl.dll",
        "./libcurl.dll",
        "libcurl.dll",
        "./curl.dll",
        "curl.dll",
#elif defined(__APPLE__)
        "lib/libcurl.dylib",
        "./libcurl.dylib",
        "libcurl.dylib",
        "libcurl.4.dylib",
        "/usr/lib/libcurl.dylib",
#else
        "lib/libcurl.so",
        "./libcurl.so",
        "libcurl.so",
        "libcurl.so.4",
        "/usr/lib/libcurl.so",
        "/usr/lib/x86_64-linux-gnu/libcurl.so",
        "/usr/lib/x86_64-linux-gnu/libcurl.so.4",
#endif
        NULL
    };

    if (requested_path && *requested_path) {
        void *handle = curl_try_dlopen(requested_path);
        if (handle) {
            return handle;
        }
    }

    for (const char *const *cand = candidates; *cand; ++cand) {
        if (requested_path && *requested_path && strcmp(requested_path, *cand) == 0) {
            continue;
        }
        void *handle = curl_try_dlopen(*cand);
        if (handle) {
            return handle;
        }
    }
    return NULL;
}

/* Write callback for collecting response data */
typedef struct {
    std_string_t *str;
} write_context_t;

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    write_context_t *wctx = (write_context_t*)userdata;
    size_t total_size = size * nmemb;

    for (size_t i = 0; i < total_size; i++) {
        std_string_append_char(wctx->str, ptr[i]);
    }

    return total_size;
}

/* Read callback for uploading data */
static size_t read_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    FILE *fp = (FILE*)userdata;
    return fread(buffer, size, nitems, fp);
}

/* ==================== Context Management ==================== */

curl_context_t* curl_init(const char *lib_path) {
    void *handle = curl_dlopen_auto(lib_path ? lib_path : "");
    if (!handle) {
        return NULL;
    }

    curl_context_t *ctx = (curl_context_t*)malloc(sizeof(curl_context_t));
    if (!ctx) {
        dlclose(handle);
        return NULL;
    }

    memset(ctx, 0, sizeof(curl_context_t));
    ctx->lib_handle = handle;
    ctx->timeout = 0;
    ctx->connect_timeout = 10;
    ctx->headers = std_hashmap_new();

    /* Load libcurl function pointers */
    ctx->curl_easy_init = dlsym(handle, "curl_easy_init");
    ctx->curl_easy_setopt = dlsym(handle, "curl_easy_setopt");
    ctx->curl_easy_perform = dlsym(handle, "curl_easy_perform");
    ctx->curl_easy_cleanup = dlsym(handle, "curl_easy_cleanup");
    ctx->curl_easy_strerror = dlsym(handle, "curl_easy_strerror");
    ctx->curl_easy_getinfo = dlsym(handle, "curl_easy_getinfo");
    ctx->curl_slist_append = dlsym(handle, "curl_slist_append");
    ctx->curl_slist_free_all = dlsym(handle, "curl_slist_free_all");
    ctx->curl_global_init = dlsym(handle, "curl_global_init");
    ctx->curl_global_cleanup = dlsym(handle, "curl_global_cleanup");

    if (!ctx->curl_easy_init || !ctx->curl_easy_setopt || !ctx->curl_easy_perform ||
        !ctx->curl_easy_cleanup || !ctx->curl_easy_strerror) {
        std_hashmap_free(ctx->headers);
        free(ctx);
        dlclose(handle);
        return NULL;
    }

    /* Initialize libcurl globally */
    if (ctx->curl_global_init) {
        ctx->curl_global_init(CURL_GLOBAL_ALL);
    }

    /* Create easy handle */
    ctx->curl_handle = ctx->curl_easy_init();
    if (!ctx->curl_handle) {
        if (ctx->curl_global_cleanup) {
            ctx->curl_global_cleanup();
        }
        std_hashmap_free(ctx->headers);
        free(ctx);
        dlclose(handle);
        return NULL;
    }

    /* Set error buffer */
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_ERRORBUFFER, ctx->error_buffer);

    return ctx;
}

void curl_cleanup(curl_context_t *ctx) {
    if (!ctx) return;

    if (ctx->curl_handle && ctx->curl_easy_cleanup) {
        ctx->curl_easy_cleanup(ctx->curl_handle);
        ctx->curl_handle = NULL;
    }

    if (ctx->curl_global_cleanup) {
        ctx->curl_global_cleanup();
    }

    if (ctx->headers) {
        std_hashmap_free(ctx->headers);
        ctx->headers = NULL;
    }

    if (ctx->lib_handle) {
        dlclose(ctx->lib_handle);
        ctx->lib_handle = NULL;
    }

    free(ctx);
}

/* ==================== Configuration ==================== */

void curl_set_timeout(curl_context_t *ctx, long timeout_seconds) {
    if (!ctx) return;
    ctx->timeout = timeout_seconds;
}

void curl_set_connect_timeout(curl_context_t *ctx, long timeout_seconds) {
    if (!ctx) return;
    ctx->connect_timeout = timeout_seconds;
}

void curl_add_header(curl_context_t *ctx, const char *key, const char *value) {
    if (!ctx || !key || !value) return;

    char *value_copy = strdup(value);
    if (value_copy) {
        std_hashmap_set(ctx->headers, key, value_copy);
    }
}

void curl_clear_headers(curl_context_t *ctx) {
    if (!ctx || !ctx->headers) return;

    std_hashmap_free(ctx->headers);
    ctx->headers = std_hashmap_new();
}

/* ==================== HTTP Requests ==================== */

/* Build curl_slist from headers hashmap */
static curl_slist* build_header_list(curl_context_t *ctx) {
    if (!ctx || !ctx->headers || !ctx->curl_slist_append) return NULL;

    curl_slist *header_list = NULL;

    /* Simple iteration - build header strings and append to list */
    /* Note: We need to iterate through hashmap, but for now keep it simple */
    /* This is a placeholder - proper implementation would iterate std_hashmap */

    return header_list;
}

/* Common request setup */
static void setup_common_options(curl_context_t *ctx) {
    if (!ctx || !ctx->curl_handle) return;

    /* Set timeouts */
    if (ctx->timeout > 0) {
        ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_TIMEOUT, ctx->timeout);
    }
    if (ctx->connect_timeout > 0) {
        ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_CONNECTTIMEOUT, ctx->connect_timeout);
    }

    /* Follow redirects */
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_MAXREDIRS, 10L);

    /* Set user agent */
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_USERAGENT, "mod_curl/1.0");
}

std_string_t* curl_get(curl_context_t *ctx, const char *url) {
    if (!ctx || !ctx->curl_handle || !url) return NULL;

    std_string_t *response = std_string_new("");
    if (!response) return NULL;

    write_context_t wctx = { .str = response };

    /* Reset to GET */
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_HTTPGET, 1L);

    /* Set URL */
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_URL, url);

    /* Set write callback */
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_WRITEDATA, &wctx);

    /* Setup common options */
    setup_common_options(ctx);

    /* Set custom headers if any */
    curl_slist *header_list = build_header_list(ctx);
    if (header_list) {
        ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_HTTPHEADER, header_list);
    }

    /* Perform request */
    CURLcode res = ctx->curl_easy_perform(ctx->curl_handle);

    /* Cleanup headers */
    if (header_list && ctx->curl_slist_free_all) {
        ctx->curl_slist_free_all(header_list);
    }

    if (res != CURLE_OK) {
        std_string_free(response);
        return NULL;
    }

    return response;
}

std_string_t* curl_post(curl_context_t *ctx, const char *url, const char *data) {
    if (!ctx || !ctx->curl_handle || !url || !data) return NULL;

    std_string_t *response = std_string_new("");
    if (!response) return NULL;

    write_context_t wctx = { .str = response };

    /* Set URL */
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_URL, url);

    /* Set POST data */
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_POST, 1L);
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_POSTFIELDS, data);
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_POSTFIELDSIZE, (long)strlen(data));

    /* Set write callback */
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_WRITEDATA, &wctx);

    /* Setup common options */
    setup_common_options(ctx);

    /* Set custom headers if any */
    curl_slist *header_list = build_header_list(ctx);
    if (header_list) {
        ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_HTTPHEADER, header_list);
    }

    /* Perform request */
    CURLcode res = ctx->curl_easy_perform(ctx->curl_handle);

    /* Cleanup headers */
    if (header_list && ctx->curl_slist_free_all) {
        ctx->curl_slist_free_all(header_list);
    }

    if (res != CURLE_OK) {
        std_string_free(response);
        return NULL;
    }

    return response;
}

std_string_t* curl_post_content_type(curl_context_t *ctx, const char *url,
                                      const char *data, const char *content_type) {
    if (!ctx || !content_type) return NULL;

    /* Add Content-Type header temporarily */
    curl_add_header(ctx, "Content-Type", content_type);

    std_string_t *result = curl_post(ctx, url, data);

    /* Remove Content-Type header after request */
    std_hashmap_remove(ctx->headers, "Content-Type");

    return result;
}

int curl_download(curl_context_t *ctx, const char *url, const char *filepath) {
    if (!ctx || !ctx->curl_handle || !url || !filepath) return -1;

    FILE *fp = fopen(filepath, "wb");
    if (!fp) return -1;

    /* Reset to GET */
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_HTTPGET, 1L);

    /* Set URL */
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_URL, url);

    /* Write to file directly */
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_WRITEDATA, fp);
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_WRITEFUNCTION, NULL); /* Use default fwrite */

    /* Setup common options */
    setup_common_options(ctx);

    /* Perform request */
    CURLcode res = ctx->curl_easy_perform(ctx->curl_handle);

    fclose(fp);

    if (res != CURLE_OK) {
        unlink(filepath); /* Remove incomplete file */
        return -1;
    }

    return 0;
}

std_string_t* curl_upload(curl_context_t *ctx, const char *url, const char *filepath) {
    if (!ctx || !ctx->curl_handle || !url || !filepath) return NULL;

    FILE *fp = fopen(filepath, "rb");
    if (!fp) return NULL;

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    std_string_t *response = std_string_new("");
    if (!response) {
        fclose(fp);
        return NULL;
    }

    write_context_t wctx = { .str = response };

    /* Set URL */
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_URL, url);

    /* Enable upload */
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_UPLOAD, 1L);
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_READFUNCTION, read_callback);
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_READDATA, fp);
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_INFILESIZE_LARGE, (long long)file_size);

    /* Set write callback for response */
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    ctx->curl_easy_setopt(ctx->curl_handle, CURLOPT_WRITEDATA, &wctx);

    /* Setup common options */
    setup_common_options(ctx);

    /* Perform request */
    CURLcode res = ctx->curl_easy_perform(ctx->curl_handle);

    fclose(fp);

    if (res != CURLE_OK) {
        std_string_free(response);
        return NULL;
    }

    return response;
}

/* ==================== Information Retrieval ==================== */

long curl_get_response_code(curl_context_t *ctx) {
    if (!ctx || !ctx->curl_handle || !ctx->curl_easy_getinfo) return -1;

    long response_code = 0;
    CURLcode res = ctx->curl_easy_getinfo(ctx->curl_handle, CURLINFO_RESPONSE_CODE, &response_code);

    if (res != CURLE_OK) return -1;

    return response_code;
}

const char* curl_get_error(curl_context_t *ctx) {
    if (!ctx) return "Invalid context";

    if (ctx->error_buffer[0] != '\0') {
        return ctx->error_buffer;
    }

    return "No error";
}

/* ==================== Self Test ==================== */

int curl_selftest(const char *lib_path) {
    printf("=== libcurl Self Test (library hint: %s) ===\n",
           (lib_path && *lib_path) ? lib_path : "<auto>");

    curl_context_t *ctx = curl_init(lib_path);
    if (!ctx) {
        printf("✗ Failed to initialize libcurl context\n");
        return -1;
    }
    printf("✓ libcurl loaded successfully\n");

    /* Test HTTP GET to example.com */
    curl_set_timeout(ctx, 10);
    printf("Testing HTTP GET request to http://example.com...\n");

    std_string_t *response = curl_get(ctx, "http://example.com");
    if (!response) {
        printf("✗ HTTP GET failed: %s\n", curl_get_error(ctx));
        curl_cleanup(ctx);
        return -1;
    }

    long response_code = curl_get_response_code(ctx);
    printf("✓ HTTP GET successful (status: %ld, size: %zu bytes)\n",
           response_code, std_string_len(response));

    if (response_code != 200) {
        printf("✗ Unexpected response code: %ld\n", response_code);
        std_string_free(response);
        curl_cleanup(ctx);
        return -1;
    }

    /* Check response contains "Example Domain" */
    const char *body = std_string_cstr(response);
    if (!strstr(body, "Example Domain")) {
        printf("✗ Response doesn't contain expected content\n");
        std_string_free(response);
        curl_cleanup(ctx);
        return -1;
    }

    printf("✓ Response validated successfully\n");
    std_string_free(response);

    curl_cleanup(ctx);
    printf("✓ Self test completed successfully\n");
    return 0;
}

int curl_selftest_default(void) {
    return curl_selftest(NULL);
}
