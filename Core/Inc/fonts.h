#ifndef INC_FONTS_H
#define INC_FONTS_H

#include <stdint.h>

#define ICON_ENVELOPE	"\x80"
#define ICON_PEN		"\x81"

typedef struct
{
	uint8_t width;
	uint16_t rows[13];
} symbol_t;

typedef struct
{
	uint8_t height;
	uint8_t num_symbols;	//glyphs for characters ' ' .. ' '+num_symbols-1
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
static inline const symbol_t *fontGlyph(const font_t *f, char c)
{
	uint8_t i = (uint8_t)c - (uint8_t)' ';

	if ((uint8_t)c < (uint8_t)' ' || i >= f->num_symbols)
		i = '?' - ' ';

	return &f->symbol[i];
}

extern const font_t nokia_small;
extern const font_t nokia_small_bold;
extern const font_t nokia_big;

#endif
