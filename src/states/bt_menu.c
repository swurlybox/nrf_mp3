#include "bt_menu.h"
#include "../buttons_and_leds/lbs.h"
#include "fs_nav.h"

#include <zephyr/bluetooth/bluetooth.h>

/* BT main menu ------------------------------------------------------------ */
/* BT Menu will define a couple of options for BT configuration:

    1. Enable/Disable Bluetooth LE.
    2. Scan for Connectable BLE devices.
    3. Disconnect/Connect to a BLE device from the scanned list.
    4. Print connection status (profiles, services, everything I'd want to know)

    Some constraints to keep it simple. Only one active connection allowed. */
typedef struct bt_menu_t {
    int index;  /* Currently selected index in the array of options. */
    const int size;
    uint8_t status; 
} bt_menu_t;

#define BT_ENABLED      BIT(0)
#define BT_SCANNING     BIT(1)
#define BT_CONNECTED    BIT(2)

#define NUM_BT_OPTIONS 4
bt_menu_t bt_menu = {
    .index = 0,
    .size = NUM_BT_OPTIONS,
    .status = 0U
};

/* Prints the menu to the UART terminal */
static void print_menu(void);

/* Main menu */
static void cycle_up(void);
static void cycle_down(void);
static void left(void);
static void right(void);
static void select(void);
static void cancel(void);

/* Scan list sub-menu ----------------------------------------------------- */
#define MAX_SCAN_LIST   10

typedef struct sl_menu_t {
    int index;                  /* currently selected index in the list */
    int size;                   /* number of scanned connections*/
    char arr[MAX_SCAN_LIST];    /* TODO: change to an array of bt-addresses. */
} sl_menu_t;

sl_menu_t sl_menu = {
    .index = 0,
    .size = 0,
    .arr = "Hello mom"
};

static void sl_enter(void);
static void sl_print(void);

static void sl_cycle_up(void);
static void sl_cycle_down(void);
static void sl_left(void);
static void sl_right(void);
static void sl_select(void);
static void sl_cancel(void);

static void device_found(const bt_addr_le_t *addr, int8_t rssi, 
    uint8_t adv_type, struct net_buf_simple *buf);

/* Implementation ---------------------------------------------------------- */

/* BT main menu ------------------------------------------------------------ */

void bt_menu_enter() {
    button_cb_reset_all();
    button_release_attach(INPUT_KEY_UP, cycle_up);
    button_release_attach(INPUT_KEY_LEFT, left);
    button_release_attach(INPUT_KEY_DOWN, cycle_down);
    button_release_attach(INPUT_KEY_RIGHT, right);
    button_release_attach(INPUT_KEY_ENTER, select);
    button_release_attach(INPUT_KEY_BACK, cancel);
    print_menu();
}

static void print_menu(void) {
    printk("\n");
    for (int option = 0; option < bt_menu.size; option++) {
        if (bt_menu.index == option) {
            printk ("> ");
        } else {
            printk("* ");
        }
        switch(option) {
            case 0:
                /* Enable/Disable Bluetooth option */
                if (bt_menu.status & BT_ENABLED) {
                    printk("Disable Bluetooth LE\n");
                } else {
                    printk("Enable Bluetooth LE\n");
                }
                break;
            case 1:
                /* Scan for LE devices */
                if (bt_menu.status & BT_SCANNING) {
                    printk("Stop scanning for LE devices\n");
                } else {
                    printk("Scan for LE devices\n");
                }
                break;
            case 2:
                /* Connect/Disconnect a device */
                if (bt_menu.status & BT_CONNECTED) {
                    printk("Disconnect from device\n");
                } else {
                    printk("Connect to a device\n");
                }
                break;
            case 3:
                /* Print connection status */
                if (bt_menu.status & BT_CONNECTED) {
                    printk("Print connection status\n");
                } else {
                    printk("Status: No active connection\n");
                }
                break;
            default:
                printk("Unsupported option\n");
                break;
        }
    }
}

static void cycle_up(void) {
    if (bt_menu.index == 0) {
        bt_menu.index = bt_menu.size - 1;
    } else {
        bt_menu.index--;
    }
    print_menu();
}

static void cycle_down(void) {
    if (bt_menu.index == bt_menu.size - 1) {
        bt_menu.index = 0;
    } else {
        bt_menu.index++;
    }
    print_menu();
}

static void left(void) {
    printk("Left pressed\n");
}

static void right(void) {
    printk("Right pressed\n");
}

static void select(void) {
    int err;
    /* Behavior changes depending on which option is selected */
    switch (bt_menu.index) {
        case 0:
            /* Enable or disable bluetooth */
            if (bt_menu.status & BT_ENABLED) {
                err = bt_disable();
                if (err) {
                    printk("Failed to disable bluetooth\n");
                } else {
                    printk("Bluetooth disabled\n");
                    bt_menu.status ^= BT_ENABLED;
                }
            } else {
                err = bt_enable(NULL);
                if (err) {
                    printk("Failed to enable bluetooth\n");
                } else {
                    printk("Bluetooth enabled\n");
                    bt_menu.status ^= BT_ENABLED;
                }
            }
            print_menu();
            break;
        case 1:
            /* Scanning option */
            if (bt_menu.status & BT_SCANNING) {
                err = bt_le_scan_stop();
                if (err) {
                    printk("Failed to stop BT scanning\n");
                } else {
                    printk("BT stop scanning\n");
                    bt_menu.status ^= BT_SCANNING;
                }
            } else {
                err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
                if (err) {
                    printk("Failed to start BT scanning\n");
                } else {
                    printk("BT start scanning\n");
                    bt_menu.status ^= BT_SCANNING;
                }
            }
            print_menu();   /* Scanning will be done in the background. */
            break;
        case 2:
            /* Connect or disconnect */
            printk("Option 2\n");
            sl_enter();
            break;
        case 3:
            /* Print status */
            printk("Option 3\n");
            break;
        default:
            printk("Unsupported option\n");
            break;
    }
}

static void cancel(void) {
    fs_enter();
}

/* Scan list sub-menu ------------------------------------------------------ */
static void sl_enter(void) {
    button_cb_reset_all();
    button_release_attach(INPUT_KEY_UP, sl_cycle_up);
    button_release_attach(INPUT_KEY_LEFT, sl_left);
    button_release_attach(INPUT_KEY_DOWN, sl_cycle_down);
    button_release_attach(INPUT_KEY_RIGHT, sl_right);
    button_release_attach(INPUT_KEY_ENTER, sl_select);
    button_release_attach(INPUT_KEY_BACK, sl_cancel);
    /* temporarily set the size to match, changed by bt-scan thread */
    sl_menu.size = 10;
    sl_print();
}

static void sl_print(void) {
    for (int i = 0; i < sl_menu.size; i++) {
        if (i == sl_menu.index) {
            printk("> ");
        } else {
            printk("  ");
        }
        printk("Option %d: %c\n", i, sl_menu.arr[i]);
    }
}

static void sl_cycle_up() {
    if (sl_menu.index == 0) {
        sl_menu.index = sl_menu.size - 1;
    } else {
        sl_menu.index--;
    }
    sl_print();
}

static void sl_cycle_down() {
    if (sl_menu.index == sl_menu.size - 1) {
        sl_menu.index = 0;
    } else {
        sl_menu.index++;
    }
    sl_print();
}

static void sl_left() {
    printk("Left\n");
}

static void sl_right() {
    printk("Right\n");
}

static void sl_select() {
    printk("Selected\n");
}

static void sl_cancel() {
    bt_menu_enter();
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, 
    uint8_t adv_type, struct net_buf_simple *buf) {
    char bt_addr_le_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, bt_addr_le_str, BT_ADDR_LE_STR_LEN);

    /* The device must be connectable. */
    if (!(adv_type & BT_GAP_ADV_TYPE_ADV_IND || 
        adv_type & BT_GAP_ADV_TYPE_ADV_DIRECT_IND)) {
        printk("Non-connectable: %s, rssi: %d\n", bt_addr_le_str, rssi);
        return;
    }

    /* Filter out rssi < -50 */
    if (rssi < -50) {
        printk("Out-of-preferred range (<-50): %s, rssi: %d\n", bt_addr_le_str,
            rssi);
        return;
    }

    printk("Found device: %s, rssi: %d\n", bt_addr_le_str, rssi);
}