#ifndef INC_CHARSET_H_
#define INC_CHARSET_H_

#include <stdint.h>

//Internal text encoding: one byte per character, so that the display,
//word wrapping and text entry code can keep indexing plain char arrays.
//
//  0x20..0x7E  ASCII
//  0x80..0x9F  UI icons (fonts.h) - never produced from received text
//  0xA5..0xFF  Cyrillic, Windows-1251 code points:
//              0xC0..0xFF  A..ya (U+0410..U+044F)
//              0xA5 GHE WITH UPTURN, 0xAA/0xBA YE, 0xAF/0xBF YI, 0xB2/0xB3 I,
//              0xB4 ghe with upturn, 0xA8/0xB8 IO
//
//Over the air (M17 SMS payload) text is UTF-8.

#define CH_UKR_APOSTROPHE	'\''	//U+02BC and U+2019 are folded to ASCII

//UTF-8 -> internal; unsupported characters become '?', Latin letters with
//diacritics lose them, control characters become spaces
//returns the output length
uint16_t utf8ToLocal(char *dst, uint16_t dst_size, const char *src);

//internal -> UTF-8 (never splits a multi-byte sequence)
//returns the output length in bytes
uint16_t localToUtf8(char *dst, uint16_t dst_size, const char *src);

//uppercase form of a letter of the internal charset (other characters unchanged)
char localToUpper(char c);

#endif /* INC_CHARSET_H_ */
