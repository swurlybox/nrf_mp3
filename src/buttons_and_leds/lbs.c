#include "lbs.h"
#include <zephyr/input/input.h>

#define LED0_NODE   DT_ALIAS(led0)
#define LED1_NODE   DT_ALIAS(led1)
#define LED2_NODE   DT_ALIAS(led2)
#define LED3_NODE   DT_ALIAS(led3)

const struct gpio_dt_spec status_led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);

static void btn_df_hdlr() { printk("No useful callback attached\n"); }

/* See DeviceTree to see which buttons are mapped to which keycode. */
button_cb button_arr[BUTTON_ARR_SIZE] = {
    {INPUT_KEY_UP,      btn_df_hdlr, btn_df_hdlr},  
    {INPUT_KEY_LEFT,    btn_df_hdlr, btn_df_hdlr},
    {INPUT_KEY_DOWN,    btn_df_hdlr, btn_df_hdlr},
    {INPUT_KEY_RIGHT,   btn_df_hdlr, btn_df_hdlr},
    {INPUT_KEY_ENTER,   btn_df_hdlr, btn_df_hdlr},
    {INPUT_KEY_BACK,    btn_df_hdlr, btn_df_hdlr}
};

/* Button arbitrator */
static void button_input_cb(struct input_event *evt, void *user_data) {
	if (evt->sync == 0) { return; }

    printk("Button %d %s\n", evt->code, evt->value ? "pressed" : "released");
    for (int i = 0; i < BUTTON_ARR_SIZE; i++) {
        if (button_arr[i].code == evt->code) {
            evt->value ? button_arr[i].press(): button_arr[i].release();
            return;
        }
    }
    printk("Unsupported code: %d\n", evt->code);
}

INPUT_CALLBACK_DEFINE(NULL, button_input_cb, NULL);

int lbs_check(void) {
    /* Check gpio0 is ready. Checks on devices on the same port are redundant. */
    if (!device_is_ready(status_led0.port)) {
        printk("error: lbs_check(): gpio port 0 not ready\n");
        return -1;
    }

    /* Configure buttons and leds for input or output. */
    if (gpio_pin_configure_dt(&status_led0, GPIO_OUTPUT_INACTIVE)    ||
        gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE)           ||
        gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE)           ||
        gpio_pin_configure_dt(&led3, GPIO_OUTPUT_INACTIVE)) {
        printk("error: lbs_check(): gpio i/o mode configure failed\n");
        return -1;    
    }

    printk("lbs_check(): success\n");
    return 0;
}

void button_cb_reset_all(void) {
    for (int i = 0; i < BUTTON_ARR_SIZE; i++) {
        button_arr[i].press = btn_df_hdlr;
        button_arr[i].release = btn_df_hdlr;
    }
    printk("reset all button callbacks\n");
}

/* Little inefficient to have to loop through the button array and examine
    their code to determine which button we're looking for. But this is fine
    since these functions are not expected to be executed very frequently. */
void button_press_attach(uint16_t code, void (*press)(void)) {
    for (int i = 0; i < BUTTON_ARR_SIZE; i++) {
        if (button_arr[i].code == code) {
            button_arr[i].press = press;
            return;
        }
    }
    printk("couldn't find button to attach: %d\n", code);
}

void button_release_attach(uint16_t code, void (*release)(void)) {
    for (int i = 0; i < BUTTON_ARR_SIZE; i++) {
        if (button_arr[i].code == code) {
            button_arr[i].release = release;
            return;
        }
    }
    printk("couldn't find button to attach: %d\n", code);
}