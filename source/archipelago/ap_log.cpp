#include "ap_lib.h"
#include "osd.h"


char* format_prefixed_str(const char *f, const char *p, va_list args)
{
    // Guess a sensible minimal size
    size_t p_len = Bstrlen(p);
    size_t size = nextPow2(Bstrlen(f) + p_len);
    char *buf = nullptr;
    int len;

    do
    {
        buf = (char *)Xrealloc(buf, (size <<= 1));
        Bstrncpy(buf, p, p_len);
        len = p_len + Bvsnprintf(&buf[p_len], size - p_len, f, args);
        // Add trailing newline
        if (len + 1 < size)
        {
            buf[len] = '\n';
            buf[len + 1] = '\0';
            len++;
        }
    } while ((unsigned)len >= size);

    return buf;
}


int AP_Printf(const char *f, ...)
{
    va_list va;
    va_start(va, f);
    char *buf = format_prefixed_str(f, AP_OSDTEXT_NORMAL, va);
    va_end(va);

    OSD_Puts(buf);

    return Bstrlen(f);
}

int AP_Printf(std::string f, ...)
{
    va_list va;
    va_start(va, f);
    char *buf = format_prefixed_str(f.c_str(), AP_OSDTEXT_NORMAL, va);
    va_end(va);

    OSD_Puts(buf);

    return Bstrlen(f.c_str());
}

extern int AP_Debugf(const char *f, ...)
{
#ifdef AP_DEBUG_ON
    va_list va;
    va_start(va, f);
    char *buf = format_prefixed_str(f, AP_OSDTEXT_DEBUG, va);
    va_end(va);

    OSD_Puts(buf);

    return Bstrlen(f);
#else
    return 0;
#endif
}

extern int AP_Debugf(std::string f, ...)
{
#ifdef AP_DEBUG_ON
    va_list va;
    va_start(va, f);
    char *buf = format_prefixed_str(f.c_str(), AP_OSDTEXT_DEBUG, va);
    va_end(va);

    OSD_Puts(buf);

    return Bstrlen(f.c_str());
#else
    return 0;
#endif
}

extern int AP_Errorf(const char *f, ...)

{
    va_list va;
    va_start(va, f);
    char *buf = format_prefixed_str(f, AP_OSDTEXT_ERROR, va);
    va_end(va);

    OSD_Puts(buf);

    return Bstrlen(f);
}

extern int AP_Errorf(std::string f, ...)

{
    va_list va;
    va_start(va, f);
    char *buf = format_prefixed_str(f.c_str(), AP_OSDTEXT_ERROR, va);
    va_end(va);

    OSD_Puts(buf);

    return Bstrlen(f.c_str());
}