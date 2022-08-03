/*

	DHT22 Sensor using multicast to distribute the measurements

	Using DHT22 code implemented by Ricardo Timmermann
    (https://github.com/gosouth/DHT22)

    Wifi code is based on the example at Github provided by espressif
    (https://github.com/espressif/esp-idf/blob/master/examples/wifi/getting_started/station/main/station_example_main.c)

    UDP multicast code is based on the example at Github provided by espressif
    https://github.com/espressif/esp-idf/blob/master/examples/protocols/sockets/udp_multicast/main/udp_multicast_example_main.c
*/


#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "esp32_dht22";

#include "crWifi.h"
#include "crUDP.h"

using namespace CoReef;

#include "../../keys/ssid.h"
#include "../../keys/coreef_addresses.h"

extern "C" {
    void app_main(void);
    #include "DHT22.h"
}

// ********************************************************************************
// Main
// ********************************************************************************

void DHT_task(void *pvParameter)
{
}

void app_main()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    crWifi wifi;
    bool connected = false;
    while (!connected) {
        ESP_LOGI(TAG, "Connecting to SSID <%s>",const_cast<char *>(WIFI_SSID));  
        connected = wifi.connect(const_cast<char *>(WIFI_SSID),const_cast<char *>(WIFI_PASSWD));
    }

    MulticastSocket ms(const_cast<char *>(COREEF_IPV4_MULTICAST_ADDR),COREEF_IPV4_MULTICAST_PORT);
    if (!ms.valid()) {
        ESP_LOGE(TAG, "Failed to create IPv4 multicast socket");
        return;
    }


	vTaskDelay( 1000 / portTICK_RATE_MS );
	setDHTgpio(27);

    int l = 0;
	while(1) {
		int ret = readDHT();		
		errorHandler(ret);

		printf( "%4d: Temperature %.1f C, Humidity %.1f\n", l++, getTemperature(), getHumidity() );
		
        const char sendfmt[] = "{ \"temperature\" : %.1f, \"humidity\" : %.1f }";
        char sendbuf[48];
        int len = snprintf(sendbuf, sizeof(sendbuf), sendfmt, getTemperature(), getHumidity());
        if (len > sizeof(sendbuf)) {
            ESP_LOGE(TAG, "Overflowed multicast sendfmt buffer!!");
            break;
        }
        ms.send(sendbuf,len);

		// -- wait at least 2 sec before reading again ------------
		// The interval of whole process must be beyond 2 seconds !! 
		vTaskDelay( 5000 / portTICK_RATE_MS );
	}

	// xTaskCreate( &DHT_task, "DHT_task", 2048, NULL, 5, NULL );
}

