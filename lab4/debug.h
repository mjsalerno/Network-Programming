#ifndef DEBUG_H
#define DEBUG_H

#define _RED     "\x1b[31m"
#define _GREEN   "\x1b[32m"
#define _YELLOW  "\x1b[33m"
#define _BLUE    "\x1b[34m"
#define _PURPLE  "\x1b[35m"
#define _RESET   "\x1b[0m"

#ifdef DEBUG
# define _DEBUG(fmt, args...) printf("DEBUG: %s:%s:%d: "fmt, __FILE__, __FUNCTION__, __LINE__, args)
# define _DEBUGY(fmt, args...) printf(_YELLOW "DEBUG: %s:%s:%d: "fmt _RESET, __FILE__, __FUNCTION__, __LINE__, args)
#else
# define _DEBUG(fmt, args...)
# define _DEBUGY(fmt, args...)
#endif /* DEBUG */

#ifdef HANDIN

#define _ERROR(fmt, args...) fprintf(stderr,   "ERROR: "fmt, args)

#define _NOTE(fmt, args...) printf("NOTICE: "fmt, args)

#define _INFO(fmt, args...) printf("INFO: "fmt, args)

#define _SPEC(fmt, args...) printf("SPEC: "fmt, args)

#define _STATS(fmt, args...) printf("STATS: "fmt, args)

#else

#define _ERROR(fmt, args...) fprintf(stderr,   _RED "ERROR: %s:%s:%d: "fmt _RESET, __FILE__, __FUNCTION__, __LINE__, args)

#define _NOTE(fmt, args...) printf(_YELLOW "NOTICE: %s:%s:%d: "fmt _RESET, __FILE__, __FUNCTION__, __LINE__, args)

#define _INFO(fmt, args...) printf(_BLUE "INFO: %s:%s:%d: "fmt _RESET, __FILE__, __FUNCTION__, __LINE__, args)

#define _SPEC(fmt, args...) printf(_GREEN "SPEC: %s:%s:%d: "fmt _RESET, __FILE__, __FUNCTION__, __LINE__, args)

#define _STATS(fmt, args...) printf(_PURPLE "STATS: "fmt _RESET, args)

#endif /* HANDIN */
#endif /* DEBUG_H */
