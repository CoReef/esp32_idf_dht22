#include <limits>

#include <stdio.h>

#include "crSensorReadings.h"

namespace CoReef {

template<unsigned int n_channels, unsigned int backlog_size> SensorReadings<n_channels,backlog_size>::SensorReadings() {
}

template<unsigned int n_channels, unsigned int backlog_size> SensorReadings<n_channels,backlog_size>::~SensorReadings() {
}

template<unsigned int n_channels, unsigned int backlog_size> void SensorReadings<n_channels,backlog_size>::add_reading ( float rs[n_channels], double t ) {
    for (int c=0; c<n_channels; c++) {
        for (int i=backlog_size-1; i>0; i--) values[c][i] = values[c][i-1];
        values[c][0] = rs[c];
    }
    for (int i=backlog_size-1; i>0; i--) timestamps[i] = timestamps[i-1];
    timestamps[0] = t;
    total_number_of_readings++;
}

template<unsigned int n_channels, unsigned int backlog_size> void SensorReadings<n_channels,backlog_size>::create_json ( JSONBuilder &jb, unsigned int poll_intervall ) {
    for (int c=0; c<n_channels; c++) {
        char channel_name[16];
        snprintf(channel_name,sizeof(channel_name),"\"channel_%d\":[",c);
        jb.add(channel_name);
        for (int i=0; i<backlog_size; i++)
            jb.add_double(values[c][i],1);
        jb.step_back().add("],");
    }
    jb.add("\"timebase\":");
    jb.add_double(timestamps[0],1);
    jb.add("\"t_deltas\":[");
    for (int i=1; i<backlog_size; i++)
        jb.add_double(timestamps[i]-timestamps[i-1]+((double) poll_intervall * 1000.0),1);
    jb.step_back().add("],");
    jb.add("sequence",total_number_of_readings).step_back();
}

template<unsigned int n_channels, unsigned int backlog_size> double SensorReadings<n_channels,backlog_size>::timestamp( int i ) {
    return timestamps[i];
}

template<unsigned int n_channels, unsigned int backlog_size> void SensorReadings<n_channels,backlog_size>::init() {
    for (int c=0; c<n_channels; c++) {
        for (int i=0; i<backlog_size; i++) {
            values[c][i] = -100.0;
        }
    }
    for (int i=0; i<backlog_size; i++) {
        timestamps[i] = 0.0;
    }
    total_number_of_readings = 0;
}

}