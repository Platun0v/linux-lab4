#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/spinlock.h> 
#include <linux/errno.h>

#include "platmod.h"

#define SUCCESS 0
#define DEVICE_NAME "platmod"
#define DEFAULT_STACK_SIZE 8

static struct class *cls;

static struct stack {
    int32_t *data;
    int length;
    int pointer;
    spinlock_t lock;
} stack;

static int stack_size = DEFAULT_STACK_SIZE;

int stack_init(void) {
    stack.data = kmalloc(stack_size * sizeof(int32_t), GFP_KERNEL);
    if (!stack.data) {
        return -ENOMEM;
    }
    stack.length = stack_size;
    stack.pointer = 0;
    return SUCCESS;
}

void stack_destroy(void) {
    kfree(stack.data);
}

int stack_push(int32_t value) {
    char *value_as_char = (char *)&value;
    printk(KERN_INFO "\tCurrent stack state: %d/%d", stack.pointer, stack.length);
    printk(KERN_INFO "Trying to push %d to stack", value);
    if (stack.pointer == stack.length) {
        printk(KERN_INFO "Stack is full");
        return -ENOSPC;
    }
    stack.data[stack.pointer++] = value;
    printk(KERN_INFO "Pushed %d to stack", value);
    // print value as chars 
    printk(KERN_INFO "Pushed %d to stack as chars: %c %c %c %c", value, value_as_char[0], value_as_char[1], value_as_char[2], value_as_char[3]);
    return SUCCESS;
}

int stack_pop(int32_t *value) {
    char *value_as_chars = (char *) value;
    printk(KERN_INFO "Trying to pop from stack");
    printk(KERN_INFO "\tCurrent stack state: %d/%d", stack.pointer, stack.length);
    if (stack.pointer == 0) {
        printk(KERN_INFO "\tStack is empty");
        return -ENODATA;
    }
    *value = stack.data[--stack.pointer];
    printk(KERN_INFO "Popped %d from stack", *value);
    // print value as chars
    printk(KERN_INFO "Popped %c%c%c%c from stack", value_as_chars[0], value_as_chars[1], value_as_chars[2], value_as_chars[3]);
    return SUCCESS;
}

int stack_set_size(int new_size) {
    int32_t *new_data;

    printk(KERN_INFO "Trying to set stack size to %d", new_size);
    if (new_size < 0) {
        printk(KERN_INFO "Invalid stack size");
        return -EINVAL;
    }
    new_data = kmalloc(new_size * sizeof(int32_t), GFP_KERNEL);
    printk(KERN_INFO "Allocated new data");

    if (!new_data) {
        return -ENOMEM;
    }

    memcpy(new_data, stack.data, stack.pointer * sizeof(int32_t));
    printk(KERN_INFO "Copied data");

    kfree(stack.data);
    printk(KERN_INFO "Freed old data");
    
    stack.data = new_data;
    stack.length = new_size;
    stack.pointer = stack.pointer > stack.length ? stack.length : stack.pointer;
    return SUCCESS;
}

static int device_open(struct inode *inode, struct file *file) {
    return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file) {
    return SUCCESS;
}

static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t * offset) {
    int bytes_read = 0;
    int res;
    int32_t value;

    while (bytes_read < length) {
        spin_lock(&stack.lock);
        res = stack_pop(&value);
        spin_unlock(&stack.lock);

        if (res < 0 && bytes_read == 0) {
            return 0;
        } else if (res < 0) {
            break;
        }
        if (copy_to_user(buffer + bytes_read, &value, sizeof(int32_t))) {
            return -EFAULT;
        }
        bytes_read += sizeof(int32_t);
    }
    return bytes_read;
}

static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t * off) {
    int32_t value; // When write more bytes than space left, return bytes written nor error
    int sum_ret = 0;
    int ret;
    int i;

    printk(KERN_INFO "Trying to write %lu bytes", len);
    for (i = 0; i < len; i += sizeof(int32_t)) {
        value = 0;
        if (i + sizeof(int32_t) > len) {
            ret = copy_from_user(&value, buff + i, len - i);
            if (ret) {
                printk(KERN_ALERT "Failed to copy bytes from user");
                return -EFAULT;
            }
        }
        else if (copy_from_user(&value, buff + i, sizeof(int32_t))) {
            printk(KERN_ALERT "Failed to copy bytes from user");
            return -EFAULT;
        }
        
        spin_lock(&stack.lock);
        ret = stack_push(value);
        spin_unlock(&stack.lock);

        if (ret < 0 && sum_ret == 0) {
            return ret;
        }
        sum_ret += i + sizeof(int32_t) > len ? len - i : sizeof(int32_t);
    }
    printk(KERN_INFO "Wrote %d bytes", sum_ret);
    return sum_ret;
}

static long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
    int res;
    switch (ioctl_num) {
        case IOCTL_SET_STACK_SIZE:
            spin_lock(&stack.lock);
            res = stack_set_size(ioctl_param);
            spin_unlock(&stack.lock);
            break;
        default:
            return -EINVAL;
    }
    return res;
}

static struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release,
    .unlocked_ioctl = device_ioctl,
};


static int __init chardev2_init(void) {
    int ret_val;

    printk(KERN_INFO "Starting platmod\n");

    ret_val = register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops);    
    if (ret_val < 0) {
        printk(KERN_ALERT "Failed to register chardev with error code %d\n", ret_val);
        return ret_val;
    }
    
    cls = class_create(THIS_MODULE, DEVICE_FILE_NAME);
    device_create(cls, NULL, MKDEV(MAJOR_NUM, 0), NULL, DEVICE_FILE_NAME);

    printk(KERN_INFO "Device created on /dev/%s\n", DEVICE_FILE_NAME);

    ret_val = stack_init();
    if (ret_val < 0) {
        printk(KERN_INFO "Failed to create stack with error code %d", ret_val);
        return ret_val;
    }

    return ret_val;
}

static void __exit chardev2_exit(void) {
    printk(KERN_INFO "Exiting platmod\n");
    device_destroy(cls, MKDEV(MAJOR_NUM, 0));
    class_destroy(cls);
    
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);

    stack_destroy();
    printk(KERN_INFO "Stack destroyed\n");
}

module_init(chardev2_init);
module_exit(chardev2_exit);

MODULE_LICENSE("GPL");
