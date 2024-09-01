//
// Emulation of WDC 6522 VIA
// Bobbi Webber-Manners
// Sept 2024
//

#pragma once

#include <stdint.h>

// VIA Register names
#define VIAREG_ORB  0   // Output register B
#define VIAREG_IRB  0   // Input register B
#define VIAREG_ORA  1   // Output register A
#define VIAREG_IRA  1   // Input register A
#define VIAREG_DDRB 2   // Data direction register B
#define VIAREG_DDRA 3   // Data direction register B
#define VIAREG_T1CL 4   // Timer 1 low order latches / counter
#define VIAREG_T1CH 5   // Timer 1 high order counter
#define VIAREG_T1LL 6   // Timer 1 low order latches
#define VIAREG_T1LH 7   // Timer 1 high order latches
#define VIAREG_T2CL 8   // Timer 2 low order latches / counter
#define VIAREG_T2CH 9   // Timer 2 high order counter
#define VIAREG_SR   10  // Shift register
#define VIAREG_ACR  11  // Aux control reg
#define VIAREG_PCR  12  // Peripheral control reg
#define VIAREG_IFR  13  // Interrupt flag reg
#define VIAREG_IER  14  // Interrupt enable reg
#define VIAREG_ORA2 15  // Same as reg 1 except no 'handshake'
#define VIAREG_IRA2 15  // Same as reg 1 except no 'handshake'

// State of VIA
typedef struct {
  uint8_t regs[16];

  uint8_t port_a;
  uint8_t port_b;
} via_state;

via_state *create_via();
void destroy_via(via_state *h);

void via_set_register(via_state *h, unsigned int reg, uint8_t val);
uint8_t via_get_register(via_state *h, unsigned int reg);
void via_process(via_state *h);



