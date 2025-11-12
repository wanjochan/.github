// mod_curl.c - libcurl HTTP client module for cosmorun v2
// Provides HTTP client functionality via dynamic loading of libcurl

#ifndef SHIM_RTLD_LAZY
#define SHIM_RTLD_LAZY 0x0001
#endif
#ifndef SHIM_RTLD_GLOBAL
#define SHIM_RTLD_GLOBAL 0x0100
#endif

/* Map standard dl* functions to System Layer equivalents */
#define dlopen(path, flags) shim_dlopen(path, flags)
#define dlsym(handle, symbol) shim_dlsym(handle, symbol)
#define dlclose(handle) shim_dlclose(handle)

/* ARM64 vararg trampoline wrapper for cross-ABI variadic function calls */
#ifdef __aarch64__
extern void *__dlsym_varg(void *vfunc, int variadic_type);
#endif

#include "mod_std.c"
#include "mod_curl.h"
#include "mod_error_impl.h"
#include "mod_ffi.c"

/* libcurl global init flags */
#define CURL_GLOBAL_ALL 0x03

/* ==================== Helper Functions ==================== */

static int curl_preferred_dlopen_flags(void) {
#if defined(_WIN32) || defined(_WIN64)
    return SHIM_RTLD_LAZY;
#else
    return SHIM_RTLD_LAZY | SHIM_RTLD_GLOBAL;
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
        "/opt/homebrew/Cellar/curl/8.13.0/lib/libcurl.dylib",
        "/opt/homebrew/lib/libcurl.dylib",
        "/opt/homebrew/lib/libcurl.4.dylib",
        "/usr/local/lib/libcurl.dylib",
        "/usr/local/lib/libcurl.4.dylib",
        "/usr/lib/libcurl.dylib",
        "/usr/lib/libcurl.4.dylib",
        "lib/libcurl.dylib",
        "./libcurl.dylib",
        "libcurl.dylib",
        "libcurl.4.dylib",
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
        if (requested_path && *requested_path && shim_strcmp(requested_path, *cand) == 0) {
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
    shim_file *fp = (shim_file*)userdata;
    return shim_fread(buffer, size, nitems, fp);
}

/* ==================== FFI Wrappers for Varargs Functions ==================== */

/* FFI wrapper for curl_easy_setopt with long value */
static CURLcode curl_easy_setopt_long(curl_context_t *ctx, CURLoption option, long value) {
    if (!ctx || !ctx->curl_easy_setopt_raw) return CURLE_FAILED_INIT;

    /* Prepare FFI call: CURLcode curl_easy_setopt(CURL*, CURLoption, long) */
    ffi_type *arg_types[3] = { &ffi_type_pointer, &ffi_type_sint32, &ffi_type_sint64 };
    ffi_cif cif;
    ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 3, &ffi_type_sint32, arg_types);
    if (status != FFI_OK) return CURLE_FAILED_INIT;

    /* Prepare arguments */
    void *arg_values[3] = { &ctx->curl_handle, &option, &value };

    /* Call function */
    CURLcode result;
    ffi_call(&cif, (void (*)(void))ctx->curl_easy_setopt_raw, &result, arg_values);

    return result;
}

/* FFI wrapper for curl_easy_setopt with pointer value */
static CURLcode curl_easy_setopt_ptr(curl_context_t *ctx, CURLoption option, const void *value) {
    if (!ctx || !ctx->curl_easy_setopt_raw) return CURLE_FAILED_INIT;

    /* Debug output for CURLOPT_URL */
    if (option == CURLOPT_URL) {
        fprintf(stderr, "[DEBUG] curl_easy_setopt_ptr: option=CURLOPT_URL, value=%p", value);
        if (value) fprintf(stderr, " (\"%s\")", (const char*)value);
        fprintf(stderr, "\n");
    }

    /* Prepare FFI call: CURLcode curl_easy_setopt(CURL*, CURLoption, void*) */
    ffi_type *arg_types[3] = { &ffi_type_pointer, &ffi_type_sint32, &ffi_type_pointer };
    ffi_cif cif;
    ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 3, &ffi_type_sint32, arg_types);
    if (status != FFI_OK) return CURLE_FAILED_INIT;

    /* Prepare arguments - note: value is already a pointer, pass &value so FFI can dereference it */
    void *arg_values[3] = { &ctx->curl_handle, &option, &value };

    /* Call function */
    CURLcode result;
    ffi_call(&cif, (void (*)(void))ctx->curl_easy_setopt_raw, &result, arg_values);

    return result;
}

/* FFI wrapper for curl_easy_getinfo with long pointer */
static CURLcode curl_easy_getinfo_long(curl_context_t *ctx, CURLINFO info, long *value) {
    if (!ctx || !ctx->curl_easy_getinfo_raw) return CURLE_FAILED_INIT;

    /* Prepare FFI call: CURLcode curl_easy_getinfo(CURL*, CURLINFO, long*) */
    ffi_type *arg_types[3] = { &ffi_type_pointer, &ffi_type_sint32, &ffi_type_pointer };
    ffi_cif cif;
    ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 3, &ffi_type_sint32, arg_types);
    if (status != FFI_OK) return CURLE_FAILED_INIT;

    /* Prepare arguments */
    void *arg_values[3] = { &ctx->curl_handle, &info, &value };

    /* Call function */
    CURLcode result;
    ffi_call(&cif, (void (*)(void))ctx->curl_easy_getinfo_raw, &result, arg_values);

    return result;
}

/* ==================== Context Management ==================== */

curl_context_t* curl_init(const char *lib_path) {
    void *handle = curl_dlopen_auto(lib_path ? lib_path : "");
    if (!handle) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_MODULE_LOAD_FAILED, "Failed to load libcurl library");
    }

    curl_context_t *ctx = (curl_context_t*)shim_malloc(sizeof(curl_context_t));
    if (!ctx) {
        dlclose(handle);
        COSMORUN_ERROR_NULL(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to allocate curl context");
    }

    shim_memset(ctx, 0, sizeof(curl_context_t));
    ctx->lib_handle = handle;
    ctx->timeout = 0;
    ctx->connect_timeout = 10;
    ctx->headers = std_hashmap_new();

    /* Load libcurl function pointers */
    ctx->curl_easy_init = (void* (*)(void))dlsym(handle, "curl_easy_init");
    ctx->curl_easy_setopt_raw = dlsym(handle, "curl_easy_setopt");  /* Varargs - call via ffi */
    ctx->curl_easy_getinfo_raw = dlsym(handle, "curl_easy_getinfo"); /* Varargs - call via ffi */
    ctx->curl_easy_perform = (CURLcode (*)(CURL*))dlsym(handle, "curl_easy_perform");
    ctx->curl_easy_cleanup = (void (*)(CURL*))dlsym(handle, "curl_easy_cleanup");
    ctx->curl_easy_strerror = (const char* (*)(CURLcode))dlsym(handle, "curl_easy_strerror");
    ctx->curl_slist_append = (curl_slist* (*)(curl_slist*, const char*))dlsym(handle, "curl_slist_append");
    ctx->curl_slist_free_all = (void (*)(curl_slist*))dlsym(handle, "curl_slist_free_all");
    ctx->curl_global_init = (CURLcode (*)(long))dlsym(handle, "curl_global_init");
    ctx->curl_global_cleanup = (void (*)(void))dlsym(handle, "curl_global_cleanup");

#ifdef __aarch64__
    /* Wrap variadic functions with ARM64 trampoline for correct ABI handling
     * curl_easy_setopt(CURL *handle, CURLoption option, ...) has 2 fixed args
     * curl_easy_getinfo(CURL *handle, CURLINFO info, ...) has 2 fixed args
     * NOTE: TinyCC variadic ABI fix is incomplete - libffi still needs trampolines
     */
    if (ctx->curl_easy_setopt_raw) {
        void *wrapped = __dlsym_varg(ctx->curl_easy_setopt_raw, 2);
        if (wrapped) ctx->curl_easy_setopt_raw = wrapped;
    }
    if (ctx->curl_easy_getinfo_raw) {
        void *wrapped = __dlsym_varg(ctx->curl_easy_getinfo_raw, 2);
        if (wrapped) ctx->curl_easy_getinfo_raw = wrapped;
    }
#endif

    if (!ctx->curl_easy_init || !ctx->curl_easy_setopt_raw || !ctx->curl_easy_perform ||
        !ctx->curl_easy_cleanup || !ctx->curl_easy_strerror) {
        std_hashmap_free(ctx->headers);
        shim_free(ctx);
        dlclose(handle);
        COSMORUN_ERROR_NULL(COSMORUN_ERR_SYMBOL_NOT_FOUND, "Failed to load required libcurl symbols");
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
        shim_free(ctx);
        dlclose(handle);
        COSMORUN_ERROR_NULL(COSMORUN_ERR_INIT_FAILED, "Failed to create curl easy handle");
    }

    /* Set error buffer */
    curl_easy_setopt_ptr(ctx, CURLOPT_ERRORBUFFER, (void*)ctx->error_buffer);

    /* Bypass proxy for localhost/127.0.0.1 to avoid interference with local testing */
    curl_easy_setopt_ptr(ctx, CURLOPT_NOPROXY, (void*)"localhost,127.0.0.1");

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

    shim_free(ctx);
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

    char *value_copy = shim_strdup(value);
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

/* Context for building header list in foreach callback */
typedef struct {
    curl_context_t *ctx;
    curl_slist *header_list;
} header_builder_ctx_t;

/* Callback for std_hashmap_foreach - formats and appends each header */
static void append_header_callback(const char *key, void *value, void *userdata) {
    header_builder_ctx_t *builder = (header_builder_ctx_t*)userdata;
    const char *val = (const char*)value;

    if (!key || !val || !builder || !builder->ctx || !builder->ctx->curl_slist_append) {
        return;
    }

    /* Format header as "Key: Value" */
    size_t header_len = shim_strlen(key) + shim_strlen(val) + 3; /* "Key: Value\0" */
    char *header_str = (char*)shim_malloc(header_len);
    if (!header_str) {
        return;
    }

    shim_snprintf(header_str, header_len, "%s: %s", key, val);

    /* Append to curl_slist */
    builder->header_list = builder->ctx->curl_slist_append(builder->header_list, header_str);

    shim_free(header_str);
}

/* Build curl_slist from headers hashmap */
static curl_slist* build_header_list(curl_context_t *ctx) {
    if (!ctx || !ctx->headers || !ctx->curl_slist_append) return NULL;

    /* Check if there are any headers to send */
    if (std_hashmap_size(ctx->headers) == 0) {
        return NULL;
    }

    header_builder_ctx_t builder = {
        .ctx = ctx,
        .header_list = NULL
    };

    /* Iterate through all headers and build the list */
    std_hashmap_foreach(ctx->headers, append_header_callback, &builder);

    return builder.header_list;
}

/* Common request setup */
static void setup_common_options(curl_context_t *ctx) {
    if (!ctx || !ctx->curl_handle) return;

    /* Set timeouts */
    if (ctx->timeout > 0) {
        curl_easy_setopt_long(ctx, CURLOPT_TIMEOUT, ctx->timeout);
    }
    if (ctx->connect_timeout > 0) {
        curl_easy_setopt_long(ctx, CURLOPT_CONNECTTIMEOUT, ctx->connect_timeout);
    }

    /* Follow redirects */
    curl_easy_setopt_long(ctx, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt_long(ctx, CURLOPT_MAXREDIRS, 10L);

    /* Set user agent */
    curl_easy_setopt_ptr(ctx, CURLOPT_USERAGENT, (void*)"mod_curl/1.0");
}

std_string_t* curl_get(curl_context_t *ctx, const char *url) {
    if (!ctx || !ctx->curl_handle || !url) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_INVALID_ARG, "Invalid arguments to curl_get");
    }

    std_string_t *response = std_string_new("");
    if (!response) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to allocate response buffer");
    }

    write_context_t wctx = { .str = response };

    /* Reset to GET */
    curl_easy_setopt_long(ctx, CURLOPT_HTTPGET, 1L);

    /* Setup common options (must be after HTTPGET which resets options) */
    setup_common_options(ctx);

    /* Set URL */
    curl_easy_setopt_ptr(ctx, CURLOPT_URL, (void*)url);

    /* Set write callback */
    curl_easy_setopt_ptr(ctx, CURLOPT_WRITEFUNCTION, (void*)write_callback);
    curl_easy_setopt_ptr(ctx, CURLOPT_WRITEDATA, (void*)&wctx);

    /* Set custom headers if any */
    curl_slist *header_list = build_header_list(ctx);
    if (header_list) {
        curl_easy_setopt_ptr(ctx, CURLOPT_HTTPHEADER, (void*)header_list);
    }

    /* Perform request */
    CURLcode res = ctx->curl_easy_perform(ctx->curl_handle);

    /* Cleanup headers */
    if (header_list && ctx->curl_slist_free_all) {
        ctx->curl_slist_free_all(header_list);
    }

    if (res != CURLE_OK) {
        const char *error_msg = ctx->curl_easy_strerror ? ctx->curl_easy_strerror(res) : "unknown error";
        fprintf(stderr, "[mod_curl] curl_easy_perform failed: %s (code=%d)\n", error_msg, res);
        std_string_free(response);
        COSMORUN_ERROR_NULL(COSMORUN_ERR_NETWORK, "curl_get request failed");
    }

    return response;
}

std_string_t* curl_post(curl_context_t *ctx, const char *url, const char *data) {
    if (!ctx || !ctx->curl_handle || !url || !data) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_INVALID_ARG, "Invalid arguments to curl_post");
    }

    std_string_t *response = std_string_new("");
    if (!response) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to allocate response buffer");
    }

    write_context_t wctx = { .str = response };

    /* Set POST data */
    curl_easy_setopt_long(ctx, CURLOPT_POST, 1L);
    curl_easy_setopt_ptr(ctx, CURLOPT_POSTFIELDS, (void*)data);
    curl_easy_setopt_long(ctx, CURLOPT_POSTFIELDSIZE, (long)shim_strlen(data));

    /* Setup common options (must be after POST which resets options) */
    setup_common_options(ctx);

    /* Set URL */
    curl_easy_setopt_ptr(ctx, CURLOPT_URL, (void*)url);

    /* Set write callback */
    curl_easy_setopt_ptr(ctx, CURLOPT_WRITEFUNCTION, (void*)write_callback);
    curl_easy_setopt_ptr(ctx, CURLOPT_WRITEDATA, (void*)&wctx);

    /* Set custom headers if any */
    curl_slist *header_list = build_header_list(ctx);
    if (header_list) {
        curl_easy_setopt_ptr(ctx, CURLOPT_HTTPHEADER, (void*)header_list);
    }

    /* Perform request */
    CURLcode res = ctx->curl_easy_perform(ctx->curl_handle);

    /* Cleanup headers */
    if (header_list && ctx->curl_slist_free_all) {
        ctx->curl_slist_free_all(header_list);
    }

    if (res != CURLE_OK) {
        const char *error_msg = ctx->curl_easy_strerror ? ctx->curl_easy_strerror(res) : "unknown error";
        fprintf(stderr, "[mod_curl] curl_easy_perform failed: %s (code=%d)\n", error_msg, res);
        std_string_free(response);
        COSMORUN_ERROR_NULL(COSMORUN_ERR_NETWORK, "curl_post request failed");
    }

    return response;
}

std_string_t* curl_post_content_type(curl_context_t *ctx, const char *url,
                                      const char *data, const char *content_type) {
    if (!ctx || !content_type) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_INVALID_ARG, "Invalid arguments to curl_post_content_type");
    }

    /* Add Content-Type header temporarily */
    curl_add_header(ctx, "Content-Type", content_type);

    std_string_t *result = curl_post(ctx, url, data);

    /* Remove Content-Type header after request */
    std_hashmap_remove(ctx->headers, "Content-Type");

    return result;
}

int curl_download(curl_context_t *ctx, const char *url, const char *filepath) {
    if (!ctx || !ctx->curl_handle || !url || !filepath) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_INVALID_ARG, "Invalid arguments to curl_download", -1);
    }

    shim_file *fp = shim_fopen(filepath, "wb");
    if (!fp) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_FILE_OPEN_FAILED, "Failed to open file for download", -1);
    }

    /* Reset to GET */
    curl_easy_setopt_long(ctx, CURLOPT_HTTPGET, 1L);

    /* Setup common options (must be after HTTPGET which resets options) */
    setup_common_options(ctx);

    /* Set URL */
    curl_easy_setopt_ptr(ctx, CURLOPT_URL, (void*)url);

    /* Write to file directly */
    curl_easy_setopt_ptr(ctx, CURLOPT_WRITEDATA, (void*)fp);
    curl_easy_setopt_ptr(ctx, CURLOPT_WRITEFUNCTION, (void*)NULL); /* Use default fwrite */

    /* Perform request */
    CURLcode res = ctx->curl_easy_perform(ctx->curl_handle);

    shim_fclose(fp);

    if (res != CURLE_OK) {
        unlink(filepath); /* Remove incomplete file */
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_NETWORK, "curl_download request failed", -1);
    }

    return 0;
}

std_string_t* curl_upload(curl_context_t *ctx, const char *url, const char *filepath) {
    if (!ctx || !ctx->curl_handle || !url || !filepath) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_INVALID_ARG, "Invalid arguments to curl_upload");
    }

    shim_file *fp = shim_fopen(filepath, "rb");
    if (!fp) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_FILE_OPEN_FAILED, "Failed to open file for upload");
    }

    /* Get file size */
    shim_fseek(fp, 0, SHIM_SEEK_END);
    long file_size = shim_ftell(fp);
    shim_fseek(fp, 0, SHIM_SEEK_SET);

    std_string_t *response = std_string_new("");
    if (!response) {
        shim_fclose(fp);
        return NULL;
    }

    write_context_t wctx = { .str = response };

    /* Enable upload */
    curl_easy_setopt_long(ctx, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt_ptr(ctx, CURLOPT_READFUNCTION, (void*)read_callback);
    curl_easy_setopt_ptr(ctx, CURLOPT_READDATA, (void*)fp);
    curl_easy_setopt_long(ctx, CURLOPT_INFILESIZE_LARGE, (long)file_size);

    /* Setup common options (must be after UPLOAD which resets options) */
    setup_common_options(ctx);

    /* Set URL */
    curl_easy_setopt_ptr(ctx, CURLOPT_URL, (void*)url);

    /* Set write callback for response */
    curl_easy_setopt_ptr(ctx, CURLOPT_WRITEFUNCTION, (void*)write_callback);
    curl_easy_setopt_ptr(ctx, CURLOPT_WRITEDATA, (void*)&wctx);

    /* Perform request */
    CURLcode res = ctx->curl_easy_perform(ctx->curl_handle);

    shim_fclose(fp);

    if (res != CURLE_OK) {
        std_string_free(response);
        COSMORUN_ERROR_NULL(COSMORUN_ERR_NETWORK, "curl_upload request failed");
    }

    return response;
}

/* ==================== Information Retrieval ==================== */

long curl_get_response_code(curl_context_t *ctx) {
    if (!ctx || !ctx->curl_handle || !ctx->curl_easy_getinfo_raw) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_INVALID_ARG, "Invalid curl context", -1);
    }

    long response_code = 0;
    CURLcode res = curl_easy_getinfo_long(ctx, CURLINFO_RESPONSE_CODE, &response_code);

    if (res != CURLE_OK) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_NETWORK, "Failed to get response code", -1);
    }

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
    if (!shim_strstr(body, "Example Domain")) {
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
