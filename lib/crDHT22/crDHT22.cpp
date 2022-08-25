
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "sys/time.h"

#include "crDHT22.h"

static const char* TAG = "DHT22";

namespace CoReef {

DHT22::DHT22 ( gpio_num_t dht22_pin ) {
    this->dht22_pin = dht22_pin;
    last_reading = get_time_in_millis() - 3000.0; // Last reading is more than 2 seconds away
}

float DHT22::get_temperature () {
    next_reading();
    return temperature;
}

float DHT22::get_humidity () {
    next_reading();
    return humidity;
}

int DHT22::getSignalLevel( int usTimeOut, bool state ) {
	int uSec = 0;
	while( gpio_get_level(dht22_pin)==state ) {
		if( uSec > usTimeOut ) 
			return -1;		
		++uSec;
		ets_delay_us(1);		// uSec delay
	}	
	return uSec;
}

#define MAXdhtData 5	// to complete 40 = 5*8 Bits
#define DHT_OK 0
#define DHT_CHECKSUM_ERROR -1
#define DHT_TIMEOUT_ERROR -2

int DHT22::readDHT22() {
    int uSec = 0;
    uint8_t dhtData[MAXdhtData];
    uint8_t byteInx = 0;
    uint8_t bitInx = 7;
	for (int k = 0; k<MAXdhtData; k++) 
		dhtData[k] = 0;

	gpio_set_direction(dht22_pin,GPIO_MODE_OUTPUT);
	// pull down for 3 ms for a smooth and nice wake up 
	gpio_set_level(dht22_pin,0);
	ets_delay_us(3000);			

	// pull up for 25 us for a gentile asking for data
	gpio_set_level(dht22_pin,1);
	ets_delay_us(25);

	gpio_set_direction(dht22_pin,GPIO_MODE_INPUT);  
	// DHT will keep the line low for 80 us and then high for 80us
    uSec = getSignalLevel(85,0);
    //	ESP_LOGI( TAG, "Response = %d", uSec );
	if (uSec<0)
        return DHT_TIMEOUT_ERROR; 
	// 80us up
	uSec = getSignalLevel(85,1);
    //	ESP_LOGI( TAG, "Response = %d", uSec );
	if(uSec<0)
        return DHT_TIMEOUT_ERROR;

	// No errors, read the 40 data bits
  
	for( int k = 0; k < 40; k++ ) {
		// starts new data transmission with >50us low signal
		uSec = getSignalLevel(56,0);
		if(uSec<0) return DHT_TIMEOUT_ERROR;
		// check to see if after >70us rx data is a 0 or a 1
		uSec = getSignalLevel(75,1);
		if(uSec<0) return DHT_TIMEOUT_ERROR;
		// Add the current read to the output data since all dhtData array where set to 0 at the start, only look for "1" (>28us us)
		if (uSec > 40) {
			dhtData[ byteInx ] |= (1 << bitInx);
		}	
		// index to next byte
		if (bitInx == 0) { bitInx = 7; ++byteInx; } else bitInx--;
	}
	humidity = dhtData[0];
	humidity *= 0x100;					// >> 8
	humidity += dhtData[1];
	humidity /= 10;						// get the decimal
	temperature = dhtData[2] & 0x7F;	
	temperature *= 0x100;				// >> 8
	temperature += dhtData[3];
	temperature /= 10;
	if( dhtData[2] & 0x80 ) 			// negative temp, brrr it's freezing
		temperature *= -1;

	// Verify if checksum is the sum of Data 8 bits masked out 0xFF
	
	if (dhtData[4] == ((dhtData[0] + dhtData[1] + dhtData[2] + dhtData[3]) & 0xFF)) 
		return DHT_OK;
	else 
		return DHT_CHECKSUM_ERROR;
}

double DHT22::get_time_in_millis () {
    struct timeval tv_now;
    gettimeofday(&tv_now,NULL);        
    return ((double) tv_now.tv_sec) * 1000.0 + ((double) tv_now.tv_usec) / 1000.0;
}

void DHT22::next_reading () {
    if (get_time_in_millis()-last_reading <= 2000.0)
        return;
    float save_t = temperature;
    float save_h = humidity;
    int ret = readDHT22();
    if (ret != DHT_OK) {
        temperature = -save_t;
        humidity = -save_h;
        return;
    }
    last_reading = get_time_in_millis();
}

}


