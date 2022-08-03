#pragma once

namespace CoReef {
class crWifi {

    public:
        crWifi ();
        ~crWifi ();

        bool connect ( char *ssid, char *passwd );

        bool connected ();
    protected:
    private:
};

}