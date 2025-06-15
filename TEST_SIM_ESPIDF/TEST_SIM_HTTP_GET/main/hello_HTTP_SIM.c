/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 *
 * File:    hello_HTTP_SIM.c
 * Author:  Alushi
 * Date:    2025-06-15
 *
 * Description:
 *   Example demonstrating how to perform an HTTPS GET using a SIM7600G 4G
 *   module via AT commands over UART on an ESP32-S3. The flow includes:
 *     1) Checking network registration and attaching to PDP context
 *     2) Initializing the SIM HTTP service with SSL enabled
 *     3) Issuing AT+HTTPACTION to trigger the GET request
 *     4) Parsing the +HTTPACTION URC for HTTP status and length
 *     5) Reading back the payload with AT+HTTPREAD
 *     6) Cleaning up and disabling the HTTP service
 *
 * Note:
 *   - Ensure the SIM module has network connectivity before invoking the
 *     HTTP commands.
 *   - Adjust PDP parameters (APN, username, password) as needed.
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "app";

// ==== UART2 (SIM-AT) setup ====
#define SIM_UART_NUM      UART_NUM_2
#define SIM_UART_TX_GPIO  18
#define SIM_UART_RX_GPIO  17
#define SIM_UART_BAUD     115200
#define UART_BUF_SIZE     1024

// Helper: send one AT command, read up to timeout_ms ms
static esp_err_t send_at(const char *cmd, char *resp_buf, size_t buf_size, TickType_t timeout_ms)
{
    // clear any old data
    uart_flush(SIM_UART_NUM);

    // write "AT...<CR><LF>"
    uart_write_bytes(SIM_UART_NUM, cmd, strlen(cmd));
    uart_write_bytes(SIM_UART_NUM, "\r\n", 2);

    // read the reply (if any)
    int len = uart_read_bytes(SIM_UART_NUM, (uint8_t*)resp_buf, buf_size - 1, timeout_ms);
    if (len < 0) {
        ESP_LOGE(TAG, "UART read failed");
        return ESP_FAIL;
    }
    resp_buf[len] = '\0';
    ESP_LOGI(TAG, "AT> %s\n< %s", cmd, resp_buf);
    return ESP_OK;
}

// Helper: parse "+HTTPACTION: 0,200,NNN"
static int parse_http_length(const char *resp)
{
    const char *p = strstr(resp, "+HTTPACTION:");
    if (!p) return 0;
    // skip to second comma
    p = strchr(p, ',');
    if (!p) return 0;
    p = strchr(p + 1, ',');
    if (!p) return 0;
    return atoi(p + 1);
}

// ====  SIM-HTTP flow in one function ====
static void sim_http_get_sample(void)
{
    char resp[UART_BUF_SIZE];

    // 1) PDP bring-up (EE network)
    send_at("AT+CGATT?",    resp, sizeof(resp), pdMS_TO_TICKS(500));
    send_at("AT+CGDCONT=1,\"IP\",\"everywhere\"", resp, sizeof(resp), pdMS_TO_TICKS(500));
    send_at("AT+CGAUTH=1,1,\"eesecure\",\"secure\"", resp, sizeof(resp), pdMS_TO_TICKS(500));
    send_at("AT+CGACT=1,1", resp, sizeof(resp), pdMS_TO_TICKS(2000));
    send_at("AT+CGPADDR=1", resp, sizeof(resp), pdMS_TO_TICKS(500));

    // 2) HTTPS GET
    send_at("AT+HTTPTERM",  resp, sizeof(resp), pdMS_TO_TICKS(500));
    send_at("AT+HTTPINIT",  resp, sizeof(resp), pdMS_TO_TICKS(500));
    send_at("AT+HTTPSSL=1", resp, sizeof(resp), pdMS_TO_TICKS(500));
    send_at("AT+HTTPPARA=\"URL\",\"https://alusys.io/test/sample.bin\"", resp, sizeof(resp), pdMS_TO_TICKS(500));
    send_at("AT+HTTPPARA=\"READMODE\",1",       resp, sizeof(resp), pdMS_TO_TICKS(500));

    // fire it and get length
    send_at("AT+HTTPACTION=0", resp, sizeof(resp), pdMS_TO_TICKS(10000));
    int len = parse_http_length(resp);
    if (len > 0) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "AT+HTTPREAD=0,%d", len);
        send_at(cmd, resp, sizeof(resp), pdMS_TO_TICKS(10000));
    }

    // tear down
    send_at("AT+HTTPTERM", resp, sizeof(resp), pdMS_TO_TICKS(500));
    send_at("AT+HTTPSSL=0", resp, sizeof(resp), pdMS_TO_TICKS(500));
}

void app_main(void)
{
    // --- existing chip-info code ---
    printf("Hello world!\n");
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), ...\n",
           CONFIG_IDF_TARGET, chip_info.cores);
    esp_flash_get_size(NULL, &flash_size);
    printf("%" PRIu32 "MB flash\n", flash_size / (1024*1024));
    printf("Min free heap: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    // --- initialize UART2 for SIM AT commands ---
    const uart_config_t uart_cfg = {
        .baud_rate = SIM_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(SIM_UART_NUM, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(SIM_UART_NUM, SIM_UART_TX_GPIO, SIM_UART_RX_GPIO,
                                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(SIM_UART_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));

    // --- now run SIM-HTTP GET ---
    sim_http_get_sample();

    // Optional: restart or enter low-power here
    printf("Done with SIM HTTP!\n");
}
