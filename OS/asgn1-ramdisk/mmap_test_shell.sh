#!/bin/bash

# Define variables
MODULE_NAME="asgn1"
DEVICE_NAME="/dev/asgn1"
MAJOR_NUM="239" # Get this from dmesg output
TEST_FILE="test_data.txt"

# Check if the script is run as root
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root."
   exit 1
fi

echo "--- Loading the ramdisk module ---"
insmod ${MODULE_NAME}.ko
sleep 1 # Give the kernel a moment to load

# Get the major number from dmesg, just in case it's different
# Although your dmesg output shows it's consistently 239
DMESG_OUTPUT=$(dmesg | grep "${MODULE_NAME}: loaded" | tail -n 1)
if [[ $DMESG_OUTPUT =~ Major=([0-9]+) ]]; then
    MAJOR_NUM=${BASH_REMATCH[1]}
    echo "Found major number: ${MAJOR_NUM}"
else
    echo "Could not find major number in dmesg. Using default: ${MAJOR_NUM}"
fi

echo "--- Creating device node: ${DEVICE_NAME} ---"
mknod ${DEVICE_NAME} c ${MAJOR_NUM} 0

# Check if the device node was created successfully
if [ ! -c "${DEVICE_NAME}" ]; then
    echo "Error: Device node not created."
    exit 1
fi

echo "--- Writing test data to the device ---"
echo "Hello, this is a test from the Bash script." > ${DEVICE_NAME}
echo "This line should also be written to the ramdisk." >> ${DEVICE_NAME}
echo "Data written to ${DEVICE_NAME}"

echo "--- Running the mmap test program ---"
# Give execute permissions to the test program
chmod +x ./mmap_test
./mmap_test ${DEVICE_NAME}

echo "--- Cleaning up: Removing the device node ---"
rm ${DEVICE_NAME}

echo "--- Unloading the ramdisk module ---"
rmmod ${MODULE_NAME}

echo "--- Script finished ---"
