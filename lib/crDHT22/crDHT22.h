#pragma once

#include "driver/gpio.h"

namespace CoReef {

    class DHT22 {
        public:
            DHT22 ( gpio_num_t dht22_pin );
            ~DHT22 ();

            float get_temperature();
            float get_humidity();
        private:
            int getSignalLevel ( int usTimeOut, bool state );
            int readDHT22 ();
            double get_time_in_millis ();
            void delay_in_millis ( unsigned int d);
            void next_reading();

            gpio_num_t dht22_pin;
            float temperature;
            float humidity;
            double last_reading;
    };
}