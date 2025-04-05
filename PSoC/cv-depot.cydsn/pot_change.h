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

#ifndef POT_CHANGE_H
#define POT_CHANGE_H

#include "pot.h"

/**
 * Schedule a pot wiper change request.
 *
 * @param pot: pot_t - Pot to modify
 * @param wiper_position: int8_t - Wiper position. Set 0 to 63 to seek the position.
 *   Set -1 if current wiper position is lost and ensure to return to the terminal B.
 * @returns 0 if the request is scheduled successfully. Non-zero on error.
 */
extern uint8_t PotChangePlaceRequest(pot_t *pot, int8_t wiper_position);

extern void PotChangeHandleRequests();

#endif  // POT_CHANGE_H

/* [] END OF FILE */
