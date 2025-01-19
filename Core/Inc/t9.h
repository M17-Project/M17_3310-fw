//--------------------------------------------------------------------
// T9 text entry - t9.h
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 6 November 2024
//--------------------------------------------------------------------
#pragma once
#include <stdint.h>
#include <string.h>

uint8_t getDigit(const char c);
char* getWord(char *dict, char *code);
