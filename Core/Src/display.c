#include "display.h"
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "platform.h"
#include "inbox.h"
#include "tuning.h"

//--- low level ---
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
	dispWrite(disp_dev, 0, 0x80|LCD_CONTRAST_DEFAULT);	//contrast (0x00 to 0x7F)
	dispWrite(disp_dev, 0, 0x06);			//temp coeff.
	dispWrite(disp_dev, 0, 0x13);			//LCD bias 1:48
	dispWrite(disp_dev, 0, 0x20);			//standard commands
	dispWrite(disp_dev, 0, 0x0C);			//normal mode
}

//contrast (Vop): 0..127
void dispSetContrast(disp_dev_t *disp_dev, uint8_t contrast)
{
	dispWrite(disp_dev, 0, 0x21);
	dispWrite(disp_dev, 0, 0x80|(contrast & 0x7F));
	dispWrite(disp_dev, 0, 0x20);
}

void dispGotoXY(disp_dev_t *disp_dev, uint8_t x, uint8_t y)
{
	dispWrite(disp_dev, 0, 0x80|(6*x));
	dispWrite(disp_dev, 0, 0x40|y);
}

//drawing functions only modify the frame buffer and mark it dirty,
//dispFlush() sends it to the display in a single SPI burst
static uint8_t disp_dirty;

void dispRefresh(disp_dev_t *disp_dev)
{
	(void)disp_dev;
	disp_dirty = 1;
}

void dispFlush(disp_dev_t *disp_dev)
{
	if (!disp_dirty)
		return;
	disp_dirty = 0;

	dispGotoXY(disp_dev, 0, 0);

	//the address auto-increments - send the whole frame at once
	HAL_GPIO_WritePin(DISP_DC_GPIO_Port, DISP_DC_Pin, 1);
	HAL_GPIO_WritePin(DISP_CE_GPIO_Port, DISP_CE_Pin, 0);
	HAL_SPI_Transmit(disp_dev->spi, disp_dev->buff, DISP_BUFF_SIZ, 50);
	HAL_GPIO_WritePin(DISP_CE_GPIO_Port, DISP_CE_Pin, 1);
}

void dispClear(disp_dev_t *disp_dev, uint8_t color)
{
	uint8_t val = color ? 0xFF : 0;

	memset(disp_dev->buff, val, DISP_BUFF_SIZ);

	disp_dirty = 1;
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
	uint8_t msb=(h>8)?(h+3):(h); //fonts are right-aligned
	const symbol_t *g = fontGlyph(f, c);

	for(uint8_t i=0; i<h; i++)
	{
		for(uint8_t j=0; j<g->width && j<msb; j++)
		{
			if(g->rows[i] & 1<<(msb-1-j))
				setPixel(disp_dev, x+j, y+i, color);
		}
	}

	disp_dirty = 1;
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
		for(uint8_t i=y0+1; i<y1; i++)
		{
			setPixel(disp_dev, x0, i, color);
			setPixel(disp_dev, x1, i, color);
		}
	}

	dispRefresh(disp_dev);
}

//--- text ---
uint16_t stringWidth(const font_t *f, const char *str)
{
	uint16_t w = 0;

	while (*str)
		w += fontGlyph(f, *str++)->width;

	return w;
}

//shorten a string in place so that it fits into max_w pixels ("abc.." if cut)
void ellipsize(const font_t *f, char *str, uint8_t max_w)
{
	if (stringWidth(f, str) <= max_w)
		return;

	uint8_t dots = 2 * fontGlyph(f, '.')->width;
	uint16_t w = 0;
	char *p = str;

	while (*p && w + fontGlyph(f, *p)->width + dots <= max_w)
		w += fontGlyph(f, *p++)->width;

	if (max_w >= dots)
	{
		*p++ = '.';
		*p++ = '.';
	}
	*p = 0;
}

//single line of text (wraps to the next line if it does not fit - use
//ellipsize() or setStringClip() for text of unknown length)
void setString(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, const char *str, uint8_t color, align_t align)
{
	uint16_t w = stringWidth(f, str);
	uint8_t xp;

	switch(align)
	{
		case ALIGN_CENTER:
			xp = (w < RES_X) ? (RES_X-w)/2+1 : 0;
		break;

		case ALIGN_RIGHT:
			xp = (w < RES_X) ? RES_X-w : 0;
		break;

		case ALIGN_ARB:
			xp = x;
		break;

		default: //ALIGN_LEFT
			xp = 0;
		break;
	}

	for(; *str; str++)
	{
		uint8_t cw = fontGlyph(f, *str)->width;

		if(xp + cw > RES_X)
		{
			y += f->height+1;
			xp = 0; //ALIGN_LEFT
		}

		setChar(disp_dev, xp, y, f, *str, color);
		xp += cw;
	}

	dispRefresh(disp_dev);
}

//draw as many characters as fit into max_w pixels, returns the end x position
uint8_t setStringClip(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, const char *str, uint8_t color, uint8_t max_w)
{
	uint16_t w = 0;

	for (; *str; str++)
	{
		uint8_t cw = fontGlyph(f, *str)->width;

		if (w + cw > max_w)
			break;

		setChar(disp_dev, x + w, y, f, *str, color);
		w += cw;
	}

	return x + w;
}

//find one line of word-wrapped text
//*start: first character to draw, *len: number of characters
//returns the beginning of the next line
static const char *wrapLine(const font_t *f, const char *s, const char **start, uint8_t *len)
{
	while (*s == ' ')
		s++;

	*start = s;

	const char *brk = NULL; //last space - preferred break position
	uint16_t w = 0;
	const char *p = s;

	while (*p && *p != '\n')
	{
		uint8_t cw = fontGlyph(f, *p)->width;

		if (*p == ' ')
			brk = p;

		if (w + cw > RES_X)
		{
			if (brk != NULL)
			{
				*len = brk - s;
				return brk;
			}

			if (p == s) //always consume at least one character
				p++;

			*len = p - s; //word longer than the screen: hard break
			return p;
		}

		w += cw;
		p++;
	}

	*len = p - s;

	return (*p == '\n') ? p + 1 : p;
}

uint16_t countLines(const font_t *f, const char *str)
{
	uint16_t n = 0;
	const char *start;
	uint8_t len;

	while (*str)
	{
		str = wrapLine(f, str, &start, &len);
		if (len == 0 && *start == 0) //only trailing spaces left
			break;
		n++;
	}

	return n;
}

void drawWrapped(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, const char *str,
		uint8_t color, uint16_t first_line, uint8_t max_lines)
{
	uint16_t line = 0;
	const char *start;
	uint8_t len;

	while (*str && max_lines)
	{
		str = wrapLine(f, str, &start, &len);

		if (len == 0 && *start == 0)
			break;

		if (line++ < first_line)
			continue;

		uint8_t xp = x;
		for (uint8_t i=0; i<len; i++)
		{
			setChar(disp_dev, xp, y, f, start[i], color);
			xp += fontGlyph(f, start[i])->width;
		}

		y += f->height + 1;
		max_lines--;
	}

	dispRefresh(disp_dev);
}

//1 if the font cannot draw some character of 'str' properly (Cyrillic without glyphs)
static uint8_t needsCyrillic(const font_t *f, const char *str)
{
	if (f->cyr != NULL)
		return 0;

	for (; *str; str++)
		if ((uint8_t)*str >= 0xA0)
			return 1;

	return 0;
}

//centered line in the big font, small font if it does not fit (or is Cyrillic)
static void drawCenteredFit(disp_dev_t *disp_dev, uint8_t y, const char *str)
{
	char buf[32];

	if (!needsCyrillic(&nokia_big, str) && stringWidth(&nokia_big, str) <= RES_X)
	{
		setString(disp_dev, 0, y, &nokia_big, str, COL_BLACK, ALIGN_CENTER);
		return;
	}

	snprintf(buf, sizeof(buf), "%s", str);
	ellipsize(&nokia_small, buf, RES_X);
	setString(disp_dev, 0, y + 3, &nokia_small, buf, COL_BLACK, ALIGN_CENTER);
}

//--- screens ---
const char *textModeLabel(text_entry_t mode)
{
	switch (mode)
	{
		case TEXT_LOWERCASE:	return "abc";
		case TEXT_UPPERCASE:	return "ABC";
		case TEXT_T9:			return "T9";
		case TEXT_NUMERIC:		return "123";
		case TEXT_UKR_LOWER:	return "\xE0\xE1\xE2";	//Ukrainian a, be, ve
		case TEXT_UKR_UPPER:	return "\xC0\xC1\xC2";
		default:				return "";
	}
}

//battery indicator in the upper right corner
void drawBattStatus(disp_dev_t *disp_dev)
{
	char str[8];

	drawRect(disp_dev, RES_X-1-15, 0, RES_X-1, 8, COL_WHITE, 1);

	if (isCharging())
	{
		snprintf(str, sizeof(str), "B+");
	}
	else
	{
		uint16_t u_batt = getBattVoltage();
		if (u_batt > 3500)
			snprintf(str, sizeof(str), "%u.%u", u_batt/1000, (u_batt%1000)/100);
		else
			snprintf(str, sizeof(str), "Lo");
	}

	setString(disp_dev, RES_X-1, 0, &nokia_small, str, COL_BLACK, ALIGN_RIGHT);
}

void showMainScreen(disp_dev_t *disp_dev)
{
	char str[32];
	const ch_settings_t *ch = &dev_settings.channel;

	dispClear(disp_dev, COL_WHITE);

	//status line: mode, memory channel, unread messages, battery
	if (dev_settings.tuning_mode == TUNING_MEM)
		snprintf(str, sizeof(str), "%s M%u/%u", (ch->mode==RF_MODE_DIG) ? "Dig" : "Ana",
				dev_settings.ch_num + 1, tuneNumChannels());
	else
		snprintf(str, sizeof(str), "%s VFO", (ch->mode==RF_MODE_DIG) ? "Dig" : "Ana");
	uint8_t x = setStringClip(disp_dev, 0, 0, &nokia_small, str, COL_BLACK, 52);

	if (inboxUnread())
		setString(disp_dev, (x < 44) ? 46 : x + 2, 0, &nokia_small, ICON_ENVELOPE, COL_BLACK, ALIGN_ARB);

	drawBattStatus(disp_dev);

	char dst[16];
	snprintf(dst, sizeof(dst), ICON_ARROW "%s", ch->dst);
	ellipsize(&nokia_small, dst, RES_X);

	uint8_t split = (ch->tx_frequency != ch->rx_frequency);

	if (dev_settings.tuning_mode == TUNING_MEM)
	{
		//name, RX frequency (+ TX shift for a split channel), destination
		drawCenteredFit(disp_dev, 12, ch->ch_name);

		fmtFreq(str, sizeof(str), ch->rx_frequency);
		if (split)
		{
			uint8_t l = strlen(str);
			str[l++] = ' ';
			fmtSignedMHz(&str[l], sizeof(str) - l, (int32_t)(ch->tx_frequency - ch->rx_frequency));
		}
		setString(disp_dev, 0, 27, &nokia_small, str, COL_BLACK, ALIGN_CENTER);
		setString(disp_dev, 0, 36, &nokia_small, dst, COL_BLACK, ALIGN_CENTER);
	}
	else
	{
		//RX frequency, destination, TX frequency only for a split
		fmtFreq(str, sizeof(str), ch->rx_frequency);
		drawCenteredFit(disp_dev, 12, str);
		setString(disp_dev, 0, 27, &nokia_small, dst, COL_BLACK, ALIGN_CENTER);

		if (split)
		{
			str[0] = 'T'; str[1] = ' ';
			fmtFreq(&str[2], sizeof(str)-2, ch->tx_frequency);
			setString(disp_dev, 0, 36, &nokia_small, str, COL_BLACK, ALIGN_CENTER);
		}
	}
}

void dispSplash(disp_dev_t *disp_dev, const char *line1, const char *line2, const char *callsign)
{
	setBacklight(0);

	dispClear(disp_dev, COL_WHITE);
	drawCenteredFit(disp_dev, 9, line1);
	drawCenteredFit(disp_dev, 22, line2);
	setString(disp_dev, 0, 40, &nokia_small, callsign, COL_BLACK, ALIGN_CENTER);
	dispFlush(disp_dev);

	//fade in
	for(uint16_t i=0; i<dev_settings.backlight_level; i++)
	{
		setBacklight(i);
		HAL_Delay(5);
	}
}

//Nokia-style scroll bar on the right: dotted track, solid slider at the selected item
#define SCROLL_W		3		//slider width
#define SCROLL_GAP		2		//space between the list and the bar
#define SCROLL_Y0		8
#define SCROLL_Y1		(RES_Y-1)

static void drawScrollBar(disp_dev_t *disp_dev, uint8_t num, uint8_t sel)
{
	const uint8_t x = RES_X - SCROLL_W;
	const uint8_t track = SCROLL_Y1 - SCROLL_Y0 + 1;

	//track: dotted centre line
	for (uint8_t y=SCROLL_Y0; y<=SCROLL_Y1; y+=2)
		setPixel(disp_dev, x + SCROLL_W/2, y, COL_BLACK);

	//slider: proportional to the visible part, at least 5 px
	uint8_t h = (uint16_t)track * LIST_ROWS / num;
	if (h < 5)
		h = 5;

	uint8_t y = SCROLL_Y0 + (uint16_t)(track - h) * sel / (num - 1);
	drawRect(disp_dev, x, y, RES_X-1, y + h - 1, COL_BLACK, 1);
}

//list with a bold title and LIST_ROWS rows, 'sel' is highlighted
//a scroll bar is shown when not all items fit
void showList(disp_dev_t *disp_dev, const char *title, uint8_t num, uint8_t top, uint8_t sel, list_item_fn item)
{
	char t[32];
	uint8_t scroll = (num > LIST_ROWS);
	uint8_t right = scroll ? RES_X - SCROLL_W - SCROLL_GAP : RES_X;	//usable width

	dispClear(disp_dev, COL_WHITE);

	snprintf(t, sizeof(t), "%s", title);
	ellipsize(&nokia_small_bold, t, RES_X);
	setString(disp_dev, 0, 0, &nokia_small_bold, t, COL_BLACK, ALIGN_CENTER);

	for (uint8_t i=0; i<LIST_ROWS && top+i<num; i++)
	{
		char label[40], value[40];
		uint8_t y = (i+1)*9;
		uint8_t col = COL_BLACK;

		label[0] = value[0] = 0;
		item(top+i, label, value, sizeof(label));

		if (top+i == sel)
		{
			drawRect(disp_dev, 0, y-1, right-1, y+8, COL_BLACK, 1);
			col = COL_WHITE;
		}

		//the label has priority, the value gets the rest (at least a third)
		uint8_t vw = 0;
		if (value[0])
		{
			uint16_t lw = stringWidth(&nokia_small, label);
			uint8_t max_vw = (lw + 3 < right - right/3) ? right - lw - 3 : right/3;
			ellipsize(&nokia_small, value, max_vw);
			vw = stringWidth(&nokia_small, value);
			setString(disp_dev, right - vw, y, &nokia_small, value, col, ALIGN_ARB);
		}

		setStringClip(disp_dev, 1, y, &nokia_small, label, col, right - 1 - (vw ? vw + 2 : 0));
	}

	if (scroll)
		drawScrollBar(disp_dev, num, sel);

	dispRefresh(disp_dev);
}

void showPopup(disp_dev_t *disp_dev, const char *text)
{
	dispClear(disp_dev, COL_WHITE);

	if (!needsCyrillic(&nokia_big, text) && stringWidth(&nokia_big, text) <= RES_X)
		setString(disp_dev, 0, 17, &nokia_big, text, COL_BLACK, ALIGN_CENTER);
	else
		drawWrapped(disp_dev, 0, 10, &nokia_small, text, COL_BLACK, 0, 3);
}

//header: entry mode on the left, 'right' on the right
static void drawEntryHeader(disp_dev_t *disp_dev, text_entry_t mode, const char *right)
{
	char str[32];

	snprintf(str, sizeof(str), ICON_PEN "%s", textModeLabel(mode));
	uint8_t x = setStringClip(disp_dev, 0, 0, &nokia_small, str, COL_BLACK, RES_X);

	if (right && *right)
	{
		snprintf(str, sizeof(str), "%s", right);
		ellipsize(&nokia_small, str, RES_X - x - 3);
		setString(disp_dev, 0, 0, &nokia_small, str, COL_BLACK, ALIGN_RIGHT);
	}
}

#define CURSOR_GAP	2		//extra pixels opened in the text at the cursor

//draw n characters, returns the end x position
static uint8_t drawSpan(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, const char *s, uint16_t n)
{
	while (n-- && *s)
	{
		setChar(disp_dev, x, y, f, *s, COL_BLACK);
		x += fontGlyph(f, *s++)->width;
	}

	return x;
}

//one line with the cursor: text before it, a bar with a gap on each side, the rest
static void drawCursorLine(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f,
		const char *s, uint16_t len, uint16_t cur, uint8_t max_x)
{
	x = drawSpan(disp_dev, x, y, f, s, cur);
	drawRect(disp_dev, x, y, x, y + f->height - 1, COL_BLACK, 1);
	x += CURSOR_GAP;

	for (s += cur, len -= cur; len && *s; len--, s++)
	{
		uint8_t w = fontGlyph(f, *s)->width;
		if (x + w > max_x)
			break;
		setChar(disp_dev, x, y, f, *s, COL_BLACK);
		x += w;
	}

	dispRefresh(disp_dev);
}

static uint16_t widthN(const font_t *f, const char *s, uint16_t n)
{
	uint16_t w = 0;

	while (n-- && *s)
		w += fontGlyph(f, *s++)->width;

	return w;
}

//single value: one line, scrolled horizontally so that the cursor is visible
void showTextValueEntry(disp_dev_t *disp_dev, text_entry_t mode, const char *title, const char *text, uint16_t cursor)
{
	const uint8_t box_w = RES_X - 7;

	dispClear(disp_dev, COL_WHITE);
	drawEntryHeader(disp_dev, mode, title);
	drawRect(disp_dev, 0, 9, RES_X-1, RES_Y-11, COL_BLACK, 0);

	//big font, small one for Cyrillic text (the big font has no Cyrillic glyphs)
	const font_t *f = needsCyrillic(&nokia_big, text) ? &nokia_small : &nokia_big;
	uint8_t y = (f == &nokia_big) ? 13 : 17;

	//first visible character: the text up to the cursor has to fit
	uint16_t len = strlen(text);
	if (cursor > len)
		cursor = len;

	uint16_t start = 0;
	while (start < cursor && widthN(f, &text[start], cursor - start) > box_w - CURSOR_GAP)
		start++;

	drawCursorLine(disp_dev, 3, y, f, &text[start], len - start, cursor - start, 3 + box_w);

	setString(disp_dev, 0, RES_Y-8, &nokia_small_bold, "Ok", COL_BLACK, ALIGN_CENTER);
}

//find the wrapped line holding the cursor: line index and offset in the line
//(the line index may be one past the last line: cursor after a wrapping space)
static uint16_t locateCursor(const font_t *f, const char *text, uint16_t cursor, uint16_t *off)
{
	const char *p = text;
	const char *start;
	uint8_t len;
	uint16_t line = 0;

	*off = 0;

	while (*p)
	{
		p = wrapLine(f, p, &start, &len);

		if (len == 0 && *start == 0)
			break;

		uint16_t so = start - text;

		if (cursor <= so + len)
		{
			*off = (cursor > so) ? cursor - so : 0;
			return line;
		}

		line++;
	}

	return line;
}

//text message: 3 lines, scrolled so that the cursor line is visible
void showTextMessageEntry(disp_dev_t *disp_dev, text_entry_t mode, const char *dst, const char *text, uint16_t cursor)
{
	char to[16];
	uint16_t off;

	dispClear(disp_dev, COL_WHITE);

	snprintf(to, sizeof(to), ICON_ARROW "%s", dst);
	drawEntryHeader(disp_dev, mode, to);

	const font_t *f = &nokia_small;
	uint16_t cline = locateCursor(f, text, cursor, &off);
	uint16_t first = (cline > 2) ? cline - 2 : 0;

	const char *p = text;
	const char *start;
	uint8_t len;
	uint16_t line = 0;
	uint8_t y = 10;

	while (line < first + 3)
	{
		uint8_t empty = (*p == 0);

		if (!empty)
		{
			p = wrapLine(f, p, &start, &len);
			if (len == 0 && *start == 0)
				empty = 1;
		}

		if (empty) //past the end: only the cursor can be here
		{
			start = "";
			len = 0;
		}

		if (line >= first)
		{
			if (line == cline)
				drawCursorLine(disp_dev, 0, y, f, start, len, off, RES_X);
			else
				drawSpan(disp_dev, 0, y, f, start, len);
			y += f->height + 1;
		}

		if (empty && line >= cline)
			break;
		line++;
	}

	setString(disp_dev, 0, RES_Y-8, &nokia_small_bold, "Send", COL_BLACK, ALIGN_CENTER);
}

void showMessageView(disp_dev_t *disp_dev, const char *src, const char *dst, const char *age,
		const char *text, uint16_t scroll, uint8_t idx, uint8_t total)
{
	char str[16];

	dispClear(disp_dev, COL_WHITE);

	setString(disp_dev, 0, 0, &nokia_small, src, COL_BLACK, ALIGN_LEFT);
	if (age && *age)
		setString(disp_dev, 0, 0, &nokia_small, age, COL_BLACK, ALIGN_RIGHT);

	snprintf(str, sizeof(str), ICON_ARROW "%s", dst);
	setString(disp_dev, 0, 8, &nokia_small, str, COL_BLACK, ALIGN_LEFT);
	snprintf(str, sizeof(str), "%u/%u", idx + 1, total);
	setString(disp_dev, 0, 8, &nokia_small, str, COL_BLACK, ALIGN_RIGHT);

	drawWrapped(disp_dev, 0, 18, &nokia_small, text, COL_BLACK, scroll, 3);
}

void showNotify(disp_dev_t *disp_dev, const char *src, uint8_t unread)
{
	char str[24];

	dispClear(disp_dev, COL_WHITE);

	setString(disp_dev, 0, 0, &nokia_small, ICON_ENVELOPE, COL_BLACK, ALIGN_CENTER);

	snprintf(str, sizeof(str), "%u message%s", unread, (unread == 1) ? "" : "s");
	drawCenteredFit(disp_dev, 10, str);

	snprintf(str, sizeof(str), "from %s", src);
	setString(disp_dev, 0, 26, &nokia_small, str, COL_BLACK, ALIGN_CENTER);

	setString(disp_dev, 0, RES_Y-8, &nokia_small_bold, "Read", COL_BLACK, ALIGN_CENTER);
}

void showTextView(disp_dev_t *disp_dev, const char *title, const char *text, uint16_t scroll)
{
	char t[32];

	dispClear(disp_dev, COL_WHITE);

	snprintf(t, sizeof(t), "%s", title);
	ellipsize(&nokia_small_bold, t, RES_X);
	setString(disp_dev, 0, 0, &nokia_small_bold, t, COL_BLACK, ALIGN_CENTER);

	drawWrapped(disp_dev, 0, 9, &nokia_small, text, COL_BLACK, scroll, 4);
}
