#ifndef LBS_H
#define LBS_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

/* Expose buttons and leds to those who are interested. */
extern const struct gpio_dt_spec status_led0;
extern const struct gpio_dt_spec led1;
extern const struct gpio_dt_spec led2;
extern const struct gpio_dt_spec led3;

typedef struct {
    uint16_t code;                /* unique identifier for a button */
    void (*press)(void);         /* button-press callback */
    void (*release)(void);       /* button-release callback */
} button_cb;

#define BUTTON_ARR_SIZE 6
extern button_cb button_arr[];

/* Check that the buttons and leds are initialized as expected. 
    Returns 0 on success, -1 on error. */
int lbs_check(void);

/* Resets all button presses and releases to their state-agnostic default 
    behavior. */
void button_cb_reset_all(void);

/* Attach a press callback to a button identified by their input code. 
    See DeviceTree overlay or buttons.c for their event codes. */
void button_press_attach(uint16_t code, void (*press)(void));

/* Attach a release callback to a button identified by their input code. 
    See DeviceTree overlay or buttons.c for their event codes. */
void button_release_attach(uint16_t code, void (*release)(void));

#endif