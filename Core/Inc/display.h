#ifndef INC_DISPLAY_H_
#define INC_DISPLAY_H_

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "typedefs.h"
#include "fonts.h"
#include "settings.h"
#include "ui_types.h"

#define RES_X					84
#define RES_Y					48
#define DISP_BUFF_SIZ			(RES_X*RES_Y/8)

#define COL_BLACK				1
#define COL_WHITE				0

typedef struct disp_dev_t
{
	SPI_HandleTypeDef *spi;
	uint8_t *buff;
} disp_dev_t;

extern dev_settings_t dev_settings;

extern void setBacklight(uint8_t level);
extern uint16_t getBattVoltage(void);

void dispWrite(disp_dev_t *disp_dev, uint8_t dc, uint8_t val);
void dispInit(disp_dev_t *disp_dev);
void dispGotoXY(disp_dev_t *disp_dev, uint8_t x, uint8_t y);
void dispRefresh(disp_dev_t *disp_dev);
void dispClear(disp_dev_t *disp_dev, uint8_t color);
void setPixel(disp_dev_t *disp_dev, uint8_t x, uint8_t y, uint8_t set);
void setChar(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, char c, uint8_t color);
void setString(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, const char *str, uint8_t color, align_t align);
void setStringWordWrap(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, const char *str, uint8_t color);
void setStringWordWrapLastLines(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, const char *str, uint8_t color, uint8_t max_lines);

void drawRect(disp_dev_t *disp_dev, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color, uint8_t fill);
void showMainScreen(disp_dev_t *disp_dev);
void dispSplash(disp_dev_t *disp_dev, const char *line1, const char *line2, const char *callsign);
void showTextMessageEntry(disp_dev_t *disp_dev, text_entry_t text_mode);
void showTextValueEntry(disp_dev_t *disp_dev, text_entry_t text_mode);
void redrawTextEntryIcon(disp_dev_t *disp_dev, text_entry_t mode);

void redrawMsgEntry(disp_dev_t *disp_dev, const char *text);
void redrawValueEntry(disp_dev_t *disp_dev, const char *text);

void showMenu(disp_dev_t *disp_dev, const disp_t *menu, uint8_t start_item, uint8_t h_item);

void redrawText(disp_dev_t *disp_dev, disp_state_t disp_state, char *text);

#endif /* INC_DISPLAY_H_ */
