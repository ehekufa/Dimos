#ifndef DIMAOS_GUI_H
#define DIMAOS_GUI_H

#include "types.h"

typedef enum {
    APP_MY_COMPUTER = 0,
    APP_NOTEPAD,
    APP_CMD,
    APP_ABOUT
} DIMA_APP_ID;

void GuiInit(void);
void GuiTask(void);
void GuiOpenApp(DIMA_APP_ID app_id);

#endif /* DIMAOS_GUI_H */
