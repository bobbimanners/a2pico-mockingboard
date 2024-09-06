//
// Emulation of General Instruments AY-3-8913 Sound Chip
// Bobbi Webber-Manners
// Sept 2024
//

#include "ay-3-8913.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Prototypes for private functions
static void ay3_reset(ay3_state *h);
static void ay3_set_register(ay3_state *h, unsigned int reg, uint8_t val);
static uint8_t ay3_get_register(ay3_state *h, unsigned int reg);
static void ay3_process(ay3_state *h);
static void ay3_gen_noise(ay3_state *h);
static void ay3_gen_tone(ay3_state *h);
static void ay3_mix(ay3_state *h);
static void reset_envelope_generator(ay3_state *h);
static uint8_t envelope_generator(ay3_state *h);
static void ay3_envelope_ampl(ay3_state *h);
static void ay3_combine(ay3_state *h);


ay3_state *create_ay3() {
  ay3_state *h = malloc(sizeof(ay3_state));
  if (!h) {
    printf("Alloc fail!");
    exit(999);
  }
  for (unsigned int i = 0; i < 16; ++i) {
    h->regs[i] = 0;
  }
  srand(time(NULL));
  ay3_reset(h);
  return h;
}

void destroy_ay3(ay3_state *h) {
  free(h);
}

// Called every clock cycle
void ay3_clk(ay3_state *h, via_state *via) {
  
  // Mockingboard PCB wiring:
  // Port A of VIA is connected directly to AY3 databus D0..D7.
  // Port B of VIA is wired as follows:
  //  PB0 -> BC1
  //  PB1 -> BDIR
  //  PB2 -> RESET'
  //  (other bits unused)
  uint8_t bc1   = via->port_b & 0x01;
  uint8_t bdir  = via->port_b & 0x02;
  uint8_t reset = via->port_b & 0x04;

  //printf("ay3_clk: bc1=%x bdir=%x reset=%x\n", bc1, bdir, reset);
  if (reset == 0) {
    ay3_reset(h);
    return;
  }

  // AY3 Interface logic is as follows:
  //  BDIR BC1
  //  ---------------------------
  //  0    0     Inactive
  //  0    1     Read from AY3
  //  1    0     Write to AY3
  //  1    1     Latch register address
  if ((bdir == 0) && (bc1 != 0)) {
    // Read register
    via->port_a = ay3_get_register(h, h->selected);
  } else if ((bdir != 0) && (bc1 == 0)) {
    // Write register
    ay3_set_register(h, h->selected, via->port_a);
  } else if ((bdir != 0) && (bc1 != 0)) {
    // Latch register
    printf("AY3: Latching R%d\n", via->port_a);
    h->selected = via->port_a;
  }

  // Generate signal
  ay3_process(h);
}

static void ay3_reset(ay3_state *h) {
  printf("AY3: reset\n");
  h->selected = 0;
  h->idx = 0;
  for (unsigned int ch = 0; ch < 3; ++ch) {
    h->tone_state.period[ch]  = 4095;
    h->tone_state.counter[ch] = 1;
    h->tone_state.signal[ch]  = 0;
  }
  h->noise_state.period  = 31;
  h->noise_state.counter = 1;
  h->noise_state.signal  = 0;
  memset(h->output, 0, AY3_SAMPLES);
  reset_envelope_generator(h);
}

static void ay3_set_register(ay3_state *h, unsigned int reg, uint8_t val) {
  printf("AY3: Setting R%d to 0x%x\n", reg, val);
  h->regs[reg] = val;
  switch (reg) {
    case 0:
    case 1:
      // Changing period on channel A - calculate the updated period
      // and reset tone generator state
      h->tone_state.period[0] = h->regs[0] + ((h->regs[1] & 0x0f) << 8);
      h->tone_state.counter[0] = h->tone_state.period[0];
      break;
    case 2:
    case 3:
      // Changing period on channel B - calculate the updated period
      // and reset tone generator state
      h->tone_state.period[1] = h->regs[2] + ((h->regs[3] & 0x0f) << 8);
      h->tone_state.counter[1] = h->tone_state.period[1];
      break;
    case 4:
    case 5:
      // Changing period on channel C - calculate the updated period
      // and reset tone generator state
      h->tone_state.period[2] = h->regs[4] + ((h->regs[5] & 0x0f) << 8);
      h->tone_state.counter[2] = h->tone_state.period[2];
      break;
    case 6:
      // Changing period on noise generator - calculate the updated period
      h->noise_state.period = h->regs[6] & 0x1f;
      h->noise_state.counter = h->noise_state.period;
      break;
    case 15:
      // Write to R15 (Envelope Shape/Cycle) resets the envelope generator
      reset_envelope_generator(h);
      break;
  }
}

static uint8_t ay3_get_register(ay3_state *h, unsigned int reg) {
  return h->regs[reg];
}

static void ay3_process(ay3_state *h) {
  static unsigned int clkcounter = 0;

  // Tone and noise is generated every 16 clocks
  if ((++clkcounter % 16) == 0) {
    ay3_gen_tone(h);
    ay3_gen_noise(h);
    ay3_mix(h);
    ay3_envelope_ampl(h);
    ay3_combine(h);
    clkcounter = 0;
  }
}

// Three-channel squarewave generator, called every 16th clock
static void ay3_gen_tone(ay3_state *h) {

  for (unsigned int ch = 0; ch < 3; ++ch) {
    if (--h->tone_state.counter[ch] == 0) {
      h->tone_state.counter[ch] = h->tone_state.period[ch];
      h->tone_state.signal[ch] = ((h->tone_state.signal[ch] == 0) ? 1 : 0);
    }
  }
}

// Single channel PRNG noise generator, called every 16th clock
static void ay3_gen_noise(ay3_state *h) {
  if (--h->noise_state.counter == 0) {
    h->noise_state.counter = h->noise_state.period;
    h->noise_state.signal = rand() & 0x01;
  }
}

// Mix the three tone channels plus noise, called every 16th clock
static void ay3_mix(ay3_state *h) {

  unsigned int en = h->regs[7];
  unsigned int tone_en[]  = {(en & 0x01) >> 0, (en & 0x02) >> 1, (en & 0x04) >> 2};
  unsigned int noise_en[] = {(en & 0x08) >> 3, (en & 0x10) >> 4, (en & 0x20) >> 5};

  for (unsigned int ch = 0; ch < 3; ++ch) {
    unsigned int t_en = (tone_en[ch]  == 0 ? 1 : 0);
    unsigned int n_en = (noise_en[ch] == 0 ? 1 : 0);
    h->mixed[ch] = t_en * h->tone_state.signal[ch] + n_en * h->noise_state.signal;
  }
}

// Reset envelope generator state
static void reset_envelope_generator(ay3_state *h) {
  h->envelope_state.envelope_value = 0;
  h->envelope_state.remaining = 1;
  h->envelope_state.period_counter = 0;
}

// Generate amplitude envelope, called every 1/256th clock
static uint8_t envelope_generator(ay3_state *h) {

  // Period of envelope in terms of cycles of (CLOCKSPEED/256)
  unsigned int period = h->regs[11] + (h->regs[12] << 8);
  unsigned int shape  = h->regs[13] & 0x0f;

  // Decode the shape
  unsigned int env_continue  = (shape & 0x08) >> 3;
  unsigned int env_attack    = (shape & 0x04) >> 2;
  unsigned int env_alternate = (shape & 0x02) >> 1;
  unsigned int env_hold      = (shape & 0x01);

  if (--h->envelope_state.remaining == 0) {
    h->envelope_state.remaining = period + 1;
    ++h->envelope_state.period_counter;
  }

  // Divide the period up into 16 segments
  unsigned int step = (period + 1 - h->envelope_state.remaining) * 16 / (period + 1);

  if (h->envelope_state.period_counter == 1) {
    // Within the first period, the only param that matters is the attack
    // which has the effect of inverting the signal
    return (env_attack ? step : 15 - step);
  } else {
    // For subsequent periods 2, 3, 4 ...
    if (!env_continue) {
      // If continue is false, then value is zero after first period expires
      // regardless of the other flags
      return 0;
    } else {
      // We are continuing ...
      if (env_hold) {
        // Holding ...
        // Value goes high if either attack is true or alternate mode is set, but not both
        return (env_alternate ^ env_attack) * 15;
      } else {
        // Not holding ...
        if (!env_alternate) {
          // Not alternating, do the same thing as initial period, again and again
          return (env_attack ? step : 15 - step);
        } else {
          // Alternating, do the opposite thing each time
          return (((h->envelope_state.period_counter % 2 == 0) ^ env_attack) ? step : 15 - step);
        }
      }
    }
  }
}

// Scale by fixed amplitude or apply envelope, called every 1/16th clock
static void ay3_envelope_ampl(ay3_state *h) {
  static unsigned int callcounter = 0;

  // Amplitude mode (0 for fixed, 1 for envelope)
  unsigned int mode[] = {(h->regs[8]  & 0x10) >> 4,
                         (h->regs[9]  & 0x10) >> 4,
                         (h->regs[10] & 0x10) >> 4};

  // Fixed amplitude in range 0..15
  unsigned int ampl[] = {h->regs[8]  & 0x0f,
                         h->regs[9]  & 0x0f,
                         h->regs[10] & 0x0f};

  // Every 16 calls, update the envelope (16*16 = every 256 clks)
  if ((++callcounter % 16) == 0) {
    h->envelope_state.envelope_value = envelope_generator(h);
    callcounter = 0;
  }

  for (unsigned int ch = 0; ch < 3; ++ch) {
    if (mode[ch] == 0) {
      h->mixed[ch] *= ampl[ch];
    } else {
      h->mixed[ch] *= h->envelope_state.envelope_value;
    }
  }
}

// Output the combined signal to output[], called every 1/16th clock
static void ay3_combine(ay3_state *h) {
  h->output[h->idx++] = (h->mixed[0] + h->mixed[1] + h->mixed[2]) * 10;
  if (h->idx >= AY3_SAMPLES) {
    h->idx = 0;
  }
}



