#include <stdio.h>

#include "pico/stdlib.h"

extern void led_hardware_bringup_run_once(void);

int main(void) {
    stdio_init_all();
    sleep_ms(2000);
    printf("Pico Music Lights: six-strip SK6812 RGBW bring-up\n");
    while (true) {
        led_hardware_bringup_run_once();
    }
}
