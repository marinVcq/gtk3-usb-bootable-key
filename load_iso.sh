#!/bin/bash

# Function to cleanup and exit
cleanup_exit() {
    echo "Interrupt signal received. Cleaning up..."
    pkill -f "dd if=$iso_path"
    exit 1
}

# Register the cleanup_exit function to be called on interrupt signal
trap cleanup_exit SIGINT

# Check if the script is run as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (sudo)."
    exit 1
fi

# Check if the correct number of arguments is provided
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 /dev/sdX /path/to/your.iso"
    exit 1
fi

# Assign the device and ISO path from the command line arguments
device="$1"
iso_path="$2"

# Check if the specified device exists
if [ ! -e "$device" ]; then
    echo "Error: Device $device does not exist."
    exit 1
fi

# Check if the specified ISO file exists
if [ ! -e "$iso_path" ]; then
    echo "Error: ISO file $iso_path does not exist."
    exit 1
fi

# Identify and kill processes using the device
fuser -k "$device"

# Wait for processes to be killed
sleep 2

# Unmount the device
umount "$device"*

# Write the ISO to the USB key using dd
dd if="$iso_path" of="$device" bs=4M status=progress 2>&1

# Sync and notify completion
sync
echo "Bootable USB created successfully on $device. Status: 100%"

