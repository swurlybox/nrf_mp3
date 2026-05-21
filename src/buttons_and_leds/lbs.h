#ifndef LBS_H
#define LBS_H

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

/* Expose buttons and leds to those who are interested. */
extern const struct gpio_dt_spec ext_button0;
extern const struct gpio_dt_spec ext_led0;
extern const struct gpio_dt_spec button0;
extern const struct gpio_dt_spec led0;

/* Check that the buttons and leds are initialized as expected. */
int lbs_check(void);

#endif