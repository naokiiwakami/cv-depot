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

#include "pot.h"
#include "pot_change.h"

typedef struct pot_change_request {
    uint8_t requested;
    int8_t target;  // set -1 to ensure to move to the terminal B
    pot_t *pot;
} pot_change_request_t;

// Request queue
#define REQUEST_QUEUE_SIZE 8
static pot_change_request_t pot_change_requests[REQUEST_QUEUE_SIZE];    
static uint8_t request_head = 0;
static uint8_t request_tail = 0;
static uint8_t pot_queue_overflow = 0;


uint8_t PotChangePlaceRequest(pot_t *pot, int8_t wiper_position)
{
    if (pot_queue_overflow) {
        return 1;
    }
    pot_change_request_t *request_item = &pot_change_requests[request_tail];
    request_tail = (request_tail + 1) % REQUEST_QUEUE_SIZE;
    if (request_head == request_tail) {
        pot_queue_overflow = 1;
    }
    
    request_item->requested = 0;
    request_item->pot = pot;
    request_item->target = wiper_position;

    return 0;
}

void PotChangeHandleRequests()
{
    if (request_head == request_tail && !pot_queue_overflow) {
        // nothing to handle
        return;
    }

    pot_change_request_t *item = &pot_change_requests[request_head];
    pot_t *pot = item->pot;

    if (!item->requested) {
        if (item->target < 0) {
            PotEnsureToMoveToB(pot);
        } else {
            PotSetTargetPosition(pot, item->target);
        }
        item->requested = 1;
    }
    
    if (PotUpdate(pot)) {
        request_head = (request_head + 1) % REQUEST_QUEUE_SIZE;
        pot_queue_overflow = 0;
    }
}

/* [] END OF FILE */
