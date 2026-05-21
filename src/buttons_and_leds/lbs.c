#include "lbs.h"

#define EB0_NODE    DT_ALIAS(ext_button0)
const struct gpio_dt_spec ext_button0 = GPIO_DT_SPEC_GET(EB0_NODE, gpios);

#define EL0_NODE    DT_ALIAS(ext_led0)
const struct gpio_dt_spec ext_led0 = GPIO_DT_SPEC_GET(EL0_NODE, gpios);

#define SW0_NODE    DT_ALIAS(sw0)
const struct gpio_dt_spec button0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);

#define LED0_NODE   DT_ALIAS(led0)
const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int lbs_check(void) {
    /* Check gpio0 is ready. */
    if (!device_is_ready(led0.port)) {
        return -1;
    }

    /* Button0 intended to control led0 */
    if (gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE) ||
        gpio_pin_configure_dt(&button0, GPIO_INPUT)) {
        return -1;
    }

    /* Ext_button0 controls ext_led0 */
    if (gpio_pin_configure_dt(&ext_led0, GPIO_OUTPUT_ACTIVE) ||
        gpio_pin_configure_dt(&ext_button0, GPIO_INPUT)) {
        return -1;
    }

    return 0;
}