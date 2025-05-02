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

cystatus Save16(uint16 data, uint16_t address)
{
    cystatus ret;
    // big endian
    ret = EEPROM_WriteByte(data >> 8, address);
    if (ret != CYRET_SUCCESS) {
        return ret;
    }
    return EEPROM_WriteByte(data & 0xff, address + 1);
}

uint16_t Load16(uint16 address)
{
    uint16_t temp;
    temp = EEPROM_ReadByte(address);
    uint16_t result = temp << 8;
    temp = EEPROM_ReadByte(address + 1);
    result += temp;
    return result;
}

/* [] END OF FILE */
