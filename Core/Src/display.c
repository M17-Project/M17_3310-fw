#include "display.h"
#include <string.h>
#include <stdio.h>
#include "main.h"

static uint8_t wrapLineStarts(const font_t *f, const char *str, const char **lines, uint8_t max)
{
	uint8_t xp = 0;
	uint8_t n  = 0;

	if (!str || !*str || max == 0)
		return 0;

	lines[n++] = str;

	while (*str)
	{
		// skip spaces
		while (*str == ' ')
		{
			xp += f->symbol[0].width;
			str++;
		}

		// stop if we reached the end
		if (!*str)
			break;

		// measure next word
		const char *w = str;
		uint8_t ww = 0;

		while (*w && *w != ' ')
		{
			ww += f->symbol[*w - ' '].width;
			w++;
		}

		// wrap before word
		if (xp && xp + ww > RES_X)
		{
			xp = 0;

			if (n < max && *str)
				lines[n++] = str;
		}

		// consume word
		while (*str && *str != ' ')
		{
			xp += f->symbol[*str - ' '].width;
			str++;
		}
	}

	return n;
}

void dispWrite(disp_dev_t *disp_dev, uint8_t dc, uint8_t val)
{
	HAL_GPIO_WritePin(DISP_DC_GPIO_Port, DISP_DC_Pin, dc);

	HAL_GPIO_WritePin(DISP_CE_GPIO_Port, DISP_CE_Pin, 0);
	HAL_SPI_Transmit(disp_dev->spi, &val, 1, 10);
	HAL_GPIO_WritePin(DISP_CE_GPIO_Port, DISP_CE_Pin, 1);
}

void dispInit(disp_dev_t *disp_dev)
{
	HAL_GPIO_WritePin(DISP_DC_GPIO_Port, DISP_DC_Pin, 1);
	HAL_GPIO_WritePin(DISP_CE_GPIO_Port, DISP_CE_Pin, 1);
	HAL_GPIO_WritePin(DISP_RST_GPIO_Port, DISP_RST_Pin, 1);

	//toggle LCD reset
	HAL_GPIO_WritePin(DISP_RST_GPIO_Port, DISP_RST_Pin, 0); //RES low
	HAL_Delay(10);
	HAL_GPIO_WritePin(DISP_RST_GPIO_Port, DISP_RST_Pin, 1); //RES high
	HAL_Delay(10);

	dispWrite(disp_dev, 0, 0x21);			//extended commands
	dispWrite(disp_dev, 0, 0x80|0x48);		//contrast (0x00 to 0x7F)
	dispWrite(disp_dev, 0, 0x06);			//temp coeff.
	dispWrite(disp_dev, 0, 0x13);			//LCD bias 1:48
	dispWrite(disp_dev, 0, 0x20);			//standard commands
	dispWrite(disp_dev, 0, 0x0C);			//normal mode
}

void dispGotoXY(disp_dev_t *disp_dev, uint8_t x, uint8_t y)
{
	dispWrite(disp_dev, 0, 0x80|(6*x));
	dispWrite(disp_dev, 0, 0x40|y);
}

void dispRefresh(disp_dev_t *disp_dev)
{
	dispGotoXY(disp_dev, 0, 0);

	for(uint16_t i=0; i<DISP_BUFF_SIZ; i++)
		dispWrite(disp_dev, 1, disp_dev->buff[i]);
}

void dispClear(disp_dev_t *disp_dev, uint8_t color)
{
	uint8_t val = color ? 0xFF : 0;

	memset(disp_dev->buff, val, DISP_BUFF_SIZ);

	dispGotoXY(disp_dev, 0, 0);

	for(uint16_t i=0; i<DISP_BUFF_SIZ; i++)
		dispWrite(disp_dev, 1, val);
}

//set pixel
// 1 - black
// 0 - white
void setPixel(disp_dev_t *disp_dev, uint8_t x, uint8_t y, uint8_t set)
{
	if(x<RES_X && y<RES_Y)
	{
		uint16_t loc = x + (y/8)*RES_X;

		if(set)
			disp_dev->buff[loc] |= (1<<(y%8));
		else
			disp_dev->buff[loc] &= ~(1<<(y%8));
	}
}

void setChar(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, char c, uint8_t color)
{
	uint8_t h=f->height;
	c-=' ';

	for(uint8_t i=0; i<h; i++)
	{
		for(uint8_t j=0; j<f->symbol[(uint8_t)c].width; j++)
		{
			if(f->symbol[(uint8_t)c].rows[i] & 1<<(((h>8)?(h+3):(h))-1-j)) //fonts are right-aligned
				setPixel(disp_dev, x+j, y+i, color);
		}
	}
}

//TODO: fix multiline text alignment when not in ALIGN_LEFT mode
void setString(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, const char *str, uint8_t color, align_t align)
{
	uint8_t xp=0, w=0;
	uint8_t len = strlen(str);

	//get width
	for(uint8_t i=0; i<len; i++)
		w+=f->symbol[str[i]-' '].width;

	switch(align)
	{
		case ALIGN_LEFT:
			xp=0;
		break;

		case ALIGN_CENTER:
			xp=(RES_X-w)/2+1;
		break;

		case ALIGN_RIGHT:
			xp=RES_X-w;
		break;

		case ALIGN_ARB:
			xp=x;
		break;

		default: //ALIGN_LEFT
			xp=0;
		break;
	}

	for(uint8_t i=0; i<len; i++)
	{
		if(xp > RES_X-f->symbol[str[i]-' '].width)
		{
			y+=f->height+1;
			xp=0; //ALIGN_LEFT
		}

		setChar(disp_dev, xp, y, f, str[i], color);
		xp+=f->symbol[str[i]-' '].width;
	}

	dispRefresh(disp_dev);
}

//display text with word wrapping
void setStringWordWrap(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, const char *str, uint8_t color)
{
	uint8_t xp = 0;

	while (*str)
	{
		// measure next word
		const char *w = str;
		uint8_t wlen = 0;
		uint8_t ww = 0;

		while (w[wlen] && w[wlen] != ' ')
		{
			ww += f->symbol[w[wlen] - ' '].width;
			wlen++;
		}

		// wrap before word if needed
		if (xp + ww > RES_X)
		{
			y += f->height + 1;
			xp = 0;
		}

		// draw word
		for (uint8_t i = 0; i < wlen; i++)
		{
			setChar(disp_dev, xp, y, f, str[i], color);
			xp += f->symbol[str[i] - ' '].width;
		}

		// draw space
		if (*w == ' ')
		{
			setChar(disp_dev, xp, y, f, ' ', color);
			xp += f->symbol[0].width;
			wlen++;
		}

		str += wlen;
	}

	dispRefresh(disp_dev);
}

//display text with word wrapping - only the last few lines (display's limitation)
void setStringWordWrapLastLines(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, const char *str, uint8_t color, uint8_t max_lines)
{
	const char *lines[16];
	uint8_t total = wrapLineStarts(f, str, lines, 16);

	if (!total)
		return;

	uint8_t first = (total > max_lines) ? (total - max_lines) : 0;

	for (uint8_t i = first; i < total; i++)
	{
		uint8_t xp = x;
		const char *s = lines[i];
		const char *end = (i + 1 < total) ? lines[i + 1] : NULL;

		while (*s == ' ')
			s++;

		while (*s && s != end)
		{
			setChar(disp_dev, xp, y, f, *s, color);
			xp += f->symbol[*s - ' '].width;
			s++;
		}

		y += f->height + 1;
	}

	dispRefresh(disp_dev);
}

void drawRect(disp_dev_t *disp_dev, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color, uint8_t fill)
{
	x0 = (x0>=RES_X) ? RES_X-1 : x0;
	y0 = (y0>=RES_Y) ? RES_Y-1 : y0;
	x1 = (x1>=RES_X) ? RES_X-1 : x1;
	y1 = (y1>=RES_Y) ? RES_Y-1 : y1;

	if(fill)
	{
		for(uint8_t i=x0; i<=x1; i++)
		{
			for(uint8_t j=y0; j<=y1; j++)
			{
				setPixel(disp_dev, i, j, color);
			}
		}
	}
	else
	{
		for(uint8_t i=x0; i<=x1; i++)
		{
			setPixel(disp_dev, i, y0, color);
			setPixel(disp_dev, i, y1, color);
		}
		for(uint8_t i=y0+1; i<=y1-1; i++)
		{
			setPixel(disp_dev, x0, i, color);
			setPixel(disp_dev, x1, i, color);
		}
	}

	dispRefresh(disp_dev);
}

void showMainScreen(disp_dev_t *disp_dev)
{
	char str[24];

	dispClear(disp_dev, COL_WHITE);

	sprintf(str, "%s %s", (dev_settings.channel.mode==RF_MODE_DIG) ? "Dig" : "Ana",
			(dev_settings.tuning_mode==TUNING_MEM) ? "Mem" : "VFO");
	setString(disp_dev, 0, 0, &nokia_small, str, COL_BLACK, ALIGN_LEFT);

	setString(disp_dev, 0, 12, &nokia_big, dev_settings.channel.ch_name, COL_BLACK, ALIGN_CENTER);

	sprintf(str, "R %ld.%04ld",
			dev_settings.channel.rx_frequency/1000000,
			(dev_settings.channel.rx_frequency%1000000)/100);
	setString(disp_dev, 0, 27, &nokia_small, str, COL_BLACK, ALIGN_CENTER);

	sprintf(str, "T %ld.%04ld",
			dev_settings.channel.tx_frequency/1000000,
			(dev_settings.channel.tx_frequency%1000000)/100);
	setString(disp_dev, 0, 36, &nokia_small, str, COL_BLACK, ALIGN_CENTER);

	//is the battery charging? (read /CHG signal)
	if(!(CHG_GPIO_Port->IDR & CHG_Pin))
	{
		setString(disp_dev, RES_X-1, 0, &nokia_small, "B+", COL_BLACK, ALIGN_RIGHT); //display charging status
	}
	else
	{
		char u_batt_str[8];
		uint16_t u_batt = getBattVoltage();
		sprintf(u_batt_str, "%1d.%1d", u_batt/1000, (u_batt%1000)/100);
		setString(disp_dev, RES_X-1, 0, &nokia_small, u_batt_str, COL_BLACK, ALIGN_RIGHT); //display voltage
	}
}

void dispSplash(disp_dev_t *disp_dev, const char *line1, const char *line2, const char *callsign)
{
	setBacklight(0);

	setString(disp_dev, 0, 9, &nokia_big, line1, COL_BLACK, ALIGN_CENTER);
	setString(disp_dev, 0, 22, &nokia_big, line2, COL_BLACK, ALIGN_CENTER);
	setString(disp_dev, 0, 40, &nokia_small, callsign, COL_BLACK, ALIGN_CENTER);

	//fade in
	for(uint16_t i=0; i<dev_settings.backlight_level; i++)
	{
		setBacklight(i);
		HAL_Delay(5);
	}
}

void showRcvdTextMessage(disp_dev_t *disp_dev, const char* src, const char* dst, const char* msg)
{
	dispClear(disp_dev, COL_WHITE);

	char header[2][8+10+1];
	sprintf(&header[0][0], "F: %s", src);
	sprintf(&header[1][0], "T: %s", dst);

	setString(disp_dev, 0, 0, &nokia_small, header[0], COL_BLACK, ALIGN_LEFT);
	setString(disp_dev, 0, 8, &nokia_small, header[1], COL_BLACK, ALIGN_LEFT);

	setStringWordWrapLastLines(disp_dev, 0, 18, &nokia_small, msg, COL_BLACK, 3);

	//press C to exit
}

void showTextMessageEntry(disp_dev_t *disp_dev, text_entry_t entry_mode)
{
	dispClear(disp_dev, COL_WHITE);

	redrawTextEntryIcon(disp_dev, entry_mode);

	setString(disp_dev, 0, RES_Y-8, &nokia_small_bold, "Send", COL_BLACK, ALIGN_CENTER);
}

void showTextValueEntry(disp_dev_t *disp_dev, text_entry_t entry_mode)
{
	dispClear(disp_dev, COL_WHITE);

	redrawTextEntryIcon(disp_dev, entry_mode);

	drawRect(disp_dev, 0, 9, RES_X-1, RES_Y-11, COL_BLACK, 0);

	setString(disp_dev, 0, RES_Y-8, &nokia_small_bold, "Ok", COL_BLACK, ALIGN_CENTER);
}

//for text messages
void redrawMsgEntry(disp_dev_t *disp_dev, const char *text)
{
	drawRect(disp_dev, 0, 10, RES_X-1, RES_Y-9, COL_WHITE, 1);
	setStringWordWrapLastLines(disp_dev, 0, 10, &nokia_small, text, COL_BLACK, 3);
}

void redrawValueEntry(disp_dev_t *disp_dev, const char *text)
{
	drawRect(disp_dev, 1, 10, RES_X-2, RES_Y-12, COL_WHITE, 1);
	setString(disp_dev, 3, 13, &nokia_big, text, COL_BLACK, ALIGN_ARB);
}

void redrawTextEntryIcon(disp_dev_t *disp_dev, text_entry_t entry_mode)
{
	drawRect(disp_dev, 0, 0, 21, 8, COL_WHITE, 1);

	if (entry_mode == TEXT_LOWERCASE)
		setString(disp_dev, 0, 0, &nokia_small, ICON_PEN "abc", COL_BLACK, ALIGN_LEFT);
	else if (entry_mode == TEXT_UPPERCASE)
		setString(disp_dev, 0, 0, &nokia_small, ICON_PEN "ABC", COL_BLACK, ALIGN_LEFT);
	else
		setString(disp_dev, 0, 0, &nokia_small, ICON_PEN "T9", COL_BLACK, ALIGN_LEFT);
}

//show menu with item highlighting
//start_item - absolute index of first item to display
//h_item - item to highlight, relative value: 0..3
void showMenu(disp_dev_t *disp_dev, const disp_t *menu, uint8_t start_item, uint8_t h_item)
{
	dispClear(disp_dev, COL_WHITE);

	setString(disp_dev, 0, 0, &nokia_small_bold, menu->title, COL_BLACK, ALIGN_CENTER);

	for(uint8_t i=0; i<start_item+menu->num_items && i<4; i++)
	{
		//highlight
		if(i==h_item)
			drawRect(disp_dev, 0, (i+1)*9-1, RES_X-1, (i+1)*9+8, COL_BLACK, 1);

		setString(disp_dev, 1, (i+1)*9, &nokia_small, menu->item[i+start_item], (i==h_item)?COL_WHITE:COL_BLACK, ALIGN_ARB);
		if(menu->value[i+start_item][0]!=0)
			setString(disp_dev, 0, (i+1)*9, &nokia_small, menu->value[i+start_item], (i==h_item)?COL_WHITE:COL_BLACK, ALIGN_RIGHT);
	}
}

void redrawText(disp_dev_t *disp_dev, disp_state_t disp_state, char *text)
{
	if (disp_state == DISP_TEXT_MSG_ENTRY)
	{
		redrawMsgEntry(disp_dev, text);
	}
	else if (disp_state == DISP_TEXT_VALUE_ENTRY)
	{
		redrawValueEntry(disp_dev, text);
	}
}
