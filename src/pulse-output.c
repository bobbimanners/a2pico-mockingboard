/***
  Based on pulse-simple.c
***/
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
 
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
 
#include <pulse/simple.h>
#include <pulse/error.h>

#include "ay-3-8913.h"
 
#define BUFSIZE 1024

int main(int argc, char*argv[]) {

    /* The Sample format to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_U8,
        .rate = AY3_SAMPLERATE,
        .channels = 1
    };
 
    via_state *via1 = create_via();
    ay3_state *ay3_1= create_ay3();

    // Set up VIA for output
    via_clk(via1, true, false, true, VIAREG_DDRA, 0xff);
    via_clk(via1, true, false, true, VIAREG_DDRB, 0xff);

    // Load AY-3-8913 registers using the VIA 6522
    uint8_t regvals[] = {64, 0,  // Tone A period (fine, coarse)
                         0, 1,   // Tone B period (fine, coarse)
                         0, 4,   // Tone C period (fine, coarse)
                         30,     // Noise period
                         0xf8,   // Mixer enable
                         15,     // Volume A
                         15,     // Volume B
                         15,     // Volume C
                         0, 0,   // Envelope period (fine, coarse)
                         0,      // Envelope shape
                         0,      // I/O Port A data - not used
                         0       // I/O Port B data - not used
                         };

    uint8_t regvals2[] = {64, 0,  // Tone A period (fine, coarse)
                         0, 1,   // Tone B period (fine, coarse)
                         0, 4,   // Tone C period (fine, coarse)
                         30,     // Noise period
                         0xf8,   // Mixer enable
                         0,     // Volume A
                         16,     // Volume B
                         0,     // Volume C
                         0, 6,   // Envelope period (fine, coarse)
                         0b1110,      // Envelope shape
                         0,      // I/O Port A data - not used
                         0       // I/O Port B data - not used
                         };

    for (uint8_t rs = 0; rs < 16; ++rs) {
      //            cs1   cs2b   rwb   rs          data
      via_clk(via1, true, false, true, VIAREG_ORB, 0b100); // AY3 inactive
      ay3_clk(ay3_1, via1);
      via_clk(via1, true, false, true, VIAREG_ORA, rs);    // Register number
      ay3_clk(ay3_1, via1);
      via_clk(via1, true, false, true, VIAREG_ORB, 0b111); // Latch register
      ay3_clk(ay3_1, via1);
      via_clk(via1, true, false, true, VIAREG_ORB, 0b100); // AY3 inactive
      ay3_clk(ay3_1, via1);
      via_clk(via1, true, false, true, VIAREG_ORA, regvals2[rs]); // Register data
      ay3_clk(ay3_1, via1);
      via_clk(via1, true, false, true, VIAREG_ORB, 0b110); // Write register
      ay3_clk(ay3_1, via1);
    }

    pa_simple *s = NULL;
    int ret = 1;
    int error;
 
    /* Create a new playback stream */
    if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        goto finish;
    }

    for (;;) {
#if 0
        pa_usec_t latency;
 
        if ((latency = pa_simple_get_latency(s, &error)) == (pa_usec_t) -1) {
            fprintf(stderr, __FILE__": pa_simple_get_latency() failed: %s\n", pa_strerror(error));
            goto finish;
        }
 
        fprintf(stderr, "%0.0f usec    \r", (float)latency);
#endif

        for (unsigned int i = 0; i < 16 * AY3_SAMPLES; ++i) {
          /* Crank the handle */
          //            cs1   cs2b   rwb   rs  data
          via_clk(via1, true, false, true, 0, 0b100); // AY3 inactive
          ay3_clk(ay3_1, via1);
        }
 
        /* ... and play it */
        if (pa_simple_write(s, ay3_1->output, (size_t) AY3_SAMPLES, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
            goto finish;
        }
    }
 
    /* Make sure that every single sample was played */
    if (pa_simple_drain(s, &error) < 0) {
        fprintf(stderr, __FILE__": pa_simple_drain() failed: %s\n", pa_strerror(error));
        goto finish;
    }
 
    ret = 0;
 
finish:
 
    if (s)
        pa_simple_free(s);
 
    return ret;
}
