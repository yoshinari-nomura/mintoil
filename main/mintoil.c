#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

#include <esp_log.h>
#include "sht31.h"

#include "sdkconfig.h"

/* Can run 'make menuconfig' to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO        CONFIG_LAMP_LED_GPIO
#define SAMPLE_PERIOD     (10 * 1000) // ms

extern void initialise_wifi();
extern void push_task(void *pvParameters);

float global_temperature_readings = 0.0;
float global_humidity_readings = 0.0;
bool global_ready_to_push = false;

void blink_task(void *pvParameter)
{
    /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    gpio_pad_select_gpio(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    while(1) {
        /* Blink off (output low) */
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        /* Blink on (output high) */
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void sh31_task(void *pvParameter)
{
  TickType_t last_wake_time = xTaskGetTickCount();

  esp_log_level_set("*", ESP_LOG_INFO);
  sht31_init();

  // Stable readings require a brief period before communication
  vTaskDelay(2000.0 / portTICK_PERIOD_MS);

  while (1) {
    last_wake_time = xTaskGetTickCount();

    if (sht31_readTempHum()) {
      global_humidity_readings = sht31_readHumidity();
      global_temperature_readings = sht31_readTemperature();

      global_ready_to_push = true;

      printf("H, T : %.f, %.f\n",
             global_humidity_readings,
             global_temperature_readings);
    } else {
      printf("sht31_readTempHum : failed\n");
    }

    vTaskDelayUntil(&last_wake_time, SAMPLE_PERIOD / portTICK_PERIOD_MS);
  }
}


void app_main()
{
  /*
    xTaskCreatePinnedToCore(TaskFunction_t pvTaskCode,
                            const char *const pcName,
                            const uint32_t usStackDepth,
                            void *const pvParameters,
                            UBaseType_t uxPriority,
                            TaskHandle_t *const pvCreatedTask,
                            const BaseType_t xCoreID);
  */
  ESP_ERROR_CHECK(nvs_flash_init());
  initialise_wifi();
  xTaskCreatePinnedToCore(&blink_task, "heartbeat_task", 1024, NULL, 5, NULL, 0);
  xTaskCreatePinnedToCore(&sh31_task,  "measure_task",   8192, NULL, 5, NULL, 0);

  vTaskDelay(10000.0 / portTICK_PERIOD_MS); // wait for measure_task becomes stable.
  xTaskCreatePinnedToCore(&push_task,  "push_task",      8192, NULL, 5, NULL, 1);
}
