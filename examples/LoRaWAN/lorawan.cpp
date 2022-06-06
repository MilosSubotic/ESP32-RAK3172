#include <esp_log.h>
#include <esp_task_wdt.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include "rak3172.h"

#include "LoRaWAN_Default.h"

#ifdef CONFIG_RAK3172_RESET_USE_HW
    static RAK3172_t _Device = RAK3172_DEFAULT_CONFIG(CONFIG_RAK3172_UART_PORT, CONFIG_RAK3172_UART_RX, CONFIG_RAK3172_UART_TX, CONFIG_RAK3172_UART_BAUD, CONFIG_RAK3172_RESET_PIN, false);
#else
    static RAK3172_t _Device = RAK3172_DEFAULT_CONFIG(CONFIG_RAK3172_UART_PORT, CONFIG_RAK3172_UART_RX, CONFIG_RAK3172_UART_TX, CONFIG_RAK3172_UART_BAUD);
#endif

static StackType_t _applicationStack[8192];

static StaticTask_t _applicationBuffer;

static TaskHandle_t _applicationHandle;

static const char* TAG 							= "main";

static void applicationTask(void* p_Parameter)
{
    bool Status;
    RAK3172_Error_t Error;
    RAK3172_Info_t Info;

    _Device.Info = &Info;

    Error = RAK3172_Init(&_Device);
    if(Error != RAK3172_ERR_OK)
    {
        ESP_LOGE(TAG, "Can not initialize RAK3172! Error: 0x%04X", Error);
    }

    ESP_LOGI(TAG, "Firmware: %s", Info.Firmware.c_str());
    ESP_LOGI(TAG, "Serial number: %s", Info.Serial.c_str());
    ESP_LOGI(TAG, "Current mode: %u", _Device.Mode);

    Error = RAK3172_LoRaWAN_Init(&_Device, 16, 3, RAK_JOIN_OTAA, DEVEUI, APPEUI, APPKEY, 'A', RAK_BAND_EU868, RAK_SUB_BAND_NONE);
    if(Error != RAK3172_ERR_OK)
    {
        ESP_LOGE(TAG, "Can not initialize RAK3172 LoRaWAN! Error: 0x%04X", Error);
    }

    Error = RAK3172_LoRaWAN_isJoined(&_Device, &Status);
    if(Error != RAK3172_ERR_OK)
    {
        ESP_LOGE(TAG, "Error: 0x%04X", Error);
    }

    if(Status == false)
    {
        ESP_LOGI(TAG, "Not joined. Rejoin...");

        Error = RAK3172_LoRaWAN_StartJoin(&_Device, 0, LORAWAN_JOIN_ATTEMPTS, true, LORAWAN_MAX_JOIN_INTERVAL_S, NULL);
        if(Error != RAK3172_ERR_OK)
        {
            ESP_LOGE(TAG, "Can not join network!");
        }
        else
        {
            ESP_LOGI(TAG, "Joined...");

            char Payload[] = {'{', '}'};

            Error = RAK3172_LoRaWAN_Transmit(&_Device, 1, Payload, sizeof(Payload), LORAWAN_TX_TIMEOUT_S, true, NULL);
            if(Error == RAK3172_ERR_INVALID_RESPONSE)
            {
                ESP_LOGE(TAG, "Can not transmit message network!");
            }
            else
            {
                ESP_LOGI(TAG, "Message transmitted...");
            }
        }
    }

    while(true)
    {
        esp_task_wdt_reset();

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void StartApplication(void)
{
    ESP_LOGI(TAG, "Starting application.");

    _applicationHandle = xTaskCreateStatic(applicationTask, "applicationTask", 8192, NULL, 1, _applicationStack, &_applicationBuffer);
    if(_applicationHandle == NULL)
    {
        ESP_LOGE(TAG, "    Unable to create application task!");

        esp_restart();
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "IDF: %s", esp_get_idf_version());

	StartApplication();
}