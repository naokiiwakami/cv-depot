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
#ifndef CYAPICALLBACKS_H
#define CYAPICALLBACKS_H
    

    /*Define your macro callbacks here */
    /*For more information, refer to the Writing Code topic in the PSoC Creator Help.*/
    // #define CAN_MSG_RX_ISR_CALLBACK
    // extern void CAN_MsgRXIsr_Callback();
    #define CAN_RECEIVE_MSG_0_CALLBACK
    extern void CAN_ReceiveMsg_0_Callback();

#endif /* CYAPICALLBACKS_H */   
/* [] */
