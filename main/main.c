/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;
const int ECHO_PIN = 13;
const int TRIG_PIN = 12;

QueueHandle_t xQueueTime;
SemaphoreHandle_t xSemaphoreTrigger;
QueueHandle_t xQueueDistance;

void oled1_btn_led_init(void) {
    gpio_init(LED_1_OLED);
    gpio_set_dir(LED_1_OLED, GPIO_OUT);

    gpio_init(LED_2_OLED);
    gpio_set_dir(LED_2_OLED, GPIO_OUT);

    gpio_init(LED_3_OLED);
    gpio_set_dir(LED_3_OLED, GPIO_OUT);

    gpio_init(BTN_1_OLED);
    gpio_set_dir(BTN_1_OLED, GPIO_IN);
    gpio_pull_up(BTN_1_OLED);

    gpio_init(BTN_2_OLED);
    gpio_set_dir(BTN_2_OLED, GPIO_IN);
    gpio_pull_up(BTN_2_OLED);

    gpio_init(BTN_3_OLED);
    gpio_set_dir(BTN_3_OLED, GPIO_IN);
    gpio_pull_up(BTN_3_OLED);
}





void pin_callback(uint gpio, uint32_t events) {
    uint32_t time;

    if (events == 0x4) {
        if (gpio == ECHO_PIN) {
            time= to_us_since_boot(get_absolute_time());
        }
    }
    if (events == 0x8) {
        if (gpio == ECHO_PIN) {
            time = to_us_since_boot(get_absolute_time());
        }
    }
    xQueueSendFromISR(xQueueTime, &time, 0);
}



void echo_task(void *p) {
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pin_callback);

    int times_read = 0;
    uint32_t trigger_times[2];
    
    while (1) {
        if (xQueueReceive(xQueueTime, &trigger_times[times_read], pdMS_TO_TICKS(1000)) == pdTRUE) {
            double distance = 0.0;
            times_read ++;
            if(times_read == 2) {
                distance = (trigger_times[1] - trigger_times[0]) * 0.01715; 
                xQueueSend(xQueueDistance, &distance, 0);
                times_read = 0;
        }
    }
}
}

void trigger_task(void *p) {
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);

    while (1) {
        gpio_put(TRIG_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_put(TRIG_PIN, 0);
        xSemaphoreGive(xSemaphoreTrigger);
        vTaskDelay(pdMS_TO_TICKS(990));
    }
}

void oled_task(void *p){
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    char distanceStr[12];
    double distance;
    const int maxWidth = 128;
    
    while(1){
        if(xSemaphoreTake(xSemaphoreTrigger , pdMS_TO_TICKS(100)) == pdTRUE){
            vTaskDelay(pdMS_TO_TICKS(100));
            if(xQueueReceive(xQueueDistance, &distance, pdMS_TO_TICKS(1000)) == pdTRUE){
                    gfx_clear_buffer(&disp);
                    snprintf(distanceStr, sizeof(distanceStr), "Dist: %d", (int) distance);
                    gfx_draw_string(&disp, 0, 0, 2, distanceStr);
                    int barLength = (int)((distance / 300.0) * maxWidth);
                    if (barLength > maxWidth) {
                        barLength = maxWidth; // Ensure the bar does not exceed the maximum width
                    }
                    gfx_draw_line(&disp, 0, 31, barLength, 31); // Draw the bar at the bottom of the OLED
                    gfx_show(&disp);
                    vTaskDelay(pdMS_TO_TICKS(150));
            }
        }
    }
}

int main() {
    stdio_init_all();
    
    xSemaphoreTrigger = xSemaphoreCreateBinary();

    xQueueTime = xQueueCreate(32, sizeof(uint32_t));
    xQueueDistance = xQueueCreate(32, sizeof(double));
    

    
    
    xTaskCreate(oled_task, "Oled Task", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo Task", 4095, NULL, 1, NULL);
    xTaskCreate(trigger_task, "Trigger Task", 4095, NULL, 1, NULL);
    vTaskStartScheduler();
    

    while (true) 
    ;
        
}











