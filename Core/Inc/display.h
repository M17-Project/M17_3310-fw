#ifndef INC_DISPLAY_H_
#define INC_DISPLAY_H_

#include <stdint.h>
#include "fonts.h"
#include "text_entry.h"
#include "ui_types.h"

#define RES_X					84
#define RES_Y					48
#define DISP_BUFF_SIZ			(RES_X*RES_Y/8)

#define COL_BLACK				1
#define COL_WHITE				0

#define LCD_CONTRAST_DEFAULT	0x48

//LCD configuration, sent as one block in the order the Nokia 3310 phone uses
//(the genuine 3310 controller is PCD8544-like, but not identical)
#define LCD_TEMPCO				0x06	//phone: 0x05
#define LCD_BIAS				0x13	//phone: 0x14

typedef struct __SPI_HandleTypeDef SPI_HandleTypeDef;
typedef struct disp_dev_t
{
	SPI_HandleTypeDef *spi;
	uint8_t *buff;
} disp_dev_t;

extern dev_settings_t dev_settings;

extern void setBacklight(uint8_t level);
extern uint16_t getBattVoltage(void);

//list item callback: fill the label and the (optional) right-aligned value
typedef void (*list_item_fn)(uint8_t idx, char *label, char *value, uint8_t size);

//low level
void dispWrite(disp_dev_t *disp_dev, uint8_t dc, uint8_t val);
void dispInit(disp_dev_t *disp_dev);
void dispSetContrast(disp_dev_t *disp_dev, uint8_t contrast);
void dispGotoXY(disp_dev_t *disp_dev, uint8_t x, uint8_t y);
void dispRefresh(disp_dev_t *disp_dev);
void dispFlush(disp_dev_t *disp_dev);
void dispClear(disp_dev_t *disp_dev, uint8_t color);
void setPixel(disp_dev_t *disp_dev, uint8_t x, uint8_t y, uint8_t set);
void setChar(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, char c, uint8_t color);
void drawRect(disp_dev_t *disp_dev, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color, uint8_t fill);

//text
uint16_t stringWidth(const font_t *f, const char *str);
void ellipsize(const font_t *f, char *str, uint8_t max_w);
void setString(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, const char *str, uint8_t color, align_t align);
uint8_t setStringClip(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, const char *str, uint8_t color, uint8_t max_w);
uint16_t countLines(const font_t *f, const char *str);
void drawWrapped(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, const char *str,
		uint8_t color, uint16_t first_line, uint8_t max_lines);

//screens
void showMainScreen(disp_dev_t *disp_dev);
void drawBattStatus(disp_dev_t *disp_dev);
void dispSplash(disp_dev_t *disp_dev, const char *line1, const char *line2, const char *callsign);
void showList(disp_dev_t *disp_dev, const char *title, uint8_t num, uint8_t top, uint8_t sel, list_item_fn item);
void showPopup(disp_dev_t *disp_dev, const char *text);
void showTextValueEntry(disp_dev_t *disp_dev, text_entry_t mode, const char *title, const char *text, uint16_t cursor);
void showTextMessageEntry(disp_dev_t *disp_dev, text_entry_t mode, const char *dst, const char *text, uint16_t cursor);
void showMessageView(disp_dev_t *disp_dev, const char *src, const char *dst, const char *age,
		const char *text, uint16_t scroll, uint8_t idx, uint8_t total);
void showNotify(disp_dev_t *disp_dev, const char *src, uint8_t unread);
void showTextView(disp_dev_t *disp_dev, const char *title, const char *text, uint16_t scroll);

const char *textModeLabel(text_entry_t mode);

#endif /* INC_DISPLAY_H_ */
