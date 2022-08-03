/*

	DHT22 Sensor using multicast to distribute the measurements

	Using DHT22 code implemented by Ricardo Timmermann
    (https://github.com/gosouth/DHT22)

    Wifi code started with an example found at Github and provided by espressif
    (https://github.com/espressif/esp-idf/blob/master/examples/wifi/getting_started/station/main/station_example_main.c)
    While trying to refactor it into a C++ class, the result did not connect to the AP. The class written by Henk Grobbelaar
    (found at https://embeddedtutorials.com/eps32/esp32-wifi-cpp-esp-idf-station/) is now the working base for the program.

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
    esp_event_loop_create_default();

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    Wifi wifi;
    wifi.SetCredentials(WIFI_SSID,WIFI_PASSWD);
    wifi.Init();

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
        bool connected = false;
        while (!connected) {
            Wifi::state_e wifi_state = wifi.GetState();
            switch (wifi_state) {
                case Wifi::state_e::READY_TO_CONNECT:
                case Wifi::state_e::DISCONNECTED:
                    wifi.Begin();
                    break;
                case Wifi::state_e::CONNECTED:
                    connected = true;
                    break;
                case Wifi::state_e::WAITING_FOR_IP:
                case Wifi::state_e::CONNECTING:
                    vTaskDelay( 100 / portTICK_RATE_MS );
                    break;
                default:
                    ESP_LOGI(TAG,"Wifi state is %s - not yet dealing with it",Wifi::GetStateDescription(wifi_state));
            }
        }
        ms.send(sendbuf,len);

		// -- wait at least 2 sec before reading again ------------
		// The interval of whole process must be beyond 2 seconds !! 
		vTaskDelay( 5000 / portTICK_RATE_MS );
	}

	// xTaskCreate( &DHT_task, "DHT_task", 2048, NULL, 5, NULL );
}

