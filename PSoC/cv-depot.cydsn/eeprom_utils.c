/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/

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

/* [] END OF FILE */
