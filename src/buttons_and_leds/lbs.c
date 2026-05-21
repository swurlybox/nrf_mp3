#include "lbs.h"

#define EB0_NODE    DT_ALIAS(ext_button0)
const struct gpio_dt_spec ext_button0 = GPIO_DT_SPEC_GET(EB0_NODE, gpios);

#define SW0_NODE    DT_ALIAS(sw0)
const struct gpio_dt_spec button0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);

#define LED0_NODE   DT_ALIAS(led0)
const struct gpio_dt_spec status_led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* Responsible for hooking up callbacks, later passed into a function to
    register the callbacks to a particular button. */
static struct gpio_callback button0_cb_data;
static struct gpio_callback ext_button0_cb_data;

void button0_pressed_print(const struct device *dev, struct gpio_callback *cb, 
    uint32_t pins) {
    printk("Button pressed\n");
}

void button0_pressed_nothing(const struct device *dev, struct gpio_callback *cb,
    uint32_t pins) {
    printk("Nothing!\n");
}

void ext_button0_pressed(const struct device *dev, struct gpio_callback *cb,
    uint32_t pins) {
    gpio_init_callback(&button0_cb_data, button0_pressed_nothing, BIT(button0.pin));
    printk("Changed button0's callback!\n");
}

int lbs_check(void) {
    /* Check gpio0 is ready. Checks on devices on the same port are redundant. */
    if (!device_is_ready(status_led0.port)) {
        return -1;
    }

    /* Configure buttons and leds. */
    if (gpio_pin_configure_dt(&status_led0, GPIO_OUTPUT_ACTIVE) ||
        gpio_pin_configure_dt(&button0, GPIO_INPUT) ||
        gpio_pin_configure_dt(&ext_button0, GPIO_INPUT)) {
        return -1;    
    }

    /* Interrupts */
    if (gpio_pin_interrupt_configure_dt(&button0, GPIO_INT_EDGE_TO_ACTIVE)) {
        return -1;
    }
    gpio_init_callback(&button0_cb_data, button0_pressed_print, BIT(button0.pin));
    gpio_add_callback(button0.port, &button0_cb_data);   // as many as we want

    /* On external button press, we change the callback in the cb_data structure */
    if (gpio_pin_interrupt_configure_dt(&ext_button0, GPIO_INT_EDGE_TO_ACTIVE)) {
        return -1;
    }
    gpio_init_callback(&ext_button0_cb_data, ext_button0_pressed, BIT(ext_button0.pin));
    gpio_add_callback(ext_button0.port, &ext_button0_cb_data);
    
    return 0;
}