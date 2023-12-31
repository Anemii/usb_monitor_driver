#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "UsbMonitorService_Server.h"


int main() {
    
    int fileDescriptor;
    int bytesRead;
    char buffer[KERNEL_DATA_LENG];
    UsbMonitorInfo usbMonitor;


    // Open the USB monitor file for reading
    fileDescriptor = open(DEV_NAME, O_RDWR);
    if (fileDescriptor == -1) {
        perror("Failed to open USB monitor file");
        return errno;
    }

    printf("USB Monitor is working!!!\n");

    while (1) {
        // Read data from the file
        bytesRead = read(fileDescriptor, buffer, KERNEL_DATA_LENG);
        if (bytesRead > 0) {
            printf("Read length: %d\n", bytesRead);

            // Copy kernel time
            for (int i = 0; i < 8; i++) {
                usbMonitor.info.kernel_time[i] = buffer[i];
                printf("kernel_time[%d] = 0x%x \n", i, usbMonitor.info.kernel_time[i]);
            }
            // memcpy(usbMonitor.info.kernel_time, buffer, 8);

            // Get device status
            usbMonitor.info._flag = buffer[8];

            // Copy device name
            memcpy(usbMonitor.info.USB_Name, buffer + 9, bytesRead - 9);
            // usbMonitor.info.USB_Name[bytesRead - 9] = '\0'; // Add null terminator

            if (usbMonitor.info._flag == 1) {
                printf("USB %s -> Plugged In\n", usbMonitor.info.USB_Name);
            } else {
                printf("USB %s -> Plugged Out\n", usbMonitor.info.USB_Name);
            }

            printf("\n");
        }
    }

    // Close the file descriptor
    close(fileDescriptor);

    return 0;
}
