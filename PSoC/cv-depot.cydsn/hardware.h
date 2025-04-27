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

/*
 * Place hardware control materials here.
 */

#pragma once

#include <stdint.h>

#define BEND_STEPS 1024

extern uint16_t bend_offset;

extern void InitializeVoiceControl();

extern void BlinkGreen(uint16_t interval_ms, uint16_t times);
extern void BlinkRed(uint16_t interval_ms, uint16_t times);

// macros
#define GET_GREEN_ENCODER_LED(x) Pin_Encoder_LED_1_Read()
#define SET_GREEN_ENCODER_LED(x) Pin_Encoder_LED_1_Write(x)
#define GREEN_ENCODER_LED_ON(x) SET_GREEN_ENCODER_LED(1)
#define GREEN_ENCODER_LED_OFF(x) SET_GREEN_ENCODER_LED(0)
#define GREEN_ENCODER_LED_TOGGLE(x) SET_GREEN_ENCODER_LED(!GET_GREEN_ENCODER_LED())

#define GET_RED_ENCODER_LED(x) Pin_Encoder_LED_2_Read()
#define SET_RED_ENCODER_LED(x) Pin_Encoder_LED_2_Write(x)
#define RED_ENCODER_LED_ON(x) SET_RED_ENCODER_LED(1)
#define RED_ENCODER_LED_OFF(x) SET_RED_ENCODER_LED(0)
#define RED_ENCODER_LED_TOGGLE(x) SET_RED_ENCODER_LED(!GET_RED_ENCODER_LED())

/* [] END OF FILE */
