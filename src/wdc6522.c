//
// Emulation of WDC 6522 VIA
// Bobbi Webber-Manners
// Sept 2024
//

#include "wdc6522.h"
#include <stdio.h>
#include <stdlib.h>

via_state *create_via() {
  via_state *h = malloc(sizeof(via_state));
  if (!h) {
    printf("Alloc fail!");
    exit(999);
  }
  h->port_a = h->port_b = 0;
  return h;
}

void destroy_via(via_state *h) {
  free(h);
}

void via_set_register(via_state *h, unsigned int reg, uint8_t val) {
  h->regs[reg] = val;
}

uint8_t via_get_register(via_state *h, unsigned int reg) {
  return h->regs[reg];
}

void via_process(via_state *h) {

  // Handle I/O for Port A
  uint8_t direction = h->regs[VIAREG_DDRA]; // Data direction register
  uint8_t reg       = h->regs[VIAREG_ORA];  // ORA/IRA are same register
  for (unsigned int i = 0; i < 8; ++i) {
    if (direction & 0x01) {
      // Output pin
      if (reg & 0x01) {
        h->port_a |= 0x01;
      } else {
        h->port_a &= 0xfe;
      }
    } else {
      // Input pin
      if (h->port_a & 0x01) {
        reg |= 0x01;
      } else {
        reg &= 0xfe;
      }
    }
    direction >>= 2;
    reg       >>= 2;
    h->port_a >>= 2;
  }
  h->regs[VIAREG_ORA] = reg;  

  // Handle I/O for Port B
  direction = h->regs[VIAREG_DDRB]; // Data direction register
  reg       = h->regs[VIAREG_ORB];  // ORB/IRB are same register
  for (unsigned int i = 0; i < 8; ++i) {
    if (direction & 0x01) {
      // Output pin
      if (reg & 0x01) {
        h->port_b |= 0x01;
      } else {
        h->port_b &= 0xfe;
      }
    } else {
      // Input pin
      if (h->port_b & 0x01) {
        reg |= 0x01;
      } else {
        reg &= 0xfe;
      }
    }
    direction >>= 2;
    reg       >>= 2;
    h->port_b >>= 2;
  }  
  h->regs[VIAREG_ORB] = reg;  

}




