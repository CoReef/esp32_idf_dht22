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

#define DEVICE_NAME "Nemo-01"
#define POLL_INTERVALL 60    // seconds
#define BACKLOG_SIZE 12
#define MAX_MESSAGE_SIZE 1024

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_timer.h"

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
struct Samples {
    unsigned int seq_number;
    int64_t current;
    float current_temperature;
    float backlog_t[BACKLOG_SIZE];
    float current_humidity;
    float backlog_h[BACKLOG_SIZE];
};

static Samples samples;

// ********************************************************************************
// Main
// ********************************************************************************

void DHT_task(void *pvParameter)
{
}

void app_main() {
    int64_t start_time = esp_timer_get_time();

    for (int i=0; i<BACKLOG_SIZE; i++) {
        samples.backlog_t[i] = -100.0;
        samples.backlog_h[i] = -100.0;
    }
    samples.current_temperature = -100.0;
    samples.current_humidity = -100.0;
    samples.current = 0;
    samples.seq_number = 0;

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

    char message[MAX_MESSAGE_SIZE];
    while(1) {
		int ret = readDHT();		
		errorHandler(ret);

        samples.current = (esp_timer_get_time() - start_time) / 1000;
        for (int i=BACKLOG_SIZE-1;i>0; i--) {
            samples.backlog_t[i] = samples.backlog_t[i-1];
            samples.backlog_h[i] = samples.backlog_h[i-1];
        }
        samples.backlog_t[0] = samples.current_temperature;
        samples.backlog_h[0] = samples.current_humidity;
        samples.current_temperature = getTemperature();
        samples.current_humidity = getHumidity();
        samples.seq_number++;

        char *m_ptr = message;
        int m_len_remain = MAX_MESSAGE_SIZE;
        
        const char fmt_head[] = " { \"device\":\"%s\", \"sequence\":%d, \"current\":%lld, \"current_t\":%.1f, \"current_h\":%.1f, \"backlog_t\":[";
        int len = snprintf(m_ptr,m_len_remain,fmt_head,DEVICE_NAME,samples.seq_number,samples.current,samples.current_temperature,samples.current_humidity);
        const char fmt_v[] = "%.1f,";
        for (int i=0;i<BACKLOG_SIZE;i++) {
            m_ptr += len;
            m_len_remain -= len;
            len = snprintf(m_ptr,m_len_remain,fmt_v,samples.backlog_t[i]);
        }
        m_ptr += len-1;
        m_len_remain -= len-1;
        const char fmt_m[] = "], \"backlog_h\":[";
        len = snprintf(m_ptr,m_len_remain,fmt_m);
        for (int i=0;i<BACKLOG_SIZE;i++) {
            m_ptr += len;
            m_len_remain -= len;
            len = snprintf(m_ptr,m_len_remain,fmt_v,samples.backlog_h[i]);
        }
        m_ptr += len-1;
        m_len_remain -= len-1;
        const char fmt_tail[] = "] }";
        len = snprintf(m_ptr,m_len_remain,fmt_tail);
        m_ptr += len;
        m_len_remain -= len;


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
        printf("%s\n",message);
        ms.send(message,MAX_MESSAGE_SIZE-m_len_remain);

		// -- wait at least 2 sec before reading again ------------
		// The interval of whole process must be beyond 2 seconds !! 
		vTaskDelay( POLL_INTERVALL * 1000 / portTICK_RATE_MS );
	}

	// xTaskCreate( &DHT_task, "DHT_task", 2048, NULL, 5, NULL );
}

