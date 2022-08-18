#include "crJSONBuilder.h"

#include <stdio.h>
#include <stdarg.h>

namespace CoReef {

JSONBuilder::JSONBuilder ( unsigned int max_length ) {
    message = new char[max_length];
    this->max_length = max_length;
    reset();
}

JSONBuilder::~JSONBuilder () {
    delete [] message;
}
void JSONBuilder::reset () {
    current = 0;
    remaining = max_length;
}

void JSONBuilder::append ( const char *format, ...) {
    va_list args;
    // printf("Enter JSONBuilder::append(%s) with current=%d and remaining=%d\n",format,current,remaining);
    if (remaining == 0)
        return;
    va_start(args,format);
    int len = vsnprintf(message+current,remaining,format,args);
    current += len;
    remaining -= len;
    va_end(args);
    // printf("Leave JSONBuilder::append()\n");
}

JSONBuilder &JSONBuilder::add ( const char *s ) {
    append("%s",s);
    return *this;
}

JSONBuilder &JSONBuilder::add ( const char *key, const char *v ) {
    append("\"%s\":\"%s\",",key,v);
    return *this;
}

JSONBuilder &JSONBuilder::add ( const char *key, int v ) {
    append("\"%s\":%d,",key,v);
    return *this;
}

JSONBuilder &JSONBuilder::add_double ( double v, int precision ) {
    char f[] = "%.xlf,";
    f[2] = '0' + (precision % 10);
    append(f,v);
    return *this;
}

JSONBuilder &JSONBuilder::step_back () {
    if (current == 0)
        return *this;
    current -= 1;
    remaining += 1;
    return *this;
}

}