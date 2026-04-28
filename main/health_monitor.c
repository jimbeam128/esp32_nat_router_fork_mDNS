#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"

static const char *TAG = "HEALTH";

static void health_task(void *arg)
{
    while (1) {

        size_t free_heap     = esp_get_free_heap_size();
        size_t min_free_heap = esp_get_minimum_free_heap_size();

        size_t internal_free =
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

        size_t largest_block =
            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

        UBaseType_t tasks = uxTaskGetNumberOfTasks();

        ESP_LOGI(TAG,
            "free=%u  min=%u  internal=%u  largest=%u  tasks=%u",
            (unsigned)free_heap,
            (unsigned)min_free_heap,
            (unsigned)internal_free,
            (unsigned)largest_block,
            (unsigned)tasks
        );

        vTaskDelay(pdMS_TO_TICKS(10000));   // alle 10 Sek.
    }
}

void start_health_monitor(void)
{
    xTaskCreate(
        health_task,
        "health_task",
        3072,
        NULL,
        1,
        NULL
    );
}