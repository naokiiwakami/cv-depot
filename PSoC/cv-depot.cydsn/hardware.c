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
#include "pot.h"
#include "pot_change.h"
#include "voice.h"

uint16_t bend_offset;

// macros
#define SetNote1 PWM_Notes_WriteCompare1
#define SetNote2 PWM_Notes_WriteCompare2

#define VELOCITY_DAC_VALUE(velocity) (((velocity) * (velocity)) >> 3)
#define INDICATOR_VALUE(velocity) ((velocity) >= 64 ? (((velocity) - 63)  * ((velocity) - 63)) / 32 - 1 : 0)
#define NOTE_PWM_MAX_VALUE 120

static void Gate1On(uint8_t velocity)
{
    DVDAC_Velocity_1_SetValue(VELOCITY_DAC_VALUE(velocity));
    PWM_Indicators_WriteCompare1(INDICATOR_VALUE(velocity));
    Pin_Gate_1_Write(1);
}

static void Gate1Off()
{
    Pin_Portament_En_Write(0);
    DVDAC_Velocity_1_SetValue(0);
    Pin_Gate_1_Write(0);
}

static void Gate2On(uint8_t velocity)
{
    DVDAC_Velocity_2_SetValue(VELOCITY_DAC_VALUE(velocity));
    PWM_Indicators_WriteCompare2(INDICATOR_VALUE(velocity));
    Pin_Gate_2_Write(1);
}

static void Gate2Off()
{
    DVDAC_Velocity_2_SetValue(0);
    Pin_Gate_2_Write(0);
}

void GetVoiceConfigs(voice_config_t voice_configs[], unsigned size)
{
    if (size < 1) {
        return;
    }
    voice_configs[0].set_note = SetNote1;
    voice_configs[0].gate_on = Gate1On;
    voice_configs[0].gate_off = Gate1Off;
    
    if (size < 2) {
        return;
    }
    voice_configs[1].set_note = SetNote2;
    voice_configs[1].gate_on = Gate2On;
    voice_configs[1].gate_off = Gate2Off;
}

void InitializeVoiceControl()
{
    // setup hardware
    Pin_Portament_En_Write(0);
    Pin_Adj_En_Write(0);
    Pin_Adj_S0_Write(0);
    PWM_Notes_Start();
    PWM_Bend_Start();
    PWM_Indicators_Start();
    DVDAC_Velocity_1_Start();
    DVDAC_Velocity_2_Start();
    DVDAC_Expression_Start();
    DVDAC_Modulation_Start();

        // Note CV
    uint8_t wiper = EEPROM_ReadByte(ADDR_NOTE_1_WIPER);
    PotChangePlaceRequest(&pot_note_1, -1);  // move to termianl B to ensure the starting position
    PotChangePlaceRequest(&pot_note_1, wiper);
    
    wiper = EEPROM_ReadByte(ADDR_NOTE_2_WIPER);
    PotChangePlaceRequest(&pot_note_2, -1);  // move to termianl B to ensure the starting position
    PotChangePlaceRequest(&pot_note_2, wiper);

    /*
    uint8_t temp = EEPROM_ReadByte(ADDR_BEND_OFFSET);
    if (temp == 0xd1) {
        temp = EEPROM_ReadByte(ADDR_BEND_OFFSET + 1);
        bend_offset = temp << 8;
        temp = EEPROM_ReadByte(ADDR_BEND_OFFSET + 2);
        bend_offset += temp;
    } else { // initial value
        bend_offset = BEND_STEPS / 2;
    }
    */
    // bend_offset = BEND_STEPS / 2;
    
    // Gates
    // Gate1Off();
    // Gate2Off();
    
    // Bend
    // BendPitch(0x00, PITCH_BEND_CENTER);  // set neutral

    // Portament
    Pin_Portament_En_Write(0);
    // move pot terminals to B to ensure the starting positions
    PotChangePlaceRequest(&pot_portament_1, -1);
    PotChangePlaceRequest(&pot_portament_2, -1);
    // then set the pot values
    PotChangePlaceRequest(&pot_portament_1, 2);
    PotChangePlaceRequest(&pot_portament_2, 2);

}

/* [] END OF FILE */
