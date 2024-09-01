//
// Emulation of WDC 6522 VIA
// Bobbi Webber-Manners
// Sept 2024
//
// Emulation ONLY of the parts necessary for Mockingboard, which is basically
// input and output from Port A and Port B, timer 1 and timer 2.
// - CB1/CB2 are not connected, so no need to emulate the 6522 shift register,
//   which only connects to these pins. There is also no need to implement
//   read or write handshaking for port B.
// - Pins PB3-PB7 are not connected. Hence there is no need to support the 
//   timer 1 PB7 output mode or the timer 2 PB6 pulse counting mode.
// - CA2 is also not connected. (If an SSI-263 chip is installed the A/R'
//   output (pin 4) goes to CA1 to signal that the SSI is ready for another
//   phoneme.  Otherwise CA1 is also not connected.
// - In the first instance we will assume no SSI-263 speech chip, but this may
//   be added later.
// - So we don't need read or write handshaing on port A either for now, and
//   we will completely ignore VIAREG_PCR since no CA1/2,CB1/2. Will
//   need to implement the CA1 support when we add SSI capability later on.
// - 


#pragma once

#include <stdint.h>
#include <stdbool.h>

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

  uint8_t port_a; // Mockingboard: Connects to/from AY-3-8913 databus (DA0..DA7)
  uint8_t port_b; // Mockingboard: 3 bits to AY-3-8913 (PB0-BC1, PB1-BDIR, PB2-RESET)
  bool    ca1;    // Mockingboard: Only used if SSI-263 speech chip installed (A/R')
  bool    ca2;    // Mockingboard: Not used
  bool    cb1;    // Mockingboard: Not used
  bool    cb2;    // Mockingboard: Not used
  bool    irqb;   // Mockingboard: Connects to Apple II IRQ
  
} via_state;

via_state *create_via();
void destroy_via(via_state *h);

void via_set_register(via_state *h, unsigned int reg, uint8_t val);
uint8_t via_get_register(via_state *h, unsigned int reg);
void via_process(via_state *h);



