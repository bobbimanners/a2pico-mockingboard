//
// Emulation of General Instruments AY-3-8913 Sound Chip
// Bobbi Webber-Manners
// Sept 2024
//

#pragma once

#include <stdint.h>
#include "wdc6522.h"

#define AY3_SAMPLES 1024
#define AY3_QUANTUM_MILLISECS 25
#define AY3_SAMPLERATE (AY3_SAMPLES * 40) // Works out to 40960Hz
#define CLOCKSPEED 1020500

// State of AY-3-8913
typedef struct {
  uint8_t regs[16];
  uint8_t selected; // Selected register

  // Buffers for three tone channels + noise
  uint8_t tone[3][AY3_SAMPLES];
  uint8_t noise[AY3_SAMPLES];

  // Output buffer
  uint8_t output[AY3_SAMPLES];

  // Interal state of tone generator
  struct {
    unsigned int counter[3];  // Will be reset on first use below
    unsigned int signal[3];   // Current signal state high or low
  } tone_state;

  // Interal state of noise generator
  struct {
    unsigned int counter;  // Will be reset on first use below
    unsigned int signal;   // Current signal state high or low
  } noise_state;

  // Internal state of envelope generator
  struct {
    unsigned int remaining;
    unsigned int period_counter;
  } envelope_state;
} ay3_state;

ay3_state *create_ay3();
void destroy_ay3(ay3_state *h);

void ay3_clk(ay3_state *h, via_state *via);
void ay3_set_register(ay3_state *h, unsigned int reg, uint8_t val);
uint8_t ay3_get_register(ay3_state *h, unsigned int reg);
void ay3_process(ay3_state *h);
void ay3_gen_noise(ay3_state *h);
void ay3_gen_tone(ay3_state *h);
void ay3_mix(ay3_state *h);
void reset_envelope_generator(ay3_state *h);
uint8_t envelope_generator(ay3_state *h, unsigned int shape, unsigned int period);
void ay3_envelope_ampl(ay3_state *h);
void ay3_combine(ay3_state *h);


