#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"
#include "shell_error.h"
#include "boot_sequence.h"
#include "vfs.h"
#include <string.h>
#include <stdio.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <SD.h>
#endif

extern "C" {
int cmd_fastfetch(terminal_state *term, int argc, char **argv);
extern const builtin_cmd cmd_fastfetch_def;
}

extern "C" const builtin_cmd cmd_fastfetch_def = {
    .name = "fastfetch",
    .handler = cmd_fastfetch,
    .help = "Display system info"
};

static void format_uptime(char *out, size_t out_size, uint32_t seconds) {
    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs = seconds % 60;
    snprintf(out, out_size, "%lu:%02lu:%02lu",
             (unsigned long)hours,
             (unsigned long)minutes,
             (unsigned long)secs);
}

static void format_bytes(char *out, size_t out_size, uint64_t bytes) {
    const char *suffix = "B";
    double value = (double)bytes;
    if (value >= 1024.0) {
        value /= 1024.0;
        suffix = "KB";
    }
    if (value >= 1024.0) {
        value /= 1024.0;
        suffix = "MB";
    }
    if (value >= 1024.0) {
        value /= 1024.0;
        suffix = "GB";
    }
    snprintf(out, out_size, "%.1f %s", value, suffix);
}

static int read_username(char *out, size_t out_len) {
    if (out == NULL || out_len == 0) {
        return 0;
    }
    out[0] = '\0';
    vfs_file_t *file = vfs_open("/etc/passwd", VFS_O_READ);
    if (file == NULL) {
        return 0;
    }
    char buf[128];
    ssize_t read_bytes = vfs_read(file, buf, sizeof(buf) - 1);
    vfs_close(file);
    if (read_bytes <= 0) {
        return 0;
    }
    buf[read_bytes] = '\0';
    char *newline = strchr(buf, '\n');
    if (newline) {
        *newline = '\0';
    }
    char *colon = strchr(buf, ':');
    if (colon) {
        *colon = '\0';
    }
    if (buf[0] == '\0') {
        return 0;
    }
    strncpy(out, buf, out_len - 1);
    out[out_len - 1] = '\0';
    return 1;
}

#ifdef ARDUINO
static int has_rgb565_ext(const char *name) {
    const char *ext = ".rgb565";
    size_t len = strlen(name);
    size_t ext_len = strlen(ext);
    if (len < ext_len) {
        return 0;
    }
    return strcmp(name + len - ext_len, ext) == 0;
}

static const char *basename_ptr(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int find_fastfetch_logo(const char *username, char *out_path, size_t out_len) {
    if (username == NULL || username[0] == '\0' || out_path == NULL || out_len == 0) {
        return 0;
    }
    out_path[0] = '\0';
    char dir_path[128];
    snprintf(dir_path, sizeof(dir_path), "/home/%s/.config/fastfetch", username);
    
    boot_sd_switch_to_sd_spi();
    File dir = SD.open(dir_path);
    if (!dir || !dir.isDirectory()) {
        if (dir) {
            dir.close();
        }
        boot_sd_restore_tft_spi();
        return 0;
    }
    
    char best_name[64] = {0};
    char best_path[256] = {0};
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            break;
        }
        const char *name = entry.name();
        const char *base = basename_ptr(name);
        if (has_rgb565_ext(base)) {
            if (best_name[0] == '\0' || strcmp(base, best_name) < 0) {
                strncpy(best_name, base, sizeof(best_name) - 1);
                best_name[sizeof(best_name) - 1] = '\0';
                if (name[0] == '/') {
                    strncpy(best_path, name, sizeof(best_path) - 1);
                    best_path[sizeof(best_path) - 1] = '\0';
                } else {
                    snprintf(best_path, sizeof(best_path), "%s/%s", dir_path, base);
                }
            }
        }
        entry.close();
    }
    dir.close();
    boot_sd_restore_tft_spi();
    
    if (best_path[0] == '\0') {
        return 0;
    }
    strncpy(out_path, best_path, out_len - 1);
    out_path[out_len - 1] = '\0';
    return 1;
}

static uint16_t *load_fastfetch_logo(const char *path, int *out_w, int *out_h, int zoom) {
    if (path == NULL || out_w == NULL || out_h == NULL) {
        return NULL;
    }
    const int src_w = 480;
    const int src_h = 320;
    if (zoom < 1) zoom = 1;
    const int target_w = 15 * 6 * zoom;
    const int target_h = (src_h * target_w) / src_w;
    if (target_h <= 0) {
        return NULL;
    }
    
    uint16_t *dest = (uint16_t*)malloc((size_t)target_w * (size_t)target_h * 2);
    if (dest == NULL) {
        return NULL;
    }
    
    size_t row_bytes = (size_t)src_w * 2;
    uint8_t *row = (uint8_t*)malloc(row_bytes);
    if (row == NULL) {
        free(dest);
        return NULL;
    }
    
    boot_sd_switch_to_sd_spi();
    File logo = SD.open(path, FILE_READ);
    if (!logo) {
        boot_sd_restore_tft_spi();
        free(row);
        free(dest);
        return NULL;
    }
    size_t expected = (size_t)src_w * (size_t)src_h * 2;
    if ((size_t)logo.size() < expected) {
        logo.close();
        boot_sd_restore_tft_spi();
        free(row);
        free(dest);
        return NULL;
    }
    
    for (int dy = 0; dy < target_h; dy++) {
        int sy = (dy * src_h) / target_h;
        size_t offset = (size_t)sy * row_bytes;
        logo.seek(offset);
        size_t read_bytes = logo.read(row, row_bytes);
        if (read_bytes != row_bytes) {
            break;
        }
        for (int dx = 0; dx < target_w; dx++) {
            int sx = (dx * src_w) / target_w;
            size_t idx = (size_t)sx * 2;
            uint16_t pix = (uint16_t)row[idx] | ((uint16_t)row[idx + 1] << 8);
            dest[dy * target_w + dx] = pix;
        }
    }
    
    logo.close();
    boot_sd_restore_tft_spi();
    free(row);
    
    *out_w = target_w;
    *out_h = target_h;
    return dest;
}

#endif

extern "C" void fastfetch_rescale_image(terminal_state *term) {
#ifdef ARDUINO
    if (term == NULL || !term->fastfetch_image_active || term->fastfetch_image_path[0] == '\0') {
        return;
    }
    if (term->fastfetch_image_pixels != NULL) {
        free(term->fastfetch_image_pixels);
        term->fastfetch_image_pixels = NULL;
    }
    term->fastfetch_image_w = 0;
    term->fastfetch_image_h = 0;
    
    int img_w = 0;
    int img_h = 0;
    int zoom = terminal_get_zoom();
    uint16_t *pixels = load_fastfetch_logo(term->fastfetch_image_path, &img_w, &img_h, zoom);
    if (pixels != NULL) {
        term->fastfetch_image_pixels = pixels;
        term->fastfetch_image_w = img_w;
        term->fastfetch_image_h = img_h;
        int char_height = 8 * (zoom < 1 ? 1 : zoom);
        int image_rows = (img_h + char_height - 1) / char_height;
        uint8_t text_lines = term->fastfetch_text_lines ? term->fastfetch_text_lines : 1;
        term->fastfetch_line_count = (uint8_t)((image_rows > text_lines) ? image_rows : text_lines);
    } else {
        term->fastfetch_image_active = 0;
        term->fastfetch_image_path[0] = '\0';
        term->fastfetch_line_count = 0;
    }
#else
    (void)term;
#endif
}

int cmd_fastfetch(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    if (argc > 1) {
        shell_error(term, "fastfetch: too many arguments");
        return SHELL_EINVAL;
    }
    
    char info_lines[8][96];
    int info_count = 0;
    
    char username[64] = "user";
    int has_username = 0;
#ifdef ARDUINO
    if (read_username(username, sizeof(username))) {
        has_username = 1;
    } else {
        strncpy(username, "user", sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
    }
#endif
    char user_line[96];
    snprintf(user_line, sizeof(user_line), "%s@TILIXI", username);
    snprintf(info_lines[info_count++], sizeof(info_lines[0]), "%s", user_line);
    int name_len = (int)strlen(user_line);
    if (name_len > (int)sizeof(info_lines[0]) - 1) {
        name_len = (int)sizeof(info_lines[0]) - 1;
    }
    char dash_line[96];
    memset(dash_line, '-', (size_t)name_len);
    dash_line[name_len] = '\0';
    snprintf(info_lines[info_count++], sizeof(info_lines[0]), "%s", dash_line);
    
    snprintf(info_lines[info_count++], sizeof(info_lines[0]), "OS:     TILIXI");
    snprintf(info_lines[info_count++], sizeof(info_lines[0]), "Host:   ESP32S3");
    
    char uptime_buf[32] = "0:00:00";
#ifdef ARDUINO
    uint32_t uptime_sec = millis() / 1000;
    format_uptime(uptime_buf, sizeof(uptime_buf), uptime_sec);
#endif
    snprintf(info_lines[info_count++], sizeof(info_lines[0]), "Uptime: %s", uptime_buf);
    snprintf(info_lines[info_count++], sizeof(info_lines[0]), "Shell:  damocles");
    
    char mem_line[96];
#ifdef ARDUINO
    uint32_t total_heap = ESP.getHeapSize();
    uint32_t free_heap = ESP.getFreeHeap();
    uint32_t used_heap = total_heap - free_heap;
    int mem_pct = total_heap ? (int)((used_heap * 100) / total_heap) : 0;
    char used_buf[32];
    char total_buf[32];
    format_bytes(used_buf, sizeof(used_buf), used_heap);
    format_bytes(total_buf, sizeof(total_buf), total_heap);
    snprintf(mem_line, sizeof(mem_line), "Memory: %s / %s (%d%%)",
             used_buf, total_buf, mem_pct);
#else
    snprintf(mem_line, sizeof(mem_line), "Memory: N/A");
#endif
    snprintf(info_lines[info_count++], sizeof(info_lines[0]), "%s", mem_line);
    
    char disk_line[96];
#ifdef ARDUINO
    uint64_t disk_total = 0;
    uint64_t disk_used = 0;
    boot_sd_switch_to_sd_spi();
    disk_total = (uint64_t)SD.cardSize();
    disk_used = (uint64_t)SD.usedBytes();
    boot_sd_restore_tft_spi();
    
    if (disk_total == 0) {
        snprintf(disk_line, sizeof(disk_line), "Disk:   N/A");
    } else {
        int disk_pct = (int)((disk_used * 100) / disk_total);
        char used_buf[32];
        char total_buf[32];
        format_bytes(used_buf, sizeof(used_buf), disk_used);
        format_bytes(total_buf, sizeof(total_buf), disk_total);
        snprintf(disk_line, sizeof(disk_line), "Disk:   %s / %s (%d%%)",
                 used_buf, total_buf, disk_pct);
    }
#else
    snprintf(disk_line, sizeof(disk_line), "Disk:   N/A");
#endif
    snprintf(info_lines[info_count++], sizeof(info_lines[0]), "%s", disk_line);
    
    term->fastfetch_image_active = 0;
    term->fastfetch_image_path[0] = '\0';
    term->fastfetch_start_row = 0;
    term->fastfetch_line_count = 0;
    term->fastfetch_text_lines = 0;
#ifdef ARDUINO
    if (term->fastfetch_image_pixels != NULL) {
        free(term->fastfetch_image_pixels);
        term->fastfetch_image_pixels = NULL;
    }
    term->fastfetch_image_w = 0;
    term->fastfetch_image_h = 0;
    char logo_path[256];
    if (has_username &&
        find_fastfetch_logo(username, logo_path, sizeof(logo_path))) {
        term->fastfetch_image_active = 1;
        strncpy(term->fastfetch_image_path, logo_path,
                sizeof(term->fastfetch_image_path) - 1);
        term->fastfetch_image_path[sizeof(term->fastfetch_image_path) - 1] = '\0';
        int img_w = 0;
        int img_h = 0;
        int zoom = terminal_get_zoom();
        uint16_t *pixels = load_fastfetch_logo(logo_path, &img_w, &img_h, zoom);
        if (pixels != NULL) {
            term->fastfetch_image_pixels = pixels;
            term->fastfetch_image_w = img_w;
            term->fastfetch_image_h = img_h;
            int char_height = 8 * (zoom < 1 ? 1 : zoom);
            int image_rows = (img_h + char_height - 1) / char_height;
            term->fastfetch_line_count = (uint8_t)((image_rows > info_count) ? image_rows : info_count);
        } else {
            term->fastfetch_image_active = 0;
            term->fastfetch_image_path[0] = '\0';
        }
    }
#endif

    terminal_newline(term);
    term->fastfetch_start_row = term->cursor_row;
    term->fastfetch_text_lines = (uint8_t)info_count;
    for (int i = 0; i < info_count; i++) {
        terminal_write_line(term, info_lines[i]);
    }
    terminal_newline(term);

    if (term->fastfetch_image_active && term->fastfetch_line_count > 0) {
        uint8_t desired_row = term->fastfetch_start_row + term->fastfetch_line_count + 1;
        while (term->cursor_row < desired_row) {
            terminal_newline(term);
        }
    }
    
    return SHELL_OK;
}

