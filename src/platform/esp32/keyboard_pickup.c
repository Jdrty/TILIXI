/*
 * USB Keyboard Pickup for ESP-IDF
 * 
 * Connects to USB keyboard on GPIO 20 (D+) and GPIO 19 (D-) as specified in WIRING.md
 * When a keyboard key is pressed, it prints to the TFT screen where the terminal prompt is.
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "driver/gpio.h"

#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"

// Include terminal system headers
#define IDF_VER 1  // Enable ESP-IDF specific code
#define PLATFORM_ESP32 1
#include "keyboard_core.h"
#include "event_processor.h"
#include "terminal.h"
#include "terminal_cmd.h"
#include "boot_splash.h"

static const char *TAG = "keyboard_pickup";

QueueHandle_t app_event_queue = NULL;

/**
 * @brief APP event group
 */
typedef enum {
    APP_EVENT = 0,
    APP_EVENT_HID_HOST
} app_event_group_t;

/**
 * @brief APP event queue
 */
typedef struct {
    app_event_group_t event_group;
    struct {
        hid_host_device_handle_t handle;
        hid_host_driver_event_t event;
        void *arg;
    } hid_host_device;
} app_event_queue_t;

/**
 * @brief Key event
 */
typedef struct {
    enum key_state {
        KEY_STATE_PRESSED = 0x00,
        KEY_STATE_RELEASED = 0x01
    } state;
    uint8_t modifier;
    uint8_t key_code;
} key_event_t;

/**
 * @brief Key buffer scan code search
 */
static inline bool key_found(const uint8_t *const src,
                             uint8_t key,
                             unsigned int length)
{
    for (unsigned int i = 0; i < length; i++) {
        if (src[i] == key) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Map HID key code to system key_code enum
 */
static key_code map_hid_key_to_key_code(uint8_t hid_key)
{
    switch (hid_key) {
        // Letters
        case HID_KEY_A: return key_a;
        case HID_KEY_B: return key_b;
        case HID_KEY_C: return key_c;
        case HID_KEY_D: return key_d;
        case HID_KEY_E: return key_e;
        case HID_KEY_F: return key_f;
        case HID_KEY_G: return key_g;
        case HID_KEY_H: return key_h;
        case HID_KEY_I: return key_i;
        case HID_KEY_J: return key_j;
        case HID_KEY_K: return key_k;
        case HID_KEY_L: return key_l;
        case HID_KEY_M: return key_m;
        case HID_KEY_N: return key_n;
        case HID_KEY_O: return key_o;
        case HID_KEY_P: return key_p;
        case HID_KEY_Q: return key_q;
        case HID_KEY_R: return key_r;
        case HID_KEY_S: return key_s;
        case HID_KEY_T: return key_tee;
        case HID_KEY_U: return key_u;
        case HID_KEY_V: return key_v;
        case HID_KEY_W: return key_w;
        case HID_KEY_X: return key_x;
        case HID_KEY_Y: return key_y;
        case HID_KEY_Z: return key_z;
        
        // Numbers
        case HID_KEY_1: return key_one;
        case HID_KEY_2: return key_two;
        case HID_KEY_3: return key_three;
        case HID_KEY_4: return key_four;
        case HID_KEY_5: return key_five;
        case HID_KEY_6: return key_six;
        case HID_KEY_7: return key_seven;
        case HID_KEY_8: return key_eight;
        case HID_KEY_9: return key_nine;
        case HID_KEY_0: return key_zero;
        
        // Special keys
        case HID_KEY_ENTER: return key_enter;
        case HID_KEY_ESC: return key_esc;
        case HID_KEY_BACKSPACE: return key_backspace;
        case HID_KEY_TAB: return key_tab;
        case HID_KEY_SPACE: return key_space;
        case HID_KEY_MINUS: return key_dash;
        case HID_KEY_EQUAL: return key_equals;
        case HID_KEY_OPEN_BRACKET: return key_openbracket;
        case HID_KEY_CLOSE_BRACKET: return key_closebracket;
        case HID_KEY_BACK_SLASH: return key_backslash;
        case HID_KEY_COLON: return key_colon;
        case HID_KEY_QUOTE: return key_quote;
        case HID_KEY_TILDE: return key_tilde;
        case HID_KEY_LESS: return key_comma;
        case HID_KEY_GREATER: return key_period;
        case HID_KEY_SLASH: return key_slash;
        
        default: return key_none;
    }
}

/**
 * @brief Map HID modifiers to system modifier bitmask
 */
static uint8_t map_hid_modifiers(const hid_keyboard_modifier_bm_t *hid_mod)
{
    uint8_t mods = 0;
    
    if (hid_mod->left_shift || hid_mod->right_shift) {
        mods |= mod_shift;
    }
    if (hid_mod->left_ctrl || hid_mod->right_ctrl) {
        mods |= mod_ctrl;
    }
    if (hid_mod->left_gui || hid_mod->right_gui) {
        mods |= mod_super;
    }
    
    return mods;
}

/**
 * @brief Key event callback - routes keyboard events to terminal and renders to TFT
 */
static void key_event_callback(key_event_t *key_event)
{
    // Only process key press events for terminal input
    if (KEY_STATE_PRESSED != key_event->state) {
        return;
    }
    
    // Map HID key code to system key_code
    key_code sys_key = map_hid_key_to_key_code(key_event->key_code);
    
    if (sys_key == key_none) {
        return;
    }
    
    // Create system key event
    key_event evt;
    evt.key = sys_key;
    evt.modifiers = key_event->modifier;
    
    // Get active terminal
    terminal_state *term = get_active_terminal();
    if (term == NULL || !term->active) {
        ESP_LOGW(TAG, "No active terminal, ignoring key event");
        return;
    }
    
    // Route through terminal input handler
    terminal_handle_key_event(evt);
    
    // Render terminal to TFT screen
    // Note: This is called from USB Host driver task context
    // For better performance, consider queueing render requests instead
    extern void terminal_render_all(void);
    terminal_render_all();
}

/**
 * @brief USB HID Host Keyboard Interface report callback handler
 */
static void hid_host_keyboard_report_callback(const uint8_t *const data, const int length)
{
    hid_keyboard_input_report_boot_t *kb_report = (hid_keyboard_input_report_boot_t *)data;

    if (length < sizeof(hid_keyboard_input_report_boot_t)) {
        return;
    }

    static uint8_t prev_keys[HID_KEYBOARD_KEY_MAX] = { 0 };
    key_event_t key_event;

    for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {
        // Key has been released verification
        if (prev_keys[i] > HID_KEY_ERROR_UNDEFINED &&
                !key_found(kb_report->key, prev_keys[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = prev_keys[i];
            key_event.modifier = 0;
            key_event.state = KEY_STATE_RELEASED;
            key_event_callback(&key_event);
        }

        // Key has been pressed verification
        if (kb_report->key[i] > HID_KEY_ERROR_UNDEFINED &&
                !key_found(prev_keys, kb_report->key[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = kb_report->key[i];
            key_event.modifier = map_hid_modifiers(&kb_report->modifier);
            key_event.state = KEY_STATE_PRESSED;
            key_event_callback(&key_event);
        }
    }

    memcpy(prev_keys, &kb_report->key, HID_KEYBOARD_KEY_MAX);
}

/**
 * @brief USB HID Host interface callback
 */
void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg)
{
    uint8_t data[64] = { 0 };
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        ESP_LOGI(TAG, "HID Keyboard INPUT_REPORT received");
        ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle,
                                                                  data,
                                                                  64,
                                                                  &data_length));

        if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
            if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                hid_host_keyboard_report_callback(data, data_length);
            } else {
                ESP_LOGW(TAG, "Not a keyboard protocol: %d", dev_params.proto);
            }
        } else {
            ESP_LOGW(TAG, "Not a boot interface: %d", dev_params.sub_class);
        }
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HID Keyboard DISCONNECTED");
        ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGI(TAG, "HID Keyboard TRANSFER_ERROR");
        break;
    default:
        break;
    }
}

/**
 * @brief USB HID Host Device event
 */
void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                           const hid_host_driver_event_t event,
                           void *arg)
{
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
        ESP_LOGI(TAG, "HID Keyboard CONNECTED!");
        ESP_LOGI(TAG, "Device params - sub_class: %d, proto: %d",
                 dev_params.sub_class, dev_params.proto);

        const hid_host_device_config_t dev_config = {
            .callback = hid_host_interface_callback,
            .callback_arg = NULL
        };

        if (dev_params.proto == HID_PROTOCOL_KEYBOARD) {
            ESP_LOGI(TAG, "Opening keyboard device...");
            ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
            if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
                ESP_LOGI(TAG, "Setting boot protocol and idle...");
                ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
                ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
            }
            ESP_LOGI(TAG, "Starting keyboard device...");
            ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
            ESP_LOGI(TAG, "Keyboard device started! Ready to receive input.");
        } else {
            ESP_LOGW(TAG, "Connected device is not a keyboard (proto: %d)", dev_params.proto);
        }
        break;
    default:
        break;
    }
}

/**
 * @brief Start USB Host install and handle common USB host library events
 */
static void usb_lib_task(void *arg)
{
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LOWMED,
    };

    ESP_LOGI(TAG, "Installing USB Host...");
    ESP_ERROR_CHECK(usb_host_install(&host_config));
    ESP_LOGI(TAG, "USB Host installed successfully");
    xTaskNotifyGive(arg);

    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
            break;
        }
    }

    ESP_LOGI(TAG, "USB shutdown");
    vTaskDelay(10);
    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskDelete(NULL);
}

/**
 * @brief HID Host Device callback
 */
void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                              const hid_host_driver_event_t event,
                              void *arg)
{
    const app_event_queue_t evt_queue = {
        .event_group = APP_EVENT_HID_HOST,
        .hid_host_device.handle = hid_device_handle,
        .hid_host_device.event = event,
        .hid_host_device.arg = arg
    };

    if (app_event_queue) {
        xQueueSend(app_event_queue, &evt_queue, 0);
    }
}

/**
 * @brief Keyboard pickup event loop task
 * 
 * This task runs the main event loop for handling HID device events.
 * It should be called from a FreeRTOS task.
 */
static void keyboard_pickup_event_task(void *arg)
{
    (void)arg;
    app_event_queue_t evt_queue;

    ESP_LOGI(TAG, "Keyboard pickup event task started");

    // Main event loop - handle HID events
    while (1) {
        if (xQueueReceive(app_event_queue, &evt_queue, portMAX_DELAY)) {
            if (APP_EVENT_HID_HOST == evt_queue.event_group) {
                hid_host_device_event(evt_queue.hid_host_device.handle,
                                      evt_queue.hid_host_device.event,
                                      evt_queue.hid_host_device.arg);
            }
        }
    }
}

/**
 * @brief Initialize keyboard pickup system
 * 
 * This function initializes:
 * - USB Host stack (configured for GPIO 19/20 via sdkconfig)
 * - HID Host driver
 * - Creates event queue and tasks
 * 
 * This function is non-blocking and should be called during boot sequence.
 * It assumes TFT display and terminal system are already initialized.
 * 
 * @return 0 on success, negative on error
 */
int keyboard_pickup_init(void)
{
    BaseType_t task_created;

    ESP_LOGI(TAG, "=== USB Keyboard Pickup Initialization ===");

    // Create USB library task
    task_created = xTaskCreatePinnedToCore(usb_lib_task,
                                           "usb_events",
                                           4096,
                                           xTaskGetCurrentTaskHandle(),
                                           2, NULL, 0);
    if (task_created != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create USB library task!");
        return -1;
    }

    // Wait for notification from usb_lib_task to proceed
    ulTaskNotifyTake(false, pdMS_TO_TICKS(1000));

    // HID host driver configuration
    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_callback,
        .callback_arg = NULL
    };

    ESP_LOGI(TAG, "Installing HID host driver...");
    esp_err_t ret = hid_host_install(&hid_host_driver_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install HID host driver: %s", esp_err_to_name(ret));
        return -1;
    }
    ESP_LOGI(TAG, "HID host driver installed");

    // Create queue
    app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));
    if (app_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue!");
        return -1;
    } else {
        ESP_LOGI(TAG, "Event queue created");
    }

    // Create keyboard pickup event task
    task_created = xTaskCreatePinnedToCore(keyboard_pickup_event_task,
                                           "keyboard_pickup",
                                           4096,
                                           NULL,
                                           3, NULL, 0);
    if (task_created != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create keyboard pickup event task!");
        return -1;
    }

    ESP_LOGI(TAG, "=== Keyboard pickup ready! Connect USB keyboard to GPIO 20 (D+) and GPIO 19 (D-) ===");
    return 0;
}

/**
 * @brief Initialize keyboard pickup with terminal setup
 * 
 * This function sets up the terminal window for keyboard input display.
 * It should be called after terminal system is initialized.
 * 
 * @return 0 on success, negative on error
 */
int keyboard_pickup_setup_terminal(void)
{
    ESP_LOGI(TAG, "Setting up terminal for keyboard input...");
    
    // Ensure terminal system is initialized
    terminal_state *term = get_active_terminal();
    if (term == NULL || !term->active) {
        // Create a new terminal window if none exists
        new_terminal();
        term = get_active_terminal();
    }
    
    if (term != NULL) {
        // Set terminal to fill the screen (480x320 after rotation)
        // Leave some margin for border
        term->x = 10;
        term->y = 10;
        term->width = 460;  // 480 - 20 (margins)
        term->height = 300; // 320 - 20 (margins)
        term->active = 1;
        
        // Initialize terminal with prompt
        terminal_write_string(term, "$ ");
        
        // Render initial terminal state to TFT
        extern void terminal_render_all(void);
        terminal_render_all();
        
        ESP_LOGI(TAG, "Terminal configured for keyboard input, active: %d", term->active);
        return 0;
    } else {
        ESP_LOGE(TAG, "Failed to create/configure terminal!");
        return -1;
    }
}

