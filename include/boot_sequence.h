#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// boot sequence status
typedef enum {
    BOOT_STATUS_OK,
    BOOT_STATUS_FAIL
} boot_status_t;

// boot sequence step result
typedef struct {
    boot_status_t status;
    const char *message;
} boot_step_result_t;

// boot sequence functions
void boot_sequence_init(void);
boot_step_result_t boot_step(const char *step_name, int (*init_func)(void));
void boot_display_message(const char *step_name, boot_status_t status);
void boot_halt(const char *message);
void boot_sequence_run(void);

// emergency and desktop functions
void boot_emergency_tty(const char *failure_reason);
void boot_start_desktop(void);
void boot_render_terminal(void);
int boot_verify_systems_ready(void);

// individual boot step functions (return 0 on success, non-zero on failure)
int boot_low_level_bringup(void);
int boot_init_core_services(void);
int boot_mount_sd(void);
int boot_init_sd_filesystem(void);
int boot_init_os_subsystems(void);
int boot_register_commands(void);
int boot_init_processes(void);
int boot_start_event_loop(void);

// SD card wrapper functions
int boot_sd_mount(void);
void boot_sd_restore_tft_spi(void);
void boot_sd_switch_to_sd_spi(void);
int boot_sd_available(void);
int boot_sd_is_directory_empty(const char *path);
int boot_sd_ensure_directory(const char *path);
int boot_sd_ensure_file(const char *path, const char *content);

#ifdef __cplusplus
}
#endif

