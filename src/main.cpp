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
#define POLL_INTERVALL 20    // seconds
#define N_CHANNELS 2
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
#include "crSensorReadings.h"
#include "crSensorReadings.cpp"
#include "crJSONBuilder.h"

using namespace CoReef;

#include "../../keys/ssid.h"
#include "../../keys/coreef_addresses.h"

extern "C" {
    void app_main(void);
    #include "DHT22.h"
}

RTC_NOINIT_ATTR SensorReadings<N_CHANNELS,BACKLOG_SIZE> samples;
RTC_NOINIT_ATTR double deep_sleep_period_ms = ((double) POLL_INTERVALL) * 1000.0;

// ********************************************************************************
// Utility functions
// ********************************************************************************

double get_time_in_millis () {
    struct timeval tv_now;
    gettimeofday(&tv_now,NULL);        
    return ((double) tv_now.tv_sec) * 1000.0 + ((double) tv_now.tv_usec) / 1000.0;
}

void delay_in_millis ( unsigned int d) {
    vTaskDelay( d / portTICK_RATE_MS );
}

double weighted_average ( double v1, double v2, double w ) {
    return w*v1+(1.0-w)*v2;
} 

// ********************************************************************************
// Main
// ********************************************************************************

void app_main() {
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (rtc_get_reset_reason(0) != DEEPSLEEP_RESET) {
        samples.init();
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

	delay_in_millis(1000);
	setDHTgpio(27);

    JSONBuilder jb(MAX_MESSAGE_SIZE);

    while(1) {
        uint32_t free_heap = esp_get_free_heap_size();
        printf("Free heap size is %d bytes\n",free_heap);
        int ret = readDHT();		
		errorHandler(ret);
        float readings[2];
        readings[0] = getTemperature();
        readings[1] = getHumidity();
        samples.add_reading(readings,get_time_in_millis());

        jb.reset();
        jb.add("{")
            .add("device",DEVICE_NAME)
            .add("poll",POLL_INTERVALL)
            .add("\"channels\":[\"Temperature\",\"Humidity\"],");
        samples.create_json(jb,POLL_INTERVALL);
        jb.add("}");
        printf("<%s>\n",jb.str());
        
        wifi.Await_Connection();
        ms.send(jb.str(),jb.strlen());
        delay_in_millis(1000);
        ms.send(jb.str(),jb.strlen());

        if (samples.number_of_readings() > 1) {
            double diff = samples.timestamp(0) - samples.timestamp(1) - ((double) POLL_INTERVALL) * 1000.0;
            deep_sleep_period_ms = weighted_average(deep_sleep_period_ms-diff,deep_sleep_period_ms,0.3);
            printf("Difference in last period was %.3f ms, new sleep intervall is %.3f\n",diff,deep_sleep_period_ms);
        }

        if (deep_sleep_period_ms < 1.0)
            continue;

        if (POLL_INTERVALL < 30) {
            delay_in_millis(deep_sleep_period_ms);
        }
        else {
            esp_sleep_enable_timer_wakeup(((uint64_t) deep_sleep_period_ms) * 1000.0);
            esp_deep_sleep_start();
        }
	}
}

