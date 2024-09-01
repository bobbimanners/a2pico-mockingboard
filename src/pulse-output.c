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

#include <sys/ioctl.h>
#include <termios.h>

// Stolen from
// https://stackoverflow.com/questions/29335758/using-kbhit-and-getch-on-linux
char kbhit() {
    struct termios term;
    tcgetattr(0, &term);

    struct termios term2 = term;
    term2.c_lflag &= ~ICANON;
    tcsetattr(0, TCSANOW, &term2);

    int byteswaiting;
    ioctl(0, FIONREAD, &byteswaiting);

    tcsetattr(0, TCSANOW, &term);

    return byteswaiting > 0;
}

void show_ay3_regs(ay3_state *ay3) {
  printf("\n\n");
  for (unsigned int i = 0; i < 16; ++i) {
    unsigned int val;
    val = ay3_get_register(ay3, i);
    printf("R%u\t%u (0x%x)\n", i, val , val);
  }
}
 
int main(int argc, char*argv[]) {

    /* The Sample format to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_U8,
        .rate = AY3_SAMPLERATE,
        .channels = 2
    };
 
    ay3_state *ay3 = create_ay3();
 
    pa_simple *s = NULL;
    int ret = 1;
    int error;
 
    /* Create a new playback stream */
    if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        goto finish;
    }

    ay3_set_register(ay3, 0, 128);
    ay3_set_register(ay3, 1, 128);
    ay3_set_register(ay3, 2, 200);
    ay3_set_register(ay3, 3, 200);
    ay3_set_register(ay3, 4, 50);
    ay3_set_register(ay3, 5, 50);
    ay3_set_register(ay3, 6, 0);
    ay3_set_register(ay3, 7, 0);
    ay3_set_register(ay3, 8, 0x0f);
    ay3_set_register(ay3, 9, 0x0f);
    ay3_set_register(ay3, 10, 0x0f);
    ay3_set_register(ay3, 11, 0);
    ay3_set_register(ay3, 12, 0);
    ay3_set_register(ay3, 13, 0);
    ay3_set_register(ay3, 14, 0);
    ay3_set_register(ay3, 15, 0);

    show_ay3_regs(ay3);
 
    for (;;) {
        uint8_t buf[BUFSIZE];
        ssize_t r;
 
#if 0
        pa_usec_t latency;
 
        if ((latency = pa_simple_get_latency(s, &error)) == (pa_usec_t) -1) {
            fprintf(stderr, __FILE__": pa_simple_get_latency() failed: %s\n", pa_strerror(error));
            goto finish;
        }
 
        fprintf(stderr, "%0.0f usec    \r", (float)latency);
#endif

        if (kbhit()) {
          unsigned int reg, val;
          do {
            printf("\nAY3 Reg [0..15 ] > ");
            scanf("%u", &reg);
          } while (reg > 15);
          do {
            printf("    Val [0..255] > ");
            scanf("%u", &val);
          } while (val > 255);
          printf("Setting R%u to %u ...\n", reg, val);
          ay3_set_register(ay3, reg, val);
          show_ay3_regs(ay3);
        }

        /* Generate some data ... */ 
        ay3_process(ay3);
 
        /* ... and play it */
        if (pa_simple_write(s, ay3->output, (size_t) AY3_SAMPLES, &error) < 0) {
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
