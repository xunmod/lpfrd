# Long Press For Recovery Daemon

Detects long press of a key and reboot into recovery or something else

## Usage
> To test if it works: `lpfrd -d`  

Build using `ndk-build`, copy `libs/[your device's arch]/lpfrd` to `/sbin` in ramdisk of boot image

## Customization
`INPUT_PATH`: Path to directory of input devices such as `event0`. Usually no need to change.  
`MAX_DEVICES`: Max number of input devices the program can monitor. Usually no need to change.  
`KEY`: The key to be detected. You can find it out by `getevent -l`.  
`REBOOT_TARGET`: Reboot to where. Value depends on vendor bootloader implementation, usually the same as the `[target]` part in `adb reboot [target]`.  
`TIMEOUT`: Time of long press, in millisecond.  
Edit these in `jni/lpfrd.c`.

## Debug
If it's not working, try `setenforce 0 && start lpfrd` and long press to see if it works now.  
If so, it's a selinux problem. Try this command to capture selinux denied log:  
`while true; do dmesg | grep denied; sleep 0.5; done & start lpfrd`  
You should see the denied log before or after long pressing.

## Todo
Help me to perform memory safety check pls
