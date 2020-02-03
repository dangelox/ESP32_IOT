/* MQTT Mutual Authentication Example

 This example code is in the Public Domain (or CC0 licensed, at your option.)

 Unless required by applicable law or agreed to in writing, this
 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied.
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "tcpip_adapter.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "driver/gpio.h"

#define RED_GPIO 	12
#define YELLOW_GPIO 27
#define BLUE_GPIO 	14

static const char *TAG = "MQTTS_EXAMPLE";

extern const uint8_t client_cert_pem_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_pem_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_pem_end[] asm("_binary_client_key_end");

static void parse_mqtt_data(int len, char* data) {
	// create string that holds the mqtt payload
	char payload[len + 1];

	// convert payload into string.
	// *note: payload do not have NULL charactor at the end
	for (int i = 0; i < len; ++i) {
		payload[i] = *data;
		data++;
	}

	// add null terminator
	payload[len] = '\0';

	printf("MQTT payload: %s\n", payload);

	// Extract the first token
	char * token = strtok(payload, ":");

	int set_led_state = 0;

	// set the LED
	char led[strlen(token)];
	strcpy(led, token);

	token = strtok(NULL, " ");
	char led_state[strlen(token)];
	strcpy(led_state, token);

	printf("%s LED is %s!\n", led, led_state);

	if (strcmp(led_state, "on") == 0) {
		set_led_state = 1;
	} else if (strcmp(led_state, "off") == 0) {
		set_led_state = 0;
	}

	if (strcmp(led, "red") == 0) {
		// red led
		printf("Red LED selected!\n");
		gpio_set_level(RED_GPIO, set_led_state);
	} else if (strcmp(led, "yellow") == 0) {
		// yellow led
		printf("Yellow LED selected!\n");
		gpio_set_level(YELLOW_GPIO, set_led_state);
	} else if (strcmp(led, "blue") == 0) {
		// blue led
		printf("Blue LED selected!\n");
		gpio_set_level(BLUE_GPIO, set_led_state);
	} else /* default: */
	{
		printf("Wrong command!\n");
	}
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event) {
	esp_mqtt_client_handle_t client = event->client;
	int msg_id;
// your_context_t *context = event->context;
	switch (event->event_id) {
	case MQTT_EVENT_CONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
		msg_id = esp_mqtt_client_subscribe(client, "/device/qos0/esp32_01", 0);
		ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

		msg_id = esp_mqtt_client_subscribe(client, "/device/qos1/esp32_01", 1);
		ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

		msg_id = esp_mqtt_client_unsubscribe(client, "/device/qos1/esp32_01");
		ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
		break;
	case MQTT_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
		break;

	case MQTT_EVENT_SUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
		msg_id = esp_mqtt_client_publish(client, "/device/qos0/esp32_01",
				"data:data", 0, 0, 0);
		ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
		break;
	case MQTT_EVENT_UNSUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_PUBLISHED:
		ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_DATA:
		ESP_LOGI(TAG, "MQTT_EVENT_DATA");
		printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
		printf("DATA=%.*s\r\n", event->data_len, event->data);
		parse_mqtt_data(event->data_len, event->data);
		break;
	case MQTT_EVENT_ERROR:
		ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
		break;
	default:
		ESP_LOGI(TAG, "Other event id:%d", event->event_id);
		break;
	}
	return ESP_OK;
}

static void mqtt_app_start(void) {
	const esp_mqtt_client_config_t mqtt_cfg = { .uri =
			"mqtts://64.126.88.35:8883", .event_handle = mqtt_event_handler,
			.client_cert_pem = (const char *) client_cert_pem_start,
			.client_key_pem = (const char *) client_key_pem_start, };

	ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
	esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_start(client);
}

void app_main() {
	ESP_LOGI(TAG, "[APP] Startup..");
	ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
	esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

	ESP_ERROR_CHECK(nvs_flash_init());
	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	/* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
	 * Read "Establishing Wi-Fi or Ethernet Connection" section in
	 * examples/protocols/README.md for more information about this function.
	 */
	ESP_ERROR_CHECK(example_connect());

	/* Configure the IOMUX register for pad BLINK_GPIO (some pads are
	 muxed to GPIO on reset already, but some default to other
	 functions and need to be switched to GPIO. Consult the
	 Technical Reference for a list of pads and their default
	 functions.)
	 */
	gpio_pad_select_gpio(RED_GPIO);
	gpio_pad_select_gpio(YELLOW_GPIO);
	gpio_pad_select_gpio(BLUE_GPIO);
	/* Set the GPIO as a push/pull output */
	gpio_set_direction(RED_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_direction(YELLOW_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_direction(BLUE_GPIO, GPIO_MODE_OUTPUT);

	mqtt_app_start();
}
