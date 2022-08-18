#pragma once

#include "crJSONBuilder.h"

namespace CoReef {

template<unsigned int n_channels, unsigned int backlog_size>
class SensorReadings {
    public:
        SensorReadings();
        ~SensorReadings();

        void init();

        void add_reading ( float r[n_channels], double t );

        void create_json ( JSONBuilder &jb, unsigned int poll_intervall );
        double timestamp ( int i );
        int number_of_readings () {
            return total_number_of_readings;
        }
    private:
        float values[n_channels][backlog_size];
        double timestamps[backlog_size];
        int total_number_of_readings;
};

}