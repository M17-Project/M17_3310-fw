#ifndef INC_FONTS_H
#define INC_FONTS_H

#include <stdint.h>

//UI icons (nokia_small only)
#define ICON_ENVELOPE	"\x80"
#define ICON_PEN		"\x81"
#define ICON_ARROW		"\x82"	//down-right bent arrow
#define ICON_RADIO_ON	"\x83"	//option list: selected
#define ICON_RADIO_OFF	"\x84"	//option list: not selected

//Cyrillic glyph table layout: 64 glyphs for 0xC0..0xFF, then CYR_EXTRA_NUM
//glyphs for the codes in cyr_extra_codes[] (see charset.h)
#define CYR_EXTRA_NUM	10

typedef struct
{
	uint8_t width;
	uint16_t rows[13];
} symbol_t;

typedef struct
{
	uint8_t height;
	uint8_t num_symbols;	//glyphs for characters ' ' .. ' '+num_symbols-1
	const symbol_t *cyr;	//Cyrillic glyphs, NULL: Latin look-alikes are drawn instead
	symbol_t symbol[];
} font_t;

typedef enum
{
	ALIGN_LEFT,
	ALIGN_CENTER,
	ALIGN_RIGHT,
	ALIGN_ARB
} align_t;

//glyph lookup with bounds check - unknown characters are drawn as '?'
const symbol_t *fontGlyph(const font_t *f, char c);

extern const font_t nokia_small;
extern const font_t nokia_small_bold;
extern const font_t nokia_big;

#endif
