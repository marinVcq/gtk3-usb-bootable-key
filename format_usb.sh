#!/bin/bash

# Function to extract percent from shred output -- NOT IN USE YET 
extract_percent_from_shred_output() {
    local buffer="$1"
    local percentStr=$(echo "$buffer" | grep -o '[0-9]\+%' | tail -n 1)

    if [ -n "$percentStr" ]; then
        local percent=$(echo "$percentStr" | sed 's/%//')
        echo "$percent"
    else
        echo "-1"  # If '%' is not found
    fi
}

# Check if the script is run with root privileges
if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root. Please use sudo or run as root."
    exit 1
fi

# Check if a device path is provided as an argument
if [ $# -eq 0 ]; then
    echo "Usage: $0 <device_path> [rewrite] [format] [label]"
    echo "Example: $0 /dev/sdb true ext4 MY_USB_LABEL"
    exit 1
fi

# Get the device path from the command line argument
device=$1
rewrite=$2
format=$3
label=$4

# Check if the 'rewrite' parameter is provided
if [ "$rewrite" == "true" ]; then
    # Shred the device before formatting
    echo "Shredding process begin..."
    shred -v -n 1 -s 1G --random-source=/dev/urandom "$device" 2>&1

    # Check if shred was successful (exit status 0)
    if [ $? -eq 0 ]; then
        echo "Shredding completed 100%"
    else
        echo "Shredding failed"
        exit 1
    fi
fi

# Unmount the device if it's mounted
umount "$device"

# Format the device using the specified format and label
if [ -n "$format" ]; then
    echo "Formatting process begin..."
    mkfs."$format" -L "$label" "$device" > /dev/null 2>&1

    # Check if mkfs was successful (exit status 0)
    if [ $? -eq 0 ]; then
        echo "Formatting completed 100%"
    else
        echo "Formatting failed"
        exit 1
    fi
else
    echo "Invalid or missing format parameter."
    exit 1
fi

