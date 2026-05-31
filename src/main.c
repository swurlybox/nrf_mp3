#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <ff.h>     /* ELM FATFS */

#include "buttons_and_leds/lbs.h"

#define DISK_DRIVE_NAME "SD"
#define DISK_MOUNT_PT "/"DISK_DRIVE_NAME":"

#define SLEEP_TIME_MS 1000

/* Filesystem setup */
static FATFS fat_fs;        /* elm fatfs structure */

/* mounting info: used by Zephyr's generic filesystem lib fs.h */
static struct fs_mount_t mp = { 
    .type = FS_FATFS,
    .mnt_point = DISK_MOUNT_PT,
    .fs_data = &fat_fs
};

#define FS_RET_OK FR_OK

LOG_MODULE_REGISTER(main);

#define MAX_PATH 128

static int lsdir(const char *path);


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
        printk("error: setup(): device initialization\n");
        return -1;
    }

    /* Attempt to mount the filesystem, then print the contents of the root
        directory. ERR: Issue with SD card initialization. */
    int res = fs_mount(&mp);

    if (res == FS_RET_OK) {
        printk("Disk mounted.\n");
        res = fs_unmount(&mp);
        if (res != FS_RET_OK) {
            printk("Error unmounting disk.\n");
            return res;
        }
        res = fs_mount(&mp);
        if (res != FS_RET_OK) {
            printk("Error remounting disk.\n");
            return res;
        }

        if (lsdir(DISK_MOUNT_PT) < 0) {
            printk("Error mounting disk.\n");
        }
    }

    while (1) {
        gpio_pin_toggle_dt(&status_led0);        
        k_msleep(SLEEP_TIME_MS);
    }
}

/* Returns the number of items in the directory, or a negative error code. 
    Prints the contents of the directory out to UART or the output source
    of printk(). */
static int lsdir(const char *path) {
    int res;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;
    int count = 0;

    fs_dir_t_init(&dirp);
    res = fs_opendir(&dirp, path);
    if (res) {
        printk("Error opening dir %s [%d]\n", path, res);
        return res;
    }

    printk("\nListing dir %s ...\n", path);
    for (;;) {
        /* Verify fs_readdir() */
        res = fs_readdir(&dirp, &entry);

        if (res || entry.name[0] == 0) {
            break;
        }

        if (entry.type == FS_DIR_ENTRY_DIR) {
            printk("[DIR ] %s\n", entry.name);
        } else {
            printk("[FILE] %s (size: %zu)\n", entry.name, entry.size);
        }
        count++;
    }

    /* Verify fs_closedir() */
    res = fs_closedir(&dirp);
    if (res == 0) {
        res = count;
    }
    return res;
}
