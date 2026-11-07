#ifndef BC3_MULTIDETAILS_H
#define BC3_MULTIDETAILS_H

#include <windows.h>

BOOL multidetails_initialize(void);
void multidetails_shutdown(void);
void multidetails_on_click(void *frame, void *sender);
void multidetails_on_current_changed(void *frame, void *sender);
void multidetails_on_details_resize(void *frame);
BOOL multidetails_is_internal_update(void);

#endif
