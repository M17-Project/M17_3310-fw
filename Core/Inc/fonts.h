#ifndef INC_FONTS_H
#define INC_FONTS_H

#include <stdint.h>

typedef struct
{
	uint8_t width;
	uint16_t rows[13];
} symbol_t;

typedef struct
{
	uint8_t height;
	symbol_t symbol[];
} font_t;

typedef enum
{
	ALIGN_LEFT,
	ALIGN_CENTER,
	ALIGN_RIGHT,
	ALIGN_ARB
} align_t;

extern const font_t nokia_small;
extern const font_t nokia_small_bold;
extern const font_t nokia_big;

#endif
