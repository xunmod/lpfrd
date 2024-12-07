#!/bin/sh
#######################################################################################
# Magisk Boot Image Patcher
# lpfrd boot image patcher, ported from magisk's
#######################################################################################
#
# Usage: boot_patch.sh <bootimage>
#
# This script should be placed in a directory with the following files:
#
# File name             Type      Description
#
# boot_patch.sh         script    A script to patch boot image for Magisk.
#                     (this file) The script will use files in its same
#                                 directory to complete the patching process
# init.lpfrd.rc         symlink   Symlink to ../init.lpfrd.rc
# lpfrd.magiskpolicy.te text      magiskpolicy version of lpfrd.te
# magiskboot            binary    A tool to manipulate boot images
# magiskpolicy          binary    A tool to manipulate selinux policy
# lpfrd                 binary    The lpfrd binary
#
#######################################################################################

############
# Functions
############

abort() {
  echo "$1"
  exit 1
}

# Pure bash dirname implementation
getdir() {
  case "$1" in
    */*)
      dir=${1%/*}
      if [ -z $dir ]; then
        echo "/"
      else
        echo $dir
      fi
    ;;
    *) echo "." ;;
  esac
}

#################
# Initialization
#################

# Switch to the location of the script file
# cd "$(getdir "${BASH_SOURCE:-$0}")"

BOOTIMAGE="$1"
[ -e "$BOOTIMAGE" ] || abort "$BOOTIMAGE does not exist!"

chmod -R 755 .

#########
# Unpack
#########

echo "- Unpacking boot image"
./magiskboot unpack "$BOOTIMAGE" || abort "! Unable to unpack boot image"

###################
# Ramdisk Restores
###################

# Test patch status and do restore
echo "- Checking ramdisk status"
if [ -e ramdisk.cpio ]; then
  ./magiskboot cpio ramdisk.cpio test
  STATUS=$?
else
  abort "! No ramdisk in image"
fi
case $((STATUS & 3)) in
  2 )  # Unsupported
    echo "! Boot image patched by unsupported programs"
    abort "! Please restore back to stock boot image"
    ;;
esac

##################
# Ramdisk Patches
##################

echo "- Patching ramdisk"

./magiskboot cpio ramdisk.cpio "extract init.rc init.rc"
./magiskboot cpio ramdisk.cpio "extract sepolicy sepolicy"
cat init.lpfrd.rc >> init.rc
./magiskpolicy --load sepolicy --save sepolicy --apply lpfrd.magiskpolicy.te
./magiskboot cpio ramdisk.cpio \
"add 0750 init.rc init.rc" \
"add 0644 sepolicy sepolicy" \
"add 0750 sbin/lpfrd lpfrd"

rm -f init.rc sepolicy

#################
# Repack
#################

echo "- Repacking boot image"
./magiskboot repack "$BOOTIMAGE" || abort "! Unable to repack boot image"

# Reset any error code
true
