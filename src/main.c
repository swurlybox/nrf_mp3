#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include "buttons_and_leds/lbs.h"

#define SLEEP_TIME_MS 1000

/* Do any hardware/software initializations or checks within this function. */
int setup(void) {
    if (lbs_check()) {
        return -1;
    }
    return 0;
}

void test_callback() {
    printk("test callback invoked\n");
}

void custom_reset() {
    button_cb_reset_all();
}

int main(void)
{
    if(setup()) {
        printk("error: setup(): device initialization\n");
        return -1;
    }

    while (1) {
        gpio_pin_toggle_dt(&status_led0);        
        k_msleep(SLEEP_TIME_MS);
    }
}
