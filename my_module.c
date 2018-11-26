#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#define DRIVER_NAME "my_gpio_driver"

#define GPFSEL0_OFFSET 0x0000 /* GPIO Function Select Registers (GPFSELn) */
#define GPSET0_OFFSET 0x001C /* GPIO Pin Output Set Registers (GPSETn) */
#define GPCLR0_OFFSET 0x0028 /* GPIO Pin Output Clear Registers (GPCLRn) */
#define GPLEV0_OFFSET 0x0034 /* GPIO Pin Level Registers (GPLEVn) */

#define NUM_PIN_EACH_SEL_REG 10 /* the number of pins in each SEL register */
#define NUM_PIN_EACH_REG 32 /* the number of pins in each register */
#define REG_GAP 4
#define RELATIVE_GPIO_ADDRESS 0x00200000
#define PERIPHERAL_ADDRESS 0x3F000000
#define GPIO_ADDRESS (PERIPHERAL_ADDRESS + RELATIVE_GPIO_ADDRESS)

#define DEVICE_FILE "/dev/mem"
#define MEMORY(addr) (*((volatile unsigned int*)(addr)))

#define PIN_MODE_IN 0
#define PIN_MODE_OUT 1

#define PIN_ON 1
#define PIN_OFF 0

#define WRITE_SEL(num, mode) (mode << (num * 3))
#define WRITE_SET_CLR(num) (1 << num)

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Kazuki Onishi <princeontrojanhorse@gmail.com>");
MODULE_DESCRIPTION("my kernel module");

static const unsigned int MINOR_NUMBER_START = 0; /* the minor number starts at */
static const unsigned int NUMBER_MINOR_NUMBER = 1; /* the number of minor numbers */
static unsigned int major_number; /* the major number of the device */
static struct cdev my_char_dev; /* character device */
static struct class* my_char_dev_class = NULL; /* class object */

int file_descriptor;
unsigned int base_address;
unsigned int pin_number;

int gpio_init(void);
void gpio_terminate(void);
void pin_mode(unsigned int pin_number, int mode);
void pin_on(unsigned int pin_number);
void pin_off(unsigned int pin_number);
int pin_read(unsigned int pin_number);

static int __init my_init(void);
static void __exit my_exit(void);
static int my_open(struct inode *inode, struct file *filp);
static int my_close(struct inode *inode, struct file *filp);
static ssize_t my_read(struct file * file, char * buff, size_t count, loff_t *pos);
static ssize_t my_write(struct file * file, const char * buff, size_t count, loff_t *pos);
static long my_ioctl(struct file *filp,unsigned int cmd, unsigned long arg);

struct file_operations my_file_ops = {
  .owner = THIS_MODULE,
  .open = my_open,
  .release = my_close,
  .read = my_read,
  .write = my_write,
	.unlocked_ioctl = my_ioctl, /* 64 bits */
  .compat_ioctl   = my_ioctl, /* 32 bits */
};


static int my_init(void)
{
	int alloc_ret;
	int cdev_err;
	dev_t dev;
	
  printk(KERN_INFO "my_init() is called\n");

  /* get not assigned major numbers */
	alloc_ret = alloc_chrdev_region(&dev, MINOR_NUMBER_START, NUMBER_MINOR_NUMBER, DRIVER_NAME);
  if (alloc_ret != 0) {
    printk(KERN_ERR "failed to alloc_chrdev_region()\n");
    return -1;
  }

  /* get one number from the not-assigend numbers */
  major_number = MAJOR(dev);
  dev = MKDEV(major_number, MINOR_NUMBER_START);
  
  /* register cdev and function table */
  cdev_init(&my_char_dev, &my_file_ops);
	my_char_dev.owner = THIS_MODULE;

  /* register the driver */
  cdev_err = cdev_add(&my_char_dev, dev, NUMBER_MINOR_NUMBER);
  if (cdev_err != 0) {
    printk(KERN_ERR "failed to cdev_add()\n");
    unregister_chrdev_region(dev, NUMBER_MINOR_NUMBER);
    return -1;
  }

  /* register as a class */
  my_char_dev_class = class_create(THIS_MODULE, "my_device");
  if (IS_ERR(my_char_dev_class)) {
    printk(KERN_ERR "class_create()\n");
    cdev_del(&my_char_dev);
    unregister_chrdev_region(dev, NUMBER_MINOR_NUMBER);
    return -1;
  }
  
  /* create "/sys/class/my_device/my_device_*" */
  for (int minor_number = MINOR_NUMBER_START; minor_number < NUMBER_MINOR_NUMBER; minor_number++) {
    device_create(my_char_dev_class, NULL, MKDEV(major_number, minor_number), NULL, "my_device_%d");
  }
  
  return 0;
}

static void my_exit(void)
{
  printk(KERN_INFO "my_exit() is called\n");

  dev_t dev = MKDEV(major_number, MINOR_NUMBER_START);

  /* remove "/sys/class/my_device/my_device_*" */
  for (int minor_number = MINOR_NUMBER_START; minor_number < NUMBER_MINOR_NUMBER; minor_number++) {
    device_destroy(my_char_dev_class, MKDEV(major_number, minor_number));
  }

  /* remove class */
  class_destroy(my_char_dev_class);

  /* remove driver */
  cdev_del(&my_char_dev);

  /* release the major number */
  unregister_chrdev_region(dev, NUMBER_MINOR_NUMBER);
}

static int my_open(struct inode *inode, struct file *filp)
{
  printk(KERN_INFO "my_open() is called\n");
  base_address = (int)ioremap_nocache(GPIO_ADDRESS, PAGE_SIZE);
  return 0;
}

int my_close(struct inode *inode, struct file *filp)
{
  printk(KERN_INFO "my_close() is called\n");
  iounmap((void*)base_address);
  return 0;
}

struct ioctl_data {
  int mode;
  unsigned int pin_number;
};

static long my_ioctl(struct file *filp,unsigned int cmd, unsigned long arg)
{
  printk(KERN_INFO "my_ioctl() is called\n");
  struct ioctl_data* data = (struct ioctl_data*)arg;
  int mode = data->mode;
  pin_number = data->pin_number;

  int num_reg = (pin_number / NUM_PIN_EACH_SEL_REG);
  int offset = (pin_number % NUM_PIN_EACH_SEL_REG);
  int addr = base_address + GPFSEL0_OFFSET + (REG_GAP * num_reg);
  MEMORY(addr) = WRITE_SEL(offset, mode);
  return 0L;
}

static ssize_t my_write(struct file * file, const char * buff, size_t count, loff_t *pos)
{
  int mode;
  get_user(mode, &buff[0]);

  int num_reg = (pin_number / NUM_PIN_EACH_REG);
  int offset = (pin_number % NUM_PIN_EACH_REG);
  int addr;

  if (mode == PIN_ON) {
    addr = base_address + GPSET0_OFFSET + (REG_GAP * num_reg);
  } else if (mode == PIN_OFF) {
    addr = base_address + GPCLR0_OFFSET + (REG_GAP * num_reg);
  }

  MEMORY(addr) = WRITE_SET_CLR(offset);
  return count;
}

static ssize_t my_read(struct file * file, char * buff, size_t count, loff_t *pos)
{
  int num_reg = (pin_number / NUM_PIN_EACH_REG);
  int offset = (pin_number % NUM_PIN_EACH_REG);
  int addr = base_address + GPLEV0_OFFSET + (REG_GAP * num_reg);

  int value = ((MEMORY(addr) >> offset) & 1);
  char return_val = value == 0 ? '0' : '1';
  put_user(return_val, &buff[0]);
  return count;
}

module_init(my_init);
module_exit(my_exit);
