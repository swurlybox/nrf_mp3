#ifndef FS_NAV_H
#define FS_NAV_H

/* A filesystem state init function
    - mount the filesystem on the SD card into a ready state
*/
int fs_check(void);

/* A filesystem state enter transition function 
    - sets the system up for the filesystem navigation state
    - hooking up the button callbacks to navigate the filesystem */
void fs_enter(void);

/* Internal source file handles the logic for navigating the filesystem. */

#endif