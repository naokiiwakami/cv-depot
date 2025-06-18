/*
 * MIT License
 *
 * Copyright (c) 2025 Naoki Iwakami
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>

#include "project.h"

#include "eeprom.h"

uint8_t ReadEepromWithValueCheck(uint16 address, uint8_t max)
{
    uint8_t value = EEPROM_ReadByte(address);
    if (value >= max) {
        return 0;
    }
    return value;
}

cystatus Save16(uint16_t data, uint16_t address)
{
    EEPROM_UpdateTemperature();
    cystatus ret;
    // big endian
    ret = EEPROM_WriteByte(data >> 8, address);
    if (ret != CYRET_SUCCESS) {
        return ret;
    }
    return EEPROM_WriteByte(data & 0xff, address + 1);
}

uint16_t Load16(uint16_t address)
{
    uint16_t temp;
    temp = EEPROM_ReadByte(address);
    uint16_t result = temp << 8;
    temp = EEPROM_ReadByte(address + 1);
    result += temp;
    return result;
}

cystatus Save32(uint32_t data, uint16_t address)
{
    EEPROM_UpdateTemperature();
    cystatus ret;
    // big endian
    for (int i = 0; i < 4; ++i) {
        ret = EEPROM_WriteByte(data >> (24 - i * 8), address + i);
        if (ret != CYRET_SUCCESS) {
            return ret;
        }
    }
    return ret;
}

uint32_t Load32(uint16_t address)
{
    uint32_t result = 0;
    for (int i = 0; i < 4; ++i) {
        result <<= 8;
        result += EEPROM_ReadByte(address + i);
    }
    return result;
}

#define MIN(x, y) ((x) < (y) ? (x) : (y))

void SaveString(const char *string, size_t max_length, uint16_t address)
{
    size_t length = MIN(strlen(string), max_length - 1);
    EEPROM_UpdateTemperature();
    while (CYRET_LOCKED == EEPROM_WriteByte((uint8_t) (length & 0xff), address)) {}
    ++address;
    for (size_t i = 0; i < length; ++i) {
        while (CYRET_LOCKED == EEPROM_WriteByte((uint8_t)string[i], address)) {}
        ++address;
    }
}

void LoadString(char *string, size_t max_length, uint16_t address)
{
    size_t length = MIN(EEPROM_ReadByte(address), max_length - 1);
    ++address;
    for (size_t i = 0; i < length; ++i) {
        string[i] = (char)EEPROM_ReadByte(address + i);
    }
    string[length] = 0;
}

/* [] END OF FILE */
