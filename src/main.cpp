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
#define POLL_INTERVALL 300    // seconds
#define BACKLOG_SIZE 12
#define MAX_MESSAGE_SIZE 1024

#define LOG_LOCAL_LEVEL ESP_LOG_WARN
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp32/rom/rtc.h"

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
    float backlog_t[BACKLOG_SIZE];
    float backlog_h[BACKLOG_SIZE];
    double t[BACKLOG_SIZE];
};

RTC_NOINIT_ATTR Samples samples;
RTC_NOINIT_ATTR double deep_sleep_period_ms = ((double) POLL_INTERVALL) * 1000.0;

// ********************************************************************************
// Main
// ********************************************************************************

void DHT_task(void *pvParameter)
{
}

double get_time_in_millis () {
    struct timeval tv_now;
    gettimeofday(&tv_now,NULL);        
    return ((double) tv_now.tv_sec) * 1000.0 + ((double) tv_now.tv_usec) / 1000.0;
}

void app_main() {
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (rtc_get_reset_reason(0) != DEEPSLEEP_RESET) {
        for (int i=0; i<BACKLOG_SIZE; i++) {
            samples.backlog_t[i] = -100.0;
            samples.backlog_h[i] = -100.0;
            samples.t[i] = -1.0;
        }
        samples.seq_number = 0;
        deep_sleep_period_ms = ((double) POLL_INTERVALL) * 1000.0;
    }
    ESP_LOGI(TAG, "data structure samples has <%d> bytes",sizeof(samples));

    esp_event_loop_create_default();

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
        for (int i=BACKLOG_SIZE-1;i>0; i--) {
            samples.backlog_t[i] = samples.backlog_t[i-1];
            samples.backlog_h[i] = samples.backlog_h[i-1];
            samples.t[i] = samples.t[i-1];
        }
		int ret = readDHT();		
		errorHandler(ret);
        samples.backlog_t[0] = getTemperature();
        samples.backlog_h[0] = getHumidity();
        samples.t[0] = get_time_in_millis();

        samples.seq_number++;

        char *m_ptr = message;
        int m_len_remain = MAX_MESSAGE_SIZE;
        
        const char fmt_head[] = "{\"device\":\"%s\",\"sequence\":%d,\"poll\":%d,\"channels\":[\"temperature\",\"humidity\"],\"channel_0\":[";
        int len = snprintf(m_ptr,m_len_remain,fmt_head,DEVICE_NAME,samples.seq_number,POLL_INTERVALL);
        const char fmt_v[] = "%.1f,";
        for (int i=0;i<BACKLOG_SIZE;i++) {
            m_ptr += len;
            m_len_remain -= len;
            len = snprintf(m_ptr,m_len_remain,fmt_v,samples.backlog_t[i]);
        }
        m_ptr += len-1;
        m_len_remain -= len-1;
        const char fmt_m[] = "],\"channel_1\":[";
        len = snprintf(m_ptr,m_len_remain,fmt_m);
        for (int i=0;i<BACKLOG_SIZE;i++) {
            m_ptr += len;
            m_len_remain -= len;
            len = snprintf(m_ptr,m_len_remain,fmt_v,samples.backlog_h[i]);
        }
        m_ptr += len-1;
        m_len_remain -= len-1;
        const char fmt_m2[] = "],\"p_delta_ms\":[";
        len = snprintf(m_ptr,m_len_remain,fmt_m2);
        for (int i=0;i<BACKLOG_SIZE-1;i++) {
            m_ptr += len;
            m_len_remain -= len;
            double diff = -1;
            if ((samples.t[i]>0) && (samples.t[i+1]>0))
                diff = samples.t[i] - samples.t[i+1] - ((double) POLL_INTERVALL) * 1000.0;
            len = snprintf(m_ptr,m_len_remain,fmt_v,diff);
        }
        m_ptr += len-1;
        m_len_remain -= len-1;
        const char fmt_tail[] = "]}";
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
        vTaskDelay( 1000 / portTICK_RATE_MS );
        ms.send(message,MAX_MESSAGE_SIZE-m_len_remain);

		// -- wait at least 2 sec before reading again ------------
		// The interval of whole process must be beyond 2 seconds !! 
		// vTaskDelay( POLL_INTERVALL * 1000 / portTICK_RATE_MS );

        if (samples.t[1] > 0) {
            double diff = samples.t[0] - samples.t[1] - ((double) POLL_INTERVALL) * 1000.0;
            double k = 3.0;
            double mk = 10.0;
            double new_deep = (k * (deep_sleep_period_ms-diff) + (mk-k) * deep_sleep_period_ms)/mk;
            deep_sleep_period_ms = new_deep;
            printf("Difference in last period was %.3f ms, new sleep intervall is %.3f\n",diff,deep_sleep_period_ms);
        }
        esp_sleep_enable_timer_wakeup(((uint64_t) deep_sleep_period_ms) * 1000.0);
        esp_deep_sleep_start();
	}

	// xTaskCreate( &DHT_task, "DHT_task", 2048, NULL, 5, NULL );
}

