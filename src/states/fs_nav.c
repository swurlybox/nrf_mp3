#include "fs_nav.h"

#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>   /* Zephyr's generic filesystem API */
#include <ff.h>     /* ELM FATFS */

#include <string.h>

#define DISK_DRIVE_NAME "SD"
#define DISK_MOUNT_PT "/"DISK_DRIVE_NAME":"
#define MAX_PATH 128
#define FS_RET_OK FR_OK
#define CWD_SIZE 512

LOG_MODULE_REGISTER(fs_nav);

/* Filesystem setup */
static FATFS fat_fs;        /* elm fatfs structure */

/* mounting info: used by Zephyr's generic filesystem lib fs.h */
static struct fs_mount_t mp = { 
    .type = FS_FATFS,
    .mnt_point = DISK_MOUNT_PT,
    .fs_data = &fat_fs
};

/* Used by the filesystem navigation functions; maintains state information
    about the currently selected file in the current working directory. */
typedef struct ff_nav_state {
    volatile uint32_t index;
    volatile uint32_t dirent_count;
    char cwd[CWD_SIZE];
} FF_NAV_T;
static FF_NAV_T ff_nav_t = {0};

/* Since Zephyr's filesystem API lacks any notion of a current working 
    directory and chdir functions, we'll extend it by keeping track
    of our current working directory and providing navigation functions. */
static void chdir(const char *path);

static int  lsdir(const char *path);
static void cycle_up(void);
static void cycle_down(void);
static void select(void);
static void cancel(void);

int fs_check(void) {
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

        if (lsdir("/") < 0) {
            printk("Error mounting disk.\n");
        }
    }

    /* Test chdir */
    k_msleep(1000);
    printk("test1\n");
    chdir("..");    /* should still be at root fs */
    lsdir(ff_nav_t.cwd);

    k_msleep(1000);
    printk("test2\n");
    chdir("testdir");
    lsdir(ff_nav_t.cwd);

    k_msleep(1000);
    printk("test3\n");
    chdir("testsubdir");
    lsdir(ff_nav_t.cwd);

    k_msleep(1000);
    printk("test4\n");
    chdir(".");
    lsdir(ff_nav_t.cwd);

    k_msleep(1000);
    printk("test5\n");
    chdir("../../..");
    lsdir(ff_nav_t.cwd);

    return res;
}

/* TODO: Add support for .., ., and relative pathing. */
static void chdir(const char *path) {
    int res;
    struct fs_dir_t dirp;
    fs_dir_t_init(&dirp);
    char temp_path[CWD_SIZE] = {0}; // mutable copy of path
    char temp_cwd[CWD_SIZE] = {0};  // mutable copy of cwd

    /* Absolute pathing: Detected by a starting '/'. Mount point is prepended,
        so user shouldn't worry about it. */
    if (path[0] == '/') {
        strcat(temp_cwd, DISK_MOUNT_PT);
        strcat(temp_cwd, path);
        res = fs_opendir(&dirp, temp_cwd);
        if (res) {
            printk("Error changing dir %s [%d]\n", path, res);
            return;
        } else {
            printk("Dir exists, changing cwd to %s\n", path);
            strncpy(ff_nav_t.cwd, path, CWD_SIZE);
            ff_nav_t.index = 0;
            fs_closedir(&dirp);
            return;
        }
    }

    /* Grab a copy of the absolute path of the cwd that we can mutate. */
    strncpy(temp_cwd, ff_nav_t.cwd, CWD_SIZE);
    strncpy(temp_path, path, CWD_SIZE);
    /* Relative pathing: 
        .. backtracks to a parent directory
        .  is ignored
        The approach here is to mutate temp_path as we traverse the tokens
        in the provided path string. If a directory doesn't exist, we fail
        and the original cwd will stay intact. If at the end we find a
        valid directory, we'll set the cwd as temp_path. */
    char *saveptr;
    char *token;
    char *c_to_rmv;
    token = strtok_r(temp_path, "/", &saveptr);  /* NOTE: strtok_r is destructive */
    while(token != NULL) {
        /* Backtrack to parent directory */
        if (strcmp(token, "..") == 0) {
            /* Assumption: first char of cwd contains "/".
                So if c_to_rmv ever is NULL, we'll be at the root directory. */
            if ((c_to_rmv = strrchr(temp_cwd, '/')) != NULL) {
                *c_to_rmv = '\0';
            }
        }
        else if (strcmp(token, ".") == 0) {
            /* do nothing. */
        }
        else {
            /* append the token to the current path. */
            /* NOTE: should really do some sort of bounds-checking */
            strcat(temp_cwd, "/");
            strcat(temp_cwd, token);
        }
        token = strtok_r(NULL, "/", &saveptr);
    }
    *temp_path = '\0';   /* reuse temp_path */
    strcat(temp_path, DISK_MOUNT_PT);
    strcat(temp_path, temp_cwd);
    res = fs_opendir(&dirp, temp_path);
    if (res) {
        printk("Error changing dir %s [%d]\n", temp_cwd, res);
        return;
    } else {
        printk("Dir exists, changing cwd to %s\n", temp_cwd);
        strncpy(ff_nav_t.cwd, temp_cwd, CWD_SIZE);
        ff_nav_t.index = 0;
        fs_closedir(&dirp);
        return;
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
    char temp_path[CWD_SIZE] = {0};

    strcat(temp_path, DISK_MOUNT_PT);
    strcat(temp_path, path);

    fs_dir_t_init(&dirp);
    res = fs_opendir(&dirp, temp_path);
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

static void cycle_up(void) {
    /* Wrap-around to the last entry. */
    if (ff_nav_t.index == 0) {
        ff_nav_t.index = ff_nav_t.dirent_count - 1;
    } else {
        ff_nav_t.index--;
    }
    lsdir(ff_nav_t.cwd);
}

static void cycle_down(void) {
    /* Wrap-around behaviour; account for 0-based indexing */
    if (ff_nav_t.index == ff_nav_t.dirent_count - 1) {
        ff_nav_t.index = 0;
    } else {
        ff_nav_t.index++;
    }
    lsdir(ff_nav_t.cwd);
}

static void cancel(void) {
    /* Go to parent directory. */
    /* Zephyr's FS API lacks a notion of CWD and CHDIR? */
}