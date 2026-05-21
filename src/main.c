#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include "buttons_and_leds/lbs.h"

#define SLEEP_TIME_MS 100

/* Do any hardware/software initializations or checks within this function. */
int setup(void) {
    if (lbs_check()) {
        return -1;
    }
    return 0;
}

int main(void)
{
    if(setup()) {
        printk("Error: setup(): device initialization\n");
        return -1;
    }

    bool button_state;
    while (1) {
        /* We're essentially polling the state of the button,
            then setting the LED state accordingly on a SLEEP_TIME_MS
            interval. */
        button_state = gpio_pin_get_dt(&button0);
        gpio_pin_set_dt(&led0, button_state);

        button_state = gpio_pin_get_dt(&ext_button0);
        gpio_pin_set_dt(&ext_led0, button_state);

        k_msleep(SLEEP_TIME_MS);
    }
}
