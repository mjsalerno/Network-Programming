#ifndef DEBUG_H
#define DEBUG_H

#define _RED     "\x1b[31m"
#define _GREEN   "\x1b[32m"
#define _YELLOW  "\x1b[33m"
#define _BLUE    "\x1b[34m"
#define _RESET   "\x1b[0m"

#ifdef DEBUG
# define _DEBUG(fmt, args...) printf("DEBUG: %s:%s:%d: "fmt, __FILE__, __FUNCTION__, __LINE__, args)
#else
# define _DEBUG(fmt, args...)
#endif

#define _ERROR(fmt, args...) fprintf(stderr, _RED "ERROR: %s:%s:%d: "fmt _RESET, __FILE__, __FUNCTION__, __LINE__, args)

#define _NOTE(fmt, args...) fprintf(stderr, _YELLOW "NOTICE: %s:%s:%d: "fmt _RESET, __FILE__, __FUNCTION__, __LINE__, args)

#define _INFO(fmt, args...) fprintf(stderr, _BLUE "INFO: %s:%s:%d: "fmt _RESET, __FILE__, __FUNCTION__, __LINE__, args)

#define _SPEC(fmt, args...) fprintf(stderr, _GREEN "SPEC: %s:%s:%d: "fmt _RESET, __FILE__, __FUNCTION__, __LINE__, args)

#endif
