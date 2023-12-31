#ifndef __USB_INFO_H_
#define __USB_INFO_H_

#include <linux/types.h>
#include <linux/ioctl.h>
#include <stdint.h>
#include <stdlib.h>

#define DEV_NAME "/proc/usb_monitor"

#define KERNEL_DATA_LENG 128

size_t BUFFER_SIZE = 512;

/*
32 bytes in total
struct suspend_message_t
{
            s64 kernel_time;                 8 bytes                
            struct timeval timeval_utc;     16 bytes                   
            enum MESSAGE_TYPE status_old;    4 bytes 
            enum MESSAGE_TYPE status_new;    4 bytes                
};
*/
struct DataInfo{
    unsigned char kernel_time[8];       //8 Byte 
    // uint8_t utc_time[16];          //16 byte
    // uint8_t status_old[4];        //4 byte
    // uint8_t status_new[4];        //4 byte
    unsigned char  _flag;         // 0表示拔出设device/bus 1表示插入device，2表示插入bus 
    char  USB_Name[KERNEL_DATA_LENG];
};

class UsbMonitorInfo {
public:
    struct DataInfo info;
};

#endif
