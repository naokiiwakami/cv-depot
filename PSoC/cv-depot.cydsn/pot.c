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
#include "pot.h"

// available pots - these are external and referred by the main program
pot_t pot_note_1;
pot_t pot_note_2;
pot_t pot_portament_1;
pot_t pot_portament_2;

void PotGlobalInit()
{
    CySysTickInit();
    CySysTickDisableInterrupt();
    CySysTickSetReload(0xffffff); // maximum
    CySysTickSetClockSource(CY_SYS_SYST_CSR_CLK_SRC_LFCLK); // 100 kHz
    CySysTickClear();
    
    Pin_Pot_UD_Write(0);
    PotInit(&pot_note_1, POT_NOTE_1);
    PotInit(&pot_note_2, POT_NOTE_2);
    PotInit(&pot_portament_1, POT_PORTAMENT_1);
    PotInit(&pot_portament_2, POT_PORTAMENT_2);
}

static void SelectDevice(enum PotId pot_id, uint8_t value)
{
    switch (pot_id) {
    case POT_NOTE_1:
        Pin_Pot_Select_Note_1_Write(value);
        break;
    case POT_NOTE_2:
        Pin_Pot_Select_Note_2_Write(value);
        break;
    case POT_PORTAMENT_1:
        Pin_Pot_Select_Portament_1_Write(value);
        break;
    case POT_PORTAMENT_2:
        Pin_Pot_Select_Portament_2_Write(value);
        break;
    }
}

void PotInit(pot_t *pot, enum PotId pot_id)
{
    // Set initial wiper position.
    // When the device powers up, the default wiper position is 1Fh.
    pot->current = 0x1f;
    pot->target = 0x1f;
    
    // Lift up the CS pin (disable)
    pot-> pot_id = pot_id;
    SelectDevice(pot_id, 1);
    
    pot->phase = PHASE_IDLE;
    pot->last_checkpoint = 0;
}

void PotSetTargetPosition(pot_t *pot, int8_t target)
{
    if (target >= 0x40) {
        target = 0x3f;
    }
    if (target < 0) {
        target = 0;
    }   
    pot->target = target;
}

void PotEnsureToMoveToB(pot_t *pot)
{
    pot->current = 63;
    pot->target = 0;
}

uint8_t PotUpdate(pot_t *pot)
{
    if (pot->current == pot->target) {
        SelectDevice(pot->pot_id, 1);
        pot->phase = PHASE_IDLE;
        CySysTickStop();
        return 1;
    }
    if (pot->phase == PHASE_IDLE) {
        CySysTickClear();
        CySysTickEnable();
        Pin_Pot_UD_Write(pot->target > pot->current ? 1 : 0);
        pot->phase = PHASE_DIRECTION_SET;
        pot->last_checkpoint = CySysTickGetValue();
        return 0;
    }
    if (CySysTickGetValue() >= pot->last_checkpoint - 1) {
        return 0;
    }
    switch (pot->phase) {
    case PHASE_DIRECTION_SET:
        SelectDevice(pot->pot_id, 0);
        pot->phase = PHASE_CHIP_SELECTED;
        pot->last_checkpoint = CySysTickGetValue();        
        return 0;
    case PHASE_CHIP_SELECTED:
        /*
        if (Pin_Pot_UD_Read()) {
            // don't change the phase, just drop the level
            Pin_Pot_UD_Write(0);
            return 0;
        }
        */
        Pin_Pot_UD_Write(!Pin_Pot_UD_Read());
        pot->last_checkpoint = CySysTickGetValue();        
        pot->phase = PHASE_READY;
        return 0;
    case PHASE_READY:
        if (!Pin_Pot_UD_Read()) {
            Pin_Pot_UD_Write(1);
            pot->current += pot->target > pot->current ? 1 : 0;
        } else {
            Pin_Pot_UD_Write(0);
            pot->current += pot->target > pot->current ? 0 : -1;
        }
        pot->last_checkpoint = CySysTickGetValue();        
        return 0;
    }
    return 1;
}

/* [] END OF FILE */
