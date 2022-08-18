#pragma once

namespace CoReef {

class JSONBuilder {
    public:
        JSONBuilder ( unsigned int max_length );
        ~JSONBuilder ();

        void reset ();

        JSONBuilder &add ( const char *s );
        JSONBuilder &add ( const char *s, const char *v );
        JSONBuilder &add ( const char *s, int v );
        JSONBuilder &add_double ( double v, int precision );
        JSONBuilder &step_back ();
        
        const char *str () {
            return message;
        };

        int strlen () {
            return max_length-remaining;
        }

    private:
        void append ( const char *format, ...);
        char *message;
        unsigned int current;
        unsigned int remaining;
        unsigned int max_length;
};

}