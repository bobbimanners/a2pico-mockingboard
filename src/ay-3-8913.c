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

ay3_state *create_ay3() {
  ay3_state *h = malloc(sizeof(ay3_state));
  if (!h) {
    printf("Alloc fail!");
    exit(999);
  }
  srand(time(NULL));
  for (unsigned int i = 0; i < 3; ++i) {
    h->tone_state.counter[i]  = 1;
    h->tone_state.signal[i]   = 0;
  }
  h->selected = 0;
  h->noise_state.counter = 1;
  h->noise_state.signal  = 1;
  memset(h->output, 0, AY3_SAMPLES);
  reset_envelope_generator(h);
  return h;
}

void destroy_ay3(ay3_state *h) {
  free(h);
}

void ay3_clk(ay3_state *h, via_state *via) {
  // Mockingboard PCB:
  // Port A of VIA is wired directly to AY3 databus D0..D7.
  // Port B of VIA is wired as follows:
  //  PB0 -> BC1
  //  PB1 -> BDIR
  //  PB2 -> RESET'
  uint8_t bc1   = via->port_b & 0x01;
  uint8_t bdir  = via->port_b & 0x02;
  uint8_t reset = via->port_b & 0x04;

  // AY3 Interface logic is as follows:
  //  BDIR BC1
  //  ---------------------------
  //  0    0     Inactive
  //  0    1     Read from AY3
  //  1    0     Write to AY3
  //  1    1     Latch register address
  if ((bdir == 0) && (bc1 != 0)) {
    // Read register
    uint8_t val = ay3_get_register(h, h->selected);
    // TODO: Do something with val!
  } else if ((bdir != 1) && (bc1 == 0)) {
    // Write register
    ay3_set_register(h, h->selected, via->port_a);
  } else {
    // Latch register
    h->selected = via->port_a;
  }
}

void ay3_set_register(ay3_state *h, unsigned int reg, uint8_t val) {
  h->regs[reg] = val;
  if (reg == 15) {
    // Write to R15 (Envelope Shape/Cycle) resets the envelope generator
    reset_envelope_generator(h);
  }
}

uint8_t ay3_get_register(ay3_state *h, unsigned int reg) {
  return h->regs[reg];
}

void ay3_process(ay3_state *h) {
  ay3_gen_tone(h);
  ay3_gen_noise(h);
  ay3_mix(h);
  ay3_envelope_ampl(h);
  ay3_combine(h);
}

// Three-channel squarewave generator
void ay3_gen_tone(ay3_state *h) {
  // Period of square wave to generate in terms of cycles of (CLOCKSPEED/16)
  unsigned int period[] = {h->regs[0] + ((h->regs[1] & 0x0f) << 8),
                           h->regs[2] + ((h->regs[3] & 0x0f) << 8),
                           h->regs[4] + ((h->regs[5] & 0x0f) << 8)};

  for (unsigned int ch = 0; ch < 3; ++ch) {
    // Period rescaled in terms of output AY3_SAMPLERATE (44.1kHz usually)
    unsigned int p = period[ch] * 16 * AY3_SAMPLERATE / CLOCKSPEED;
    for (unsigned int s = 0; s < AY3_SAMPLES; ++s) {
      if (--h->tone_state.counter[ch] == 0) {
        h->tone_state.counter[ch] = p;
        h->tone_state.signal[ch] = (h->tone_state.signal[ch] == 0 ? 1 : 0);
      }
      h->tone[ch][s] = h->tone_state.signal[ch];
    }
  }
}

// Single channel PRNG noise generator
void ay3_gen_noise(ay3_state *h) {
  // Period of noise wave to generate in terms of cycles of (CLOCKSPEED/16)
  unsigned int noise_period = h->regs[6] & 0x1f;

  // In terms of output AY3_SAMPLERATE
  unsigned int p = noise_period * 16 * AY3_SAMPLERATE / CLOCKSPEED + 1;

  for (unsigned int s = 0; s < AY3_SAMPLES; ++s) {
    if (--h->noise_state.counter == 0) {
      h->noise_state.counter = p;
      h->noise_state.signal = rand() & 0x01;
    }
    h->noise[s] = h->noise_state.signal;
  }
}

// Mix the three tone channels plus noise
void ay3_mix(ay3_state *h) {

  unsigned int en = h->regs[7];
  unsigned int tone_en[]  = {(en & 0x01) >> 0, (en & 0x02) > 1, (en & 0x04) > 2};
  unsigned int noise_en[] = {(en & 0x08) >> 3, (en & 0x10) > 4, (en & 0x20) > 5};

  for (unsigned int ch = 0; ch < 3; ++ch) {
    unsigned int t_en = (tone_en[ch]  == 0 ? 1 : 0);
    unsigned int n_en = (noise_en[ch] == 0 ? 1 : 0);
    for (unsigned int s = 0; s < AY3_SAMPLES; ++s) {
      h->tone[ch][s] = t_en * h->tone[ch][s] + n_en * h->noise[s];
    }
  }
}

// Reset envelope generator state
void reset_envelope_generator(ay3_state *h) {
  h->envelope_state.remaining = 1;
  h->envelope_state.period_counter = 0;
}

// Generate amplitude envelope
// Params: shape - encodes the shape of the envelope (continue/attack/alternate/hold)
//         period - envelope period in cycles of (CLOCKSPEED/256)
uint8_t envelope_generator(ay3_state *h, unsigned int shape, unsigned int period) {
  // Period rescaled in terms of output AY3_SAMPLERATE (44.1kHz usually)
  unsigned int p = (period + 1) * 256 * AY3_SAMPLERATE / CLOCKSPEED;

  // Decode the shape
  unsigned int env_continue  = (shape & 0x08) >> 3;
  unsigned int env_attack    = (shape & 0x04) >> 2;
  unsigned int env_alternate = (shape & 0x02) >> 1;
  unsigned int env_hold      = (shape & 0x01);

  if (--h->envelope_state.remaining == 0) {
    h->envelope_state.remaining = p;
    ++h->envelope_state.period_counter;
  }

  // Step 0..15 of the envelope pattern
  unsigned int step = (p - h->envelope_state.remaining) * 16 / p;

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
        return (env_alternate ^ env_attack);
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

// Scale by fixed amplitude or apply envelope
void ay3_envelope_ampl(ay3_state *h) {

  // Amplitude mode (0 for fixed, 1 for envelope)
  unsigned int mode[] = {(h->regs[8]  & 0x10) >> 4,
                         (h->regs[9]  & 0x10) >> 4,
                         (h->regs[10] & 0x10) >> 4};

  // Fixed amplitude in range 0..15
  unsigned int ampl[] = {h->regs[8]  & 0x0f,
                         h->regs[9]  & 0x0f,
                         h->regs[10] & 0x0f};

  // Period of envelope in terms of cycles of (CLOCKSPEED/256)
  unsigned int env_period = h->regs[11] + (h->regs[12] << 8);

  unsigned int env_shape  = h->regs[13] & 0x0f;

  for (unsigned int s = 0; s < AY3_SAMPLES; ++s) {
    uint8_t envelope_value = envelope_generator(h, env_shape, env_period);
    for (unsigned int ch = 0; ch < 3; ++ch) {
      if (mode[ch] == 0) {
        h->tone[ch][s] *= ampl[ch];
      } else {
        h->tone[ch][s] *= envelope_value;
      }
    }
  }
}

// Output the combined signal to output[]
void ay3_combine(ay3_state *h) {
  for (unsigned int s = 0; s < AY3_SAMPLES; ++s) {
    h->output[s] = (h->tone[0][s] + h->tone[1][s] + h->tone[2][s] + h->noise[s]) * 3;
  }
}



