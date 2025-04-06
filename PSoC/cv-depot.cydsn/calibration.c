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
#include "main.h"
#include "pot_change.h"

/**
 * Find bend width for a octave (i.e., 1V) manually.
 *
 * Strategy:
 * The user already know current CV output by a tester reading. The module runs binary search
 * targeting 1V above the current voltage. The module changes the voltage, then the user answers
 * "raise" or "lower" by turning the encoder. User would press button when the voltage reaches
 * the target, then this method exits.
 *
 */
static uint16_t FindBendOctaveWidth()
{
    QuadDec_SetCounter(0);    
    int16_t counter_last_value = 0;
    uint16_t bend_lower = bend_offset;  // we know the target is 1V higher than the starting point.
    uint16_t bend_upper = 32768;

    int16_t bend = bend_offset;
    uint8_t temp = EEPROM_ReadByte(ADDR_BEND_OCTAVE_WIDTH);
    bend += temp << 8;
    temp = EEPROM_ReadByte(ADDR_BEND_OCTAVE_WIDTH + 1);
    bend += temp;
    if (bend < bend_lower || bend > bend_upper) {
        // in case the stored data is coruppted and the bend gets out of range
        bend = (bend_lower + bend_upper) / 2;
    }
    
    PWM_Bend_WriteCompare(bend);
    
    // Run binary search. User would decide to go higher or lower.
    while (mode == MODE_CALIBRATION_BEND_WIDTH) {
        int16_t counter_value = QuadDec_GetCounter();
        if (counter_value == counter_last_value) {
            continue;
        }
        if (counter_value < counter_last_value) {
            bend_upper = bend;
            LED_Driver_WriteString7Seg("dwn", 0);
        } else if (counter_value > counter_last_value) {
            bend_lower = bend;
            LED_Driver_WriteString7Seg("up ", 0);
        }
        counter_last_value = counter_value;
        bend = (bend_lower + bend_upper) / 2;
        PWM_Bend_WriteCompare(bend);
    }
    LED_Driver_ClearDisplayAll();
    return bend - bend_offset;
}

#define MEASUREMENT_WAIT_MS 256
#define WAIT(x) CyDelay(MEASUREMENT_WAIT_MS)

uint16_t BendToReference(int16_t initial_bend)
{
    uint16_t bend_lower = 0;
    uint16_t bend_upper = 32768;
    uint16_t bend = initial_bend >= 0 ? initial_bend : (bend_lower + bend_upper) / 2;
    PWM_Bend_WriteCompare(bend);
    WAIT();
    uint8_t reading = Pin_Adjustment_In_Read();
    Pin_LED_Write(reading);
    do {
        bend = (bend_lower + bend_upper) / 2;
        PWM_Bend_WriteCompare(bend);
        WAIT();
        reading = Pin_Adjustment_In_Read();
        Pin_LED_Write(reading);
        if (reading) {
            bend_lower = bend;
        } else {
            bend_upper = bend;
        }
    } while (bend_upper - bend_lower > 1);
    return bend;
}

void ChangeWiper(pot_t *pot, uint16_t wiper)
{
    LED_Driver_Write7SegNumberDec(wiper, 1, 2, LED_Driver_RIGHT_ALIGN);
    PotSetTargetPosition(pot, wiper);
    while (!PotUpdate(pot)) {}
}

void SetUpOctaveMeasurement(uint16_t wiper, uint16_t bend_octave_width)
{
    ChangeWiper(&pot_note_1, wiper);
    PWM_Notes_WriteCompare1(36);
    bend_offset = BendToReference(-1);
    PWM_Bend_WriteCompare(bend_offset - bend_octave_width);  
    PWM_Notes_WriteCompare1(48);
}

void Calibrate()
{
    Pin_Portament_En_Write(0);
    mode = MODE_CALIBRATION_INIT;
    Pin_Encoder_LED_2_Write(1);
    
    LED_Driver_WriteString7Seg("___", 0);
    // set note C2
    PWM_Notes_WriteCompare1(12);
    // PWM_Notes_WriteCompare2(12);
    
    // set center bender position
    PWM_Bend_WriteCompare(bend_offset);
    
    // Halt while reading current value. User will proceed the mode when ready
    while (mode == MODE_CALIBRATION_INIT) {}
    
    LED_Driver_SetDisplayRAM(1, 0);
    LED_Driver_SetDisplayRAM(1, 1);
    LED_Driver_SetDisplayRAM(1, 2);
    
    // Find the bend width for a octave manually
    uint16_t bend_octave_width = FindBendOctaveWidth();
    
    // save the octave bend width to EEPROM, in big endian manner
    Save16(bend_octave_width, ADDR_BEND_OCTAVE_WIDTH);
    
    LED_Driver_WriteString7Seg("a", 0);
    
    Pin_Adj_S0_Write(0);
    Pin_Adj_En_Write(1);
    
    // LED_Driver_Write7SegNumberDec(pot_note_1.current, 0, 3, LED_Driver_RIGHT_ALIGN);
    uint8_t wiper_lowest = 0;
    uint8_t wiper_highest = 63;
    uint8_t wiper;
    uint8_t reading;
    do {
        wiper = (wiper_lowest + wiper_highest) / 2;
        SetUpOctaveMeasurement(wiper, bend_octave_width);
        WAIT();
        reading = Pin_Adjustment_In_Read();
        Pin_LED_Write(reading);
        if (reading) {
            wiper_lowest = wiper;
        } else {
            wiper_highest = wiper;
        }
    } while (wiper_highest - wiper_lowest > 1);
    
    Pin_Encoder_LED_1_Write(1);
    
    uint16_t adjusted_bend = BendToReference(-1);
    int16_t error1 = adjusted_bend - bend_offset + bend_octave_width;
    
    uint16_t another_wiper = wiper == wiper_highest ? wiper_lowest : wiper_highest;
    SetUpOctaveMeasurement(another_wiper, bend_octave_width);
    /*
    ChangeWiper(&pot_note_1, another_wiper);    
    PWM_Notes_WriteCompare1(36);
    bend_offset = BendToReference(-1);
    PWM_Bend_WriteCompare(bend_offset - bend_octave_width);  
    PWM_Notes_WriteCompare1(48);
    */
    adjusted_bend = BendToReference(-1);
    int16_t error2 = adjusted_bend - bend_offset + bend_octave_width;
        
    // compare the error and choose the closer one
    if (error1 * error1 < error2 * error2) {
        ChangeWiper(&pot_note_1, wiper);
    }
    
    PWM_Bend_WriteCompare(bend_offset);
    
    Pin_Encoder_LED_1_Write(0);
    Pin_Encoder_LED_2_Write(0);
}

/* [] END OF FILE */
