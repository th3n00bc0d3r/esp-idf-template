#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event_loop.h"

//Wifi
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

//Button
#include "driver/gpio.h"
#include "freertos/semphr.h"

//Button Inits
#define ESP_INTR_FLAG_DEFAULT 0
#define CONFIG_BUTTON_PIN 15
#define CONFIG_LED_PIN 2

SemaphoreHandle_t xSemaphore = NULL;
bool led_status = false;

//Scan Wifi
void scan_wifi(void) {
    // begin the scan with default parameters (NULL) and delay the code execution until the scan is complete (true).
    esp_wifi_scan_start(NULL, true);

    uint16_t ap_num;
    // get the number of found aps 
    esp_wifi_scan_get_ap_num(&ap_num);
    wifi_ap_record_t ap_records[ap_num];
    // get the list of found aps
    esp_wifi_scan_get_ap_records(
            &ap_num, ap_records
            );
    // print the list
    printf("%d Netze gefunden:\n", ap_num); // %d Networks found
    printf(" Kanal | RSSI | SSID \n");	// Channel | RSSI | SSID
    // walk the list of aps and print their data
    for(int i = 0; i < ap_num; i++)
        printf("%6d |%5d | %-27s \n", 
                ap_records[i].primary, 
                ap_records[i].rssi, 
                (char *)ap_records[i].ssid
                );
    // print a line to increase legibility
    printf("===========================================\n\n");    
}

//interrupt service routine, called when the button is pressed
void IRAM_ATTR button_isr_handler(void* arg) {
    // notify the button task
	xSemaphoreGiveFromISR(xSemaphore, NULL);
}

// task that will react to button clicks
void button_task(void* arg) {
	// infinite loop
	for(;;) {
		// wait for the notification from the ISR
		if(xSemaphoreTake(xSemaphore,portMAX_DELAY) == pdTRUE) {
			printf("Button pressed!\n");
			led_status = !led_status;
			gpio_set_level(CONFIG_LED_PIN, led_status);

            scan_wifi();
		}
	}
}

// Empty infinite task
void loop_task(void *pvParameter) {
    while(1) { 
		vTaskDelay(1000 / portTICK_RATE_MS);		
    }
}

void app_main() {

    /* SCAN WIFI */
	// the tcpip adapter is a requirement for the wifi scan
	tcpip_adapter_init();

	// the esp event loop must be initialized, but doesn't need to do anything
	esp_event_loop_init(NULL, NULL);
	
	// try to init the nvs flash. it is required for the wifi data. This has been modified from the original code presented in c't to increase compatibility
	esp_err_t ret = nvs_flash_init();
	// if the init fails, try to format the nvs and try another init
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	// ESP_ERROR_CHECK is a macro usable for all esp function calls. it will produce a human readable error description.
	ESP_ERROR_CHECK(ret);
	
	// initialize the wifi config with the default data
	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	// initialize the wifi with the config data
	esp_wifi_init(&wifi_init_config);
	// set the mode of the wifi to STA[tion]. this an additional modification from the original article. otherwise the scan will fail sometimes
	esp_wifi_set_mode(WIFI_MODE_STA);
	// start the wifi driver
	esp_wifi_start();

    scan_wifi();

    /* BUTTON */
	// create the binary semaphore
	xSemaphore = xSemaphoreCreateBinary();
	
	// configure button and led pins as GPIO pins
	gpio_pad_select_gpio(CONFIG_BUTTON_PIN);
	gpio_pad_select_gpio(CONFIG_LED_PIN);
	
	// set the correct direction
	gpio_set_direction(CONFIG_BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(CONFIG_LED_PIN, GPIO_MODE_OUTPUT);

	// enable interrupt on falling (1->0) edge for button pin
	gpio_set_intr_type(CONFIG_BUTTON_PIN, GPIO_INTR_NEGEDGE);
	
	// start the task that will handle the button
	xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);
	
	// install ISR service with default configuration
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	
	// attach the interrupt service routine
	gpio_isr_handler_add(CONFIG_BUTTON_PIN, button_isr_handler, NULL);       

	// infinite loop
	xTaskCreate(&loop_task, "loop_task", 2048, NULL, 5, NULL);
}