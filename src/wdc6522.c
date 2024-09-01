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
}




