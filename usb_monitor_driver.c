/**
 * Linux内核模块，用于监控USB设备的插拔状态，并将相关信息提供给用户空间程序。
 * 
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/slab.h>



#define LOGI(...)	(pr_info(__VA_ARGS__))
#define LOGE(...)	(pr_err(__VA_ARGS__))

#define MESSAGE_BUFFER_SIZE 512

struct usb_message_t {
    signed long long kernel_time;         
    char    _flag;         // 0表示拔出设device/bus 1表示插入device，2表示插入bus 
    char    USB_Name[31];
};


struct usb_monitor_t {
    struct usb_message_t    message[MESSAGE_BUFFER_SIZE];
    int                     message_count;
    int                     read_index;
    int                     write_index;
    wait_queue_head_t       usb_monitor_queue;
    struct mutex            usb_monitor_mutex;
    char*                   flag;
};

static struct usb_monitor_t *monitor;
static char *TAG = "MONITOR";

static ssize_t usb_monitor_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos) {
    int index;
    size_t message_size = sizeof(struct usb_message_t);

    if (size < message_size) {
		LOGE("%s:read size is smaller than message size!\n", TAG);
		return -EINVAL;
	}

    wait_event_interruptible(monitor->usb_monitor_queue, monitor->message_count > 0);

    mutex_lock(&monitor->usb_monitor_mutex);

    if (monitor->message_count > 0) {
        index = monitor->read_index;
        if (copy_to_user(buf, &monitor->message[index], message_size)) {
            mutex_unlock(&monitor->usb_monitor_mutex);
            return -EFAULT;
        }

        printk(KERN_INFO "Read out %s, left num is %d\n", monitor->message[index].USB_Name, monitor->message_count);

        monitor->read_index++;
        if (monitor->read_index >= MESSAGE_BUFFER_SIZE) {
            monitor->read_index = 0;
        }
        monitor->message_count--;
    }
    mutex_unlock(&monitor->usb_monitor_mutex);

    return message_size;
}

static const struct file_operations usb_monitor_fops = {
    .owner = THIS_MODULE,
    .read = usb_monitor_read,
    .write = NULL
};


int write_usb_status_to_message(char status, struct usb_device *usb_dev) {

    int index;

    // 获取写入索引
    index = monitor->write_index;

    // 记录当前时间
    // memcpy(monitor->message[index].kernel_time,  ktime_to_ns(ktime_get()),  sizeof(monitor->message[index].kernel_time));
    monitor->message[index].kernel_time = ktime_to_ns(ktime_get());

    // 处理设备名称
    if (usb_dev->product == NULL) {
        // 设备没有名称时，设置为"NULL"
        memcpy(monitor->message[index].USB_Name, "NULL", 4);
        printk("### write_usb_status_to_message: Device name is NULL\n");
    } else {
        // 拷贝设备名称到消息结构体
        printk("### write_usb_status_to_message: %ld\n", strlen(usb_dev->product));
        memcpy(monitor->message[index].USB_Name, usb_dev->product, strlen(usb_dev->product));
    }

    // 记录USB状态
    monitor->message[index]._flag = status;

    // 更新消息数量
    if (monitor->message_count < MESSAGE_BUFFER_SIZE)
        monitor->message_count++;

    // 更新写入索引
    monitor->write_index++;
    if (monitor->write_index >= MESSAGE_BUFFER_SIZE)
        monitor->write_index = 0;

    return index;
}

static int usb_state_change_notify(struct notifier_block *self, unsigned long action, void *dev) {
    
    struct usb_device *usb_dev = (struct usb_device *)dev;
    int index;

    // 互斥锁，保护monitor中的数据
    mutex_lock(&monitor->usb_monitor_mutex);

    /**
     * 根据USB设备的状态进行处理
     * USB_DEVICE_ADD：表示USB设备被添加到系统中，也就是设备的插入事件。
     * USB_DEVICE_REMOVE：表示USB设备被从系统中移除，也就是设备的拔出事件。
     * USB_DEVICE_BIND：表示USB设备与其驱动程序进行绑定。
     * USB_DEVICE_UNBIND：表示USB设备与其驱动程序解除绑定。
     * USB_BUS_ADD：表示USB总线（控制器）被添加到系统中。
     * USB_BUS_REMOVE：表示USB总线（控制器）被从系统中移除。
    */
    switch (action) {
        case USB_DEVICE_ADD:
            index = write_usb_status_to_message(1, usb_dev);
            printk(KERN_INFO "### USB_DEVICE_ADD: Device name is %s, Message count: %d\n",
                    monitor->message[index].USB_Name, monitor->message_count);
            wake_up_interruptible(&monitor->usb_monitor_queue);
            break;

        case USB_DEVICE_REMOVE:
            index = write_usb_status_to_message(0, usb_dev);
            printk(KERN_INFO "### USB_DEVICE_REMOVE: Device name is %s, Message count: %d\n",
                    monitor->message[index].USB_Name, monitor->message_count);
            wake_up_interruptible(&monitor->usb_monitor_queue);
            break;

        case USB_BUS_ADD:
            index = write_usb_status_to_message(1, usb_dev);
            printk(KERN_INFO "### USB_BUS_ADD: Bus name is %s\n", monitor->message[index].USB_Name);
            wake_up_interruptible(&monitor->usb_monitor_queue);
            break;

        case USB_BUS_REMOVE:
            index = write_usb_status_to_message(0, usb_dev);
            printk(KERN_INFO "### USB_BUS_REMOVE: Bus name is %s\n", monitor->message[index].USB_Name);
            wake_up_interruptible(&monitor->usb_monitor_queue);
            break;
    }

    mutex_unlock(&monitor->usb_monitor_mutex);
    return NOTIFY_OK;
}

static struct notifier_block usb_nb = {
    .notifier_call = usb_state_change_notify,
    .next = NULL,
};

static int usb_monitor_init(void) {

    // 分配内存空间
    monitor = kzalloc(sizeof(struct usb_monitor_t), GFP_KERNEL);

    if (!monitor) {
        LOGE("%s: failed to kzalloc\n", TAG);
        return -ENOMEM;
    }

    // 初始化环形队列读写地址和大小
    monitor->message_count = 0;
    monitor->read_index = 0;
    monitor->write_index = 0;
    monitor->flag = "usb_monitor_init is initialized!\n";

    // 在/proc下创建虚拟文件，用于用户和内核交互，这里只有读的要求
    proc_create("usb_monitor", 0644, NULL, &usb_monitor_fops);

    // 初始化等待队列，用于让usb_monitor_read没有数据时挂起，避免频繁调用
    init_waitqueue_head(&monitor->usb_monitor_queue);
    printk(KERN_INFO "usb_monitor_init.\n");

    // 注册USB状态改变通知
    usb_register_notify(&usb_nb);

    return 0;
}

static void usb_monitor_exit(void) {

    // 删除之前创建的虚拟文件
    remove_proc_entry("usb_monitor", NULL);

    // 取消注册USB状态改变通知
    usb_unregister_notify(&usb_nb);

    printk(KERN_INFO "usb_monitor_exit.\n");

    // 释放分配的内存空间
    kfree(monitor);
}


module_init(usb_monitor_init);
module_exit(usb_monitor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("enero0826@outlook.com");
