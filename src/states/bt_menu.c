#include "bt_menu.h"
#include "../buttons_and_leds/lbs.h"
#include "fs_nav.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>  // for GATT discover 
#include <zephyr/bluetooth/uuid.h>  // for UUID helpers
#include <zephyr/bluetooth/hci.h>

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

K_SEM_DEFINE(alter_conn, 0, 1);

static struct bt_conn *conn;

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
    int size;                   /* number of scanned connections */
    bt_addr_le_t arr[MAX_SCAN_LIST];    /* TODO: change to an array of bt-addresses. */
} sl_menu_t;

sl_menu_t sl_menu = { 0 };

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

static uint8_t discover_cb(struct bt_conn *conn, 
    const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params) {
    if (attr == NULL) {
        printk("Discovery complete\n");
        return BT_GATT_ITER_STOP;
    }

    /* 64 characters should be long enough for a uuid? */
    char uuid_str[64];

    /* Just print the value of the attr->uuid */
    bt_uuid_to_str(attr->uuid, uuid_str, 64);

    printk("UUID: %s, attr_handle: %d\n", uuid_str, 
        bt_gatt_attr_get_handle(attr));

    return BT_GATT_ITER_CONTINUE;
};

/* Zephyr's documentation is shit at explaining what needs to be set. Only found
    this out through a blog post someone wrote on Zephyr Centrals. */
static struct bt_gatt_discover_params discover_params = {
    .uuid = NULL,   /* set to null, all attributes will be discovered */
    .func = discover_cb,
    .start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE,
    .end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE,
    .type = BT_GATT_DISCOVER_PRIMARY
};

static uint8_t read_cb(struct bt_conn *conn, uint8_t err,
    struct bt_gatt_read_params *params, const void *data, uint16_t length) {
    if (err || data == NULL) {
        printk("Done reading\n");
        return BT_GATT_ITER_STOP;
    }
    
    printk("New UUID ------------ \n");
    const char *byte = data;
    for (int i = 0; i < length; i++) {
        printk("%x ", byte[i]);
        if (i % 10 == 0 && i != 0) {
            printk("\n");
        }
    }
    printk("\nEnd UUID ---------- \n");
    return BT_GATT_ITER_CONTINUE;
}

static struct bt_gatt_read_params read_params = {
    .func = read_cb,
    .by_uuid.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE,
    .by_uuid.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE,
    .by_uuid.uuid = BT_UUID_GATT_PRIMARY  
};

static void print_connection_status(void) {
    /* Call bt_gatt_discover(), remember to attach a callback to
        params->func to print the discovered profiles/services/attributes. */
    int err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        printk("Failed to print status\n");
    }

    /* UUID 2800 is the primary service we're interested in. */
    err = bt_gatt_read(conn, &read_params);
    if(err) {
        printk("Failed to read GATT services\n");
    }
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
                bt_menu.status ^= BT_ENABLED;
                if (err) {
                    printk("Failed to enable bluetooth\n");
                } else {
                    printk("Bluetooth enabled\n");
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
                sl_menu.size = 0;
                err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
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
            if (bt_menu.status & BT_CONNECTED) {
                bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
                k_sem_take(&alter_conn, K_FOREVER); // Block until callback releases
                print_menu();
            } else {
                sl_enter();
            }
            break;
        case 3:
            /* Print status */
            if (bt_menu.status & BT_CONNECTED) {
                print_connection_status();
            } else {
                printk("No active connection\n");
            }
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
    sl_print();
}

static void sl_print(void) {
    if (sl_menu.size == 0) {
        printk("No connections available, please go back and scan\n");
        return;
    }
    printk("\n");
    char bt_addr_le_str[BT_ADDR_LE_STR_LEN];
    for (int i = 0; i < sl_menu.size; i++) {
        if (i == sl_menu.index) {
            printk("> ");
        } else {
            printk("  ");
        }
        bt_addr_le_to_str(&sl_menu.arr[i], bt_addr_le_str, BT_ADDR_LE_STR_LEN);
        printk("Option %d: %s\n", i, bt_addr_le_str);
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
    int err;
    char bt_addr_le_str[BT_ADDR_LE_STR_LEN];

    if (sl_menu.index >= sl_menu.size) {
        printk("Index inconsistent with array size\n");
        return;
    }

    /* Grab the selected ble address from the scan list. */
    bt_addr_le_t *addr = &sl_menu.arr[sl_menu.index];
    bt_addr_le_to_str(addr, bt_addr_le_str, BT_ADDR_LE_STR_LEN);

    /* Attempt to establish a connection with the chosen ble address from
        the scan list. This seems to error if we've already connected before. */
    err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
        BT_LE_CONN_PARAM_DEFAULT, &conn);

    if (err) {
        printk("Unable to establish BLE connection: %s (%d)\n",
            bt_addr_le_str, err);
        return;
    }

    k_sem_take(&alter_conn, K_FOREVER); /* Block until callback releases */
    bt_menu_enter();
}

static void sl_cancel() {
    bt_menu_enter();
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, 
    uint8_t adv_type, struct net_buf_simple *buf) {
    char bt_addr_le_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, bt_addr_le_str, BT_ADDR_LE_STR_LEN);

    /* The device must be connectable. */
    if (!(adv_type == BT_GAP_ADV_TYPE_ADV_IND || 
        adv_type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND ||
        adv_type == BT_GAP_ADV_TYPE_EXT_ADV)) {
        // printk("Non-connectable: %s, rssi: %d\n", bt_addr_le_str, rssi);
        return;
    }

    /* Filter out rssi < -50 */
    if (rssi < -50) {
        // printk("Out-of-preferred range (<-50): %s, rssi: %d\n", bt_addr_le_str,
        //     rssi);
        return;
    }

    if (sl_menu.size >= MAX_SCAN_LIST) {
        // printk("Hit max scan list\n");
        return;
    }

    for (int i = 0; i < sl_menu.size; i++) {
        if (bt_addr_le_cmp(&sl_menu.arr[i], addr) == 0) {
            // printk("Found duplicate entry: %s, rssi: %d\n", bt_addr_le_str, 
            //     rssi);
            return;
        }
    }
    
    printk("Found device, adding to list: %s, rssi: %d\n", bt_addr_le_str, 
        rssi);

    memcpy(&sl_menu.arr[sl_menu.size++], addr, sizeof(*addr));
}

/* Bluetooth Connection Callbacks ------------------------------------------ */

static void connected(struct bt_conn *i_conn, uint8_t err)
{
	if (err) {
		printk("Connection callback: failed to connect\n");
        k_sem_give(&alter_conn);    /* So we don't brick the input-thread */
		bt_conn_unref(i_conn);
        conn = NULL;
		return;
    }
	printk("Connection callback: Connected\n");
	// bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    bt_menu.status ^= BT_CONNECTED;
    k_sem_give(&alter_conn);
}

static void disconnected(struct bt_conn *i_conn, uint8_t reason)
{
	printk("Disconnected: reason 0x%02x %s\n", reason, 
        bt_hci_err_to_str(reason));
    bt_conn_unref(i_conn);
    conn = NULL;
    bt_menu.status ^= BT_CONNECTED;
    if (reason != BT_HCI_ERR_LOCALHOST_TERM_CONN) {
        /* Anything other than the central intiatiating the disconnection,
            is considered an unexpected disconnect. Don't touch the semaphore.*/
        return;
    }
    k_sem_give(&alter_conn);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};