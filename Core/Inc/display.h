#ifndef INC_DISPLAY_H_
#define INC_DISPLAY_H_

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "typedefs.h"
#include "fonts.h"
#include "menus.h"
#include "settings.h"

#define RES_X					84
#define RES_Y					48
#define DISP_BUFF_SIZ			(RES_X*RES_Y/8)

extern dev_settings_t dev_settings;

extern void setBacklight(uint8_t level);
extern uint16_t getBattVoltage(void);

void dispWrite(SPI_HandleTypeDef *spi, uint8_t dc, uint8_t val);
void dispInit(SPI_HandleTypeDef *spi);
void dispGotoXY(SPI_HandleTypeDef *spi, uint8_t x, uint8_t y);
void dispRefresh(SPI_HandleTypeDef *spi, uint8_t buff[DISP_BUFF_SIZ]);
void dispClear(SPI_HandleTypeDef *spi, uint8_t buff[DISP_BUFF_SIZ], uint8_t fill);
void setPixel(uint8_t buff[DISP_BUFF_SIZ], uint8_t x, uint8_t y, uint8_t set);
void setChar(uint8_t buff[DISP_BUFF_SIZ], uint8_t x, uint8_t y, const font_t *f, char c, uint8_t color);
void setString(SPI_HandleTypeDef *spi, uint8_t buff[DISP_BUFF_SIZ], uint8_t x, uint8_t y, const font_t *f, char *str, uint8_t color, align_t align);

void drawRect(SPI_HandleTypeDef *spi, uint8_t buff[DISP_BUFF_SIZ], uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color, uint8_t fill);
void showMainScreen(SPI_HandleTypeDef *spi, uint8_t buff[DISP_BUFF_SIZ]);
void dispSplash(SPI_HandleTypeDef *spi, uint8_t buff[DISP_BUFF_SIZ], char *line1, char *line2, char *callsign);
void showTextMessageEntry(SPI_HandleTypeDef *spi, uint8_t buff[DISP_BUFF_SIZ], text_entry_t text_mode);
void showTextValueEntry(SPI_HandleTypeDef *spi, uint8_t buff[DISP_BUFF_SIZ], text_entry_t text_mode);
void showMenu(SPI_HandleTypeDef *spi, uint8_t buff[DISP_BUFF_SIZ], disp_t menu, uint8_t start_item, uint8_t h_item);

#endif /* INC_DISPLAY_H_ */
