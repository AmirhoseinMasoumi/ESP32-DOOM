// ESP32 DOOM Port - DeHackEd stubs
// Minimal dehacked support (disabled)

#include <stdio.h>
#include <stdarg.h>
#include "deh_str.h"

// Just pass through strings without replacement
const char *DEH_String(const char *s)
{
    return s;
}

void DEH_printf(const char *fmt, ...)
{
    // Disabled
}

void DEH_fprintf(FILE *fstream, const char *fmt, ...)
{
    // Disabled
}

void DEH_snprintf(char *buffer, size_t len, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, len, fmt, args);
    va_end(args);
}

void DEH_AddStringReplacement(const char *from_text, const char *to_text)
{
    // Disabled
}
