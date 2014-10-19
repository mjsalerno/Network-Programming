#ifndef DEBUG_H
    #define DEBUG_H
#ifdef DEBUG
    #define _DEBUG(fmt, args...) printf("DEBUG: %s:%s:%d: "fmt, __FILE__, __FUNCTION__, __LINE__, args)
#else
    #define _DEBUG(fmt, args...)
#endif
#endif
