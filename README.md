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
`USE_SHELL_REBOOT`: Use `reboot` command to reboot instead of `set_property`. Define it if it doesn't reboot to specified target.  
Edit these in `jni/lpfrd.c`.

## Todo
Help me to perform memory safety check pls
