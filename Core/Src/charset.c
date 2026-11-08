#include "charset.h"
#include <stddef.h>

//Latin-1 Supplement and Latin Extended-A (U+00C0..U+017F) without diacritics
static const char latin_fold[192+1] =
	"AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty"
	"AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiIiJjKkkLlLlLlLlLlNnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";

//Cyrillic letters outside U+0410..U+044F
static const struct
{
	uint16_t cp;
	uint8_t local;
} cyr_extra[] =
{
	{0x0401, 0xA8}, {0x0451, 0xB8},		//IO, io
	{0x0404, 0xAA}, {0x0454, 0xBA},		//Ukrainian YE, ye
	{0x0406, 0xB2}, {0x0456, 0xB3},		//Ukrainian I, i
	{0x0407, 0xAF}, {0x0457, 0xBF},		//YI, yi
	{0x0490, 0xA5}, {0x0491, 0xB4},		//GHE WITH UPTURN, ghe with upturn
};

//code point -> internal character (0: drop it)
static char mapCodePoint(uint32_t cp)
{
	if (cp >= 0x20 && cp <= 0x7E)
		return (char)cp;

	if (cp == '\t' || cp == '\n' || cp == '\r' || cp == 0xA0)
		return ' ';

	if (cp < 0x20 || cp == 0x7F)
		return 0;

	if (cp >= 0x410 && cp <= 0x44F)
		return (char)(0xC0 + (cp - 0x410));

	for (uint8_t i=0; i<sizeof(cyr_extra)/sizeof(cyr_extra[0]); i++)
		if (cyr_extra[i].cp == cp)
			return (char)cyr_extra[i].local;

	if (cp >= 0xC0 && cp <= 0x17F)
		return latin_fold[cp - 0xC0];

	switch (cp)
	{
		case 0x02BC:	//modifier letter apostrophe (Ukrainian)
		case 0x2018:
		case 0x2019:
		case 0x201A:
		case 0x2032:
			return CH_UKR_APOSTROPHE;

		case 0x00AB:
		case 0x00BB:
		case 0x201C:
		case 0x201D:
		case 0x201E:
			return '"';

		case 0x2010:
		case 0x2011:
		case 0x2012:
		case 0x2013:
		case 0x2014:
		case 0x2015:
		case 0x2212:
			return '-';

		case 0x2022:
			return '*';

		case 0x2116:	//numero sign
			return '#';

		default:
			return '?';
	}
}

uint16_t utf8ToLocal(char *dst, uint16_t dst_size, const char *src)
{
	const uint8_t *s = (const uint8_t*)src;
	uint16_t n = 0;

	if (dst_size == 0)
		return 0;

	while (*s && n < dst_size - 1)
	{
		uint32_t cp;
		uint8_t extra;
		uint8_t c = *s++;

		if (c < 0x80)
		{
			cp = c;
			extra = 0;
		}
		else if ((c & 0xE0) == 0xC0)
		{
			cp = c & 0x1F;
			extra = 1;
		}
		else if ((c & 0xF0) == 0xE0)
		{
			cp = c & 0x0F;
			extra = 2;
		}
		else if ((c & 0xF8) == 0xF0)
		{
			cp = c & 0x07;
			extra = 3;
		}
		else //stray continuation byte or invalid lead byte
		{
			dst[n++] = '?';
			continue;
		}

		uint8_t ok = 1;
		for (uint8_t i=0; i<extra; i++)
		{
			if ((*s & 0xC0) != 0x80) //truncated sequence - do not eat the next character
			{
				ok = 0;
				break;
			}
			cp = (cp << 6) | (*s++ & 0x3F);
		}

		char out = ok ? mapCodePoint(cp) : '?';

		//"..." for the ellipsis, the rest is 1:1
		if (ok && cp == 0x2026)
		{
			for (uint8_t i=0; i<3 && n < dst_size - 1; i++)
				dst[n++] = '.';
			continue;
		}

		if (out)
			dst[n++] = out;
	}

	dst[n] = 0;
	return n;
}

uint16_t localToUtf8(char *dst, uint16_t dst_size, const char *src)
{
	const uint8_t *s = (const uint8_t*)src;
	uint16_t n = 0;

	if (dst_size == 0)
		return 0;

	for (; *s; s++)
	{
		uint8_t c = *s;
		uint16_t cp = 0;

		if (c < 0x80)
		{
			if (n + 1 >= dst_size)
				break;
			dst[n++] = (char)c;
			continue;
		}

		if (c >= 0xC0)
		{
			cp = 0x410 + (c - 0xC0);
		}
		else
		{
			for (uint8_t i=0; i<sizeof(cyr_extra)/sizeof(cyr_extra[0]); i++)
				if (cyr_extra[i].local == c)
					cp = cyr_extra[i].cp;
		}

		if (cp == 0) //icons and unassigned codes
		{
			if (n + 1 >= dst_size)
				break;
			dst[n++] = '?';
			continue;
		}

		//all supported code points are in U+0080..U+07FF: 2 bytes
		if (n + 2 >= dst_size)
			break;
		dst[n++] = (char)(0xC0 | (cp >> 6));
		dst[n++] = (char)(0x80 | (cp & 0x3F));
	}

	dst[n] = 0;
	return n;
}

char localToUpper(char c)
{
	uint8_t u = (uint8_t)c;

	if (u >= 'a' && u <= 'z')
		return (char)(u - 'a' + 'A');
	if (u >= 0xE0)
		return (char)(u - 0x20);

	switch (u)
	{
		case 0xB8: return (char)0xA8;
		case 0xBA: return (char)0xAA;
		case 0xB3: return (char)0xB2;
		case 0xBF: return (char)0xAF;
		case 0xB4: return (char)0xA5;
		default:   return c;
	}
}
