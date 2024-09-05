//
// Emulation of WDC 6522 VIA
// Bobbi Webber-Manners
// Sept 2024
//

#include "wdc6522.h"
#include <stdio.h>
#include <stdlib.h>

// Prototypes for private functions
static void via_set_register(via_state *h, unsigned int reg, uint8_t val);
static uint8_t via_get_register(via_state *h, unsigned int reg);
static void via_write_port(uint8_t direction, uint8_t reg, uint8_t *port);
static void via_read_port(uint8_t direction, uint8_t *reg, uint8_t port);
static void via_timer1_expire(via_state *h);
static void via_timer2_expire(via_state *h);
static void via_interrupt();


via_state *create_via() {
  via_state *h = malloc(sizeof(via_state));
  if (!h) {
    printf("Alloc fail!");
    exit(999);
  }
  h->port_a = h->port_b = 0;
  h->regs[VIAREG_IER] = 128; // Disable all interrupts
  h->regs[VIAREG_IFR] = 0;   // Clear all interrupt flags
  h->regs[VIAREG_ACR] = 0;   // Clear Aux Control Register
  return h;
}

void destroy_via(via_state *h) {
  free(h);
}

void via_clk(via_state *h, bool cs1, bool cs2b, bool rwb, uint8_t rs, uint8_t data) {
  // Decrement timer 1
  if(--h->regs[VIAREG_T1CL] == 0xff) {
    --h->regs[VIAREG_T1CH];
  }
  // Check if expired
  if ((h->regs[VIAREG_T1CL] == 0) && (h->regs[VIAREG_T1CH] == 0)) {
    via_timer1_expire(h);
  }
  // Decrement timer 2
  if(--h->regs[VIAREG_T2CL] == 0xff) {
    --h->regs[VIAREG_T2CH];
  }
  // Check if expired
  if ((h->regs[VIAREG_T2CL] == 0) && (h->regs[VIAREG_T2CH] == 0)) {
    via_timer2_expire(h);
  }

  if (cs1 && !cs2b) {
    // Chip is selected ...
    if (rwb) {
      via_set_register(h, rs, data);
    } else {
      data = via_get_register(h, rs);
    }
  }
}

static void via_set_register(via_state *h, unsigned int reg, uint8_t val) {
  switch (reg) {
    case VIAREG_ORB:
      // CPU write to Port B. Update h->port_b.
      h->regs[reg] = val;
      via_write_port(h->regs[VIAREG_DDRB], h->regs[reg], &(h->port_b));
      break;
    case VIAREG_ORA:
      // CPU write to Port A. Update h->port_a.
      h->regs[reg] = val;
      via_write_port(h->regs[VIAREG_DDRA], h->regs[reg], &(h->port_a));
      break;
    case VIAREG_T1CL:
      // Timer 1 low order counter. Write to latch not counter.
      h->regs[VIAREG_T1LL] = val;
      break;
    case VIAREG_T1CH:
      // Timer 1 high order counter. Write to latch not counter.
      h->regs[VIAREG_T1LH] = val;
      // Then copy latch->counter
      h->regs[VIAREG_T1CL] = h->regs[VIAREG_T1LL];
      h->regs[VIAREG_T1CH] = h->regs[VIAREG_T1LH];
      // And reset timer 1 interrupt flag
      h->regs[VIAREG_IFR] &= 0xbf; // Turn off bit 6
      // Clear bit 7 if no other outstanding interrupts
      if ((h->regs[VIAREG_IFR] & 0x7f) == 0) {
        h->regs[VIAREG_IFR] = 0;
      }
      break;
    case VIAREG_T2CH:
      // Timer 2 high order counter
      h->regs[reg] = val;
      // And reset timer 2 interrupt flag
      h->regs[VIAREG_IFR] &= 0xdf; // Turn off bit 5
      // Clear bit 7 if no other outstanding interrupts
      if ((h->regs[VIAREG_IFR] & 0x7f) == 0) {
        h->regs[VIAREG_IFR] = 0;
      }
      break;
    case VIAREG_IER:
      // When writing to the interrupt enable register, bit 7 controls
      // whether the other bits mean set or clear.
      if (val < 128) {
        // Bit seven clear. Flip all bits.
        val = ~val;
      }
      h->regs[reg] = val;
    default:
      printf("VIA: setting R%d to 0x%x\n", reg, val);
      h->regs[reg] = val;
  }
}

static uint8_t via_get_register(via_state *h, unsigned int reg) {
  switch (reg) {
    case VIAREG_IRB:
      // CPU read from Port B. Update VIAREG_IRB.
      via_read_port(h->regs[VIAREG_DDRB], &(h->regs[reg]), h->port_b);
      break;
    case VIAREG_IRA:
      // CPU read from Port A. Update VIAREG_IRA.
      via_read_port(h->regs[VIAREG_DDRA], &(h->regs[reg]), h->port_a);
      break;
    case VIAREG_T1CL:
      // Timer 1 low order counter. Reset T1 interrupt flag.
      h->regs[VIAREG_IFR] &= 0xbf; // Turn off bit 6
      // Clear bit 7 if no other outstanding interrupts
      if ((h->regs[VIAREG_IFR] & 0x7f) == 0) {
        h->regs[VIAREG_IFR] = 0;
      }
      break;
    case VIAREG_T2CL:
      // Timer 2 low order counter. Reset T2 interrupt flag.
      h->regs[VIAREG_IFR] &= 0xdf; // Turn off bit 5
      // Clear bit 7 if no other outstanding interrupts
      if ((h->regs[VIAREG_IFR] & 0x7f) == 0) {
        h->regs[VIAREG_IFR] = 0;
      }
      break;
  }
  return h->regs[reg];
}

// Handle CPU writing to Port A or Port B
// Params: direction - direction register [IN]
//         reg - register value to output [IN]
//         port - pointer to output port to update [OUT]
static void via_write_port(uint8_t direction, uint8_t reg, uint8_t *port) {
  *port = (reg & direction) | (*port & ~direction);
}

// Handle CPU reading from Port A or Port B
// Params: direction - direction register [IN]
//         reg - pointer to register to update [OUT]
//         port - output port [IN]
static void via_read_port(uint8_t direction, uint8_t *reg, uint8_t port) {
  *reg = (port & ~direction) | (*reg & direction);
}

// Called when timer 1 expires
// Handles one-shot and continuous mode
static void via_timer1_expire(via_state *h) {

  // Bit 6 of the Aux Control Register determines mode
  // If we are in continuous mode, rearm the timer
  if (h->regs[VIAREG_ACR] & 0x40) {
    // Copy latch->counter
    h->regs[VIAREG_T1CL] = h->regs[VIAREG_T1LL];
    h->regs[VIAREG_T1CH] = h->regs[VIAREG_T1LH];
  }

  // If we are in continuous mode, OR if the Timer 1 interrupt flag has not yet been asserted
  if ((h->regs[VIAREG_ACR] & 0x40) || ((h->regs[VIAREG_IFR] & 0x40) == 0)) {
    // Set Timer 1 interrupt flag.
    h->regs[VIAREG_IFR] |= 0x40; // Turn on bit 6
    h->regs[VIAREG_IFR] |= 0x80; // Turn on bit 7 (any interrupt)
    // If Interrupt Enable Register Bit 6 is set, then assert the interrupt
    if (h->regs[VIAREG_IER] & 0x40) {
      via_interrupt();
    }
  }
}

// Called when timer 2 expires
// NOTE: We do not support pulse-counting mode, because PB6 is not utilized.
//       So there is only one-shot mode to consider for timer 2.
static void via_timer2_expire(via_state *h) {
  // If the Timer 2 interrupt flag is not asserted yet
  if ((h->regs[VIAREG_IFR] & 0x20) == 0) {
    // Set Timer 2 interrupt flag.
    h->regs[VIAREG_IFR] |= 0x20; // Turn on bit 5
    h->regs[VIAREG_IFR] |= 0x80; // Turn on bit 7 (any interrupt)
    // If Interrupt Enable Register Bit 5 is set, then assert the interrupt
    if (h->regs[VIAREG_IER] & 0x20) {
      via_interrupt();
    }
  }
}

// Called when the VIA wants to raise an IRQ
static void via_interrupt() {
  // TODO: Wire this up
}


