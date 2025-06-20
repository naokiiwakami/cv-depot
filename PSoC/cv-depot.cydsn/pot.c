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

#include "project.h"
#include "pot.h"

// available pots - these are external and referred by the main program
pot_t pot_note_1;
pot_t pot_note_2;
pot_t pot_portament_1;
pot_t pot_portament_2;

void PotGlobalInit()
{
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
        return 1;
    }
    switch (pot->phase) {
    case PHASE_IDLE:
        pot->level = pot->target > pot->current ? 1 : 0;
        Pin_Pot_UD_Write(pot->level);
        pot->phase = PHASE_DIRECTION_SET;
        return 0;
    case PHASE_DIRECTION_SET:
        SelectDevice(pot->pot_id, 0);
        pot->phase = PHASE_TO_LOAD;
        return 0;
    case PHASE_TO_LOAD:
        pot->level ^= 1;
        Pin_Pot_UD_Write(pot->level);
        pot->phase = PHASE_TO_TRIGGER;
        return 0;
    case PHASE_TO_TRIGGER:
        pot->level ^= 1;
        Pin_Pot_UD_Write(pot->level);
        pot->current += pot->target > pot->current ? 1 : -1;
        pot->phase = PHASE_TO_LOAD;
        return 0;
    }
    return 1;
}

/* [] END OF FILE */
