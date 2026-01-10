/*
 * Physical Keyboard Test for ESP IDF
 * 
 * This test uses ESP IDF's USB HID Host driver to test keyboard input.
 * It prints key down/up events with modifier and key codes, similar to
 * the Arduino USBHost test format.
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

static const char *TAG = "keyboard_test";

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
 * @brief Key event callback - prints key down/up events
 */
static void key_event_callback(key_event_t *key_event)
{
    if (KEY_STATE_PRESSED == key_event->state) {
        printf("[KEYBOARD] DOWN  mod=0x%02X key=0x%02X\n", 
               key_event->modifier, key_event->key_code);
    } else {
        printf("[KEYBOARD] UP    mod=0x%02X key=0x%02X\n", 
               key_event->modifier, key_event->key_code);
    }
    fflush(stdout);
}

/**
 * @brief USB HID Host Keyboard Interface report callback handler
 */
static void hid_host_keyboard_report_callback(const uint8_t *const data, const int length)
{
    hid_keyboard_input_report_boot_t *kb_report = (hid_keyboard_input_report_boot_t *)data;

    if (length < sizeof(hid_keyboard_input_report_boot_t)) {
        printf("[KEYBOARD] Report too short: %d bytes (expected %zu)\n",
               length, sizeof(hid_keyboard_input_report_boot_t));
        fflush(stdout);
        return;
    }

    static uint8_t prev_keys[HID_KEYBOARD_KEY_MAX] = { 0 };
    key_event_t key_event;

    // Print raw report data
    printf("[KEYBOARD] Report - Modifiers: 0x%02X, Keys: [",
           kb_report->modifier.val);
    bool has_keys = false;
    for (int j = 0; j < HID_KEYBOARD_KEY_MAX; j++) {
        if (kb_report->key[j] > HID_KEY_ERROR_UNDEFINED) {
            printf("0x%02X ", kb_report->key[j]);
            has_keys = true;
        }
    }
    if (!has_keys) {
        printf("(none)");
    }
    printf("]\n");
    fflush(stdout);

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
            key_event.modifier = kb_report->modifier.val;
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
        ESP_LOGI(TAG, "HID Keyboard INPUT_REPORT received (length: %d)", data_length);
        ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle,
                                                                  data,
                                                                  64,
                                                                  &data_length));
        ESP_LOGI(TAG, "Got report data: %d bytes, sub_class: %d, proto: %d",
                 data_length, dev_params.sub_class, dev_params.proto);

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
    } else {
        ESP_LOGW(TAG, "Event queue not initialized, dropping event");
    }
}

extern "C" void app_main(void)
{
    BaseType_t task_created;
    app_event_queue_t evt_queue;

    ESP_LOGI(TAG, "=== USB HOST keyboard test ===");

    // Create USB library task
    ESP_LOGI(TAG, "Creating USB library task...");
    task_created = xTaskCreatePinnedToCore(usb_lib_task,
                                           "usb_events",
                                           4096,
                                           xTaskGetCurrentTaskHandle(),
                                           2, NULL, 0);
    assert(task_created == pdTRUE);

    // Wait for notification from usb_lib_task to proceed
    ESP_LOGI(TAG, "Waiting for USB Host to initialize...");
    ulTaskNotifyTake(false, 1000);

    // HID host driver configuration
    ESP_LOGI(TAG, "Installing HID host driver...");
    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_callback,
        .callback_arg = NULL
    };

    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));
    ESP_LOGI(TAG, "HID host driver installed");

    // Create queue
    app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));
    if (app_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue!");
    } else {
        ESP_LOGI(TAG, "Event queue created");
    }

    ESP_LOGI(TAG, "=== System ready! Connect a USB keyboard now... ===");

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
