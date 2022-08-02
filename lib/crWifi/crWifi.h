//#pragma once

#include "freertos/event_groups.h"

namespace CoReef {
class crWifi {

    public:
        crWifi ( char *ssid, char *passwd );
        ~crWifi ();

    protected:
    private:
};

}