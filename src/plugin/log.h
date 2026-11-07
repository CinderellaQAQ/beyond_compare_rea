#ifndef BC3_LOG_H
#define BC3_LOG_H

#include <windows.h>

void log_initialize(void);
void log_line(const char *format, ...);

#endif
