#ifndef NEO_BOOTSTRAP_H
#define NEO_BOOTSTRAP_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define NEO_BOOTSTRAP_API __declspec(dllexport)
#else
#define NEO_BOOTSTRAP_API __attribute__((visibility("default")))
#endif

typedef enum neo_bootstrap_state {
    NEO_BOOTSTRAP_NOT_STARTED = 0,
    NEO_BOOTSTRAP_INITIALIZING = 1,
    NEO_BOOTSTRAP_READY_HOST_PENDING = 2,
    NEO_BOOTSTRAP_RUNTIME_READY = 3,
    NEO_BOOTSTRAP_FAILED = 4,
    NEO_BOOTSTRAP_STOPPED = 5
} neo_bootstrap_state_t;

typedef struct neo_bootstrap_status {
    neo_bootstrap_state_t state;
    char state_name[32];
    char bootstrap_version[32];
    char core_version[64];
    char error[256];
    unsigned long long init_duration_ms;
    int shutdown_called;
    int runtime_available;
    int game_capabilities_available;
    int dylib_loaded;
    int bootstrap_initializing;
    int bootstrap_initialized;
    int host_pending;
    int runtime_ready_flag;
    int failed_flag;
    int stopped_flag;
} neo_bootstrap_status_t;

/* Native interfaces supplied by a real in-process host. All pointers are
   borrowed; the host must keep them valid for the bootstrap lifetime. */
typedef struct neo_bootstrap_host_interfaces {
    void (*graphics_failure_set_handler)(void* context, void (*handler)(void* context));
} neo_bootstrap_host_interfaces_t;

/* Return codes shared by all C ABI functions. */
#define NEO_BOOTSTRAP_OK 0
#define NEO_BOOTSTRAP_INVALID_ARGUMENT 1
#define NEO_BOOTSTRAP_INVALID_STATE 2
#define NEO_BOOTSTRAP_INTERNAL_ERROR 3

NEO_BOOTSTRAP_API int neo_bootstrap_initialize(void);
NEO_BOOTSTRAP_API int neo_bootstrap_shutdown(void);
NEO_BOOTSTRAP_API int neo_bootstrap_get_status(neo_bootstrap_status_t* out_status);
NEO_BOOTSTRAP_API const char* neo_bootstrap_get_version(void);
NEO_BOOTSTRAP_API int neo_bootstrap_attach_host(const neo_bootstrap_host_interfaces_t* interfaces);
NEO_BOOTSTRAP_API int neo_bootstrap_set_documents_root(const char* path);
NEO_BOOTSTRAP_API int neo_bootstrap_get_marker_path(char* out_path, unsigned long long capacity);

/* Appends one diagnostic marker event. Used by the frame probe for
   installation/error evidence; never call this from a per-frame callback. */
NEO_BOOTSTRAP_API void neo_bootstrap_write_marker(const char* event, const char* detail);

#ifdef __cplusplus
}
#endif

#endif /* NEO_BOOTSTRAP_H */
