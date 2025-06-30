#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>

#define HELIOS_MAXFRAMESIZE 210			// In points
#define HELIOS_BYTESPERFRAME 18
#define HELIOS_FRAMEHEADER_SIZE 16
#define HELIOS_FRAMEFOOTER_SIZE 4

#define BUF_SIZE  (HELIOS_MAXFRAMESIZE * HELIOS_BYTESPERFRAME + HELIOS_FRAMEHEADER_SIZE + HELIOS_FRAMEFOOTER_SIZE)

struct heliospro_buffer {
	uint8_t* data;
	uint16_t size;
};

static atomic_t stop = ATOMIC_INIT(0);
static struct task_struct* spi_thread;
static struct gpio_desc* bufferstatus_gpiod;
static struct spi_device* spi;

static struct heliospro_buffer frameBuffer1;
static struct heliospro_buffer frameBuffer2;

static volatile struct heliospro_buffer* frameBuffer;
static volatile struct heliospro_buffer* newFrameBuffer;

DECLARE_WAIT_QUEUE_HEAD(newFrameWaitQueue);
DECLARE_WAIT_QUEUE_HEAD(statusSignalWaitQueue);
//static wait_queue_head_t newFrameWaitQueue;
//static wait_queue_head_t statusSignalWaitQueue;

static DEFINE_MUTEX(lock);
static atomic_t newFrameReady = ATOMIC_INIT(0);
//static atomic_t bufferStatusReady = ATOMIC_INIT(0);

static int bufferstatus_irq;


static int spi_worker(void *data)
{
    // Set real-time high priority (RT FIFO)
	sched_set_fifo(current);

    while (!kthread_should_stop()) 
	{
        // Wait until there's data or we're told to stop
        wait_event_interruptible(newFrameWaitQueue,  atomic_read(&newFrameReady) || atomic_read(&stop) || kthread_should_stop());
        
        if (atomic_read(&stop) || kthread_should_stop())
            break;
		
		wait_event_interruptible(statusSignalWaitQueue,  gpiod_get_value(bufferstatus_gpiod) || atomic_read(&stop) || kthread_should_stop());
		
		if (atomic_read(&stop) || kthread_should_stop())
            break;

        //mutex_lock(&lock);
		volatile struct heliospro_buffer* previousFrameBuffer = frameBuffer;
		frameBuffer = newFrameBuffer;
		newFrameBuffer = previousFrameBuffer;
		atomic_set(&newFrameReady, 0);
        //mutex_unlock(&lock);
		wake_up_interruptible(&newFrameWaitQueue);
		spi_write(spi, frameBuffer->data, frameBuffer->size);

    }
    return 0;
}

static ssize_t heliospro_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	if (count > BUF_SIZE)
        return -EINVAL;
	
	wait_event_interruptible(newFrameWaitQueue,  !atomic_read(&newFrameReady) || atomic_read(&stop));
	
	if (atomic_read(&stop))
		return -ESHUTDOWN;
	
	//if (!lock.)
	//	return -EINVAL;
	
	//mutex_lock(&lock);
	
	if (copy_from_user(newFrameBuffer->data, buf, count)) 
	{
		//mutex_unlock(&lock);
        return -EFAULT;
    }
	newFrameBuffer->size = count;
	atomic_set(&newFrameReady, 1);
	//mutex_unlock(&lock);
	wake_up_interruptible(&newFrameWaitQueue);
	
    return count;
}

static const struct file_operations myspi_fileops = {
    .owner  = THIS_MODULE,
    .write  = heliospro_write,
    // .read, .ioctl, etc., as needed...
};

static struct miscdevice heliospro_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,            // Allocates a free minor
    .name  = "heliospro-spi",               // /dev/heliospro-spi
    .fops  = &myspi_fileops,    
};

static irqreturn_t bufferstatus_isr(int irq, void *data)
{
    wake_up_interruptible(&statusSignalWaitQueue);

    return IRQ_HANDLED;
}

static int my_spi_probe(struct spi_device *_spi)
{
	spi = _spi;
    dev_info(&spi->dev, "HeliosPRO SPI device probed\n");
    // Initialization, buffer allocations, etc.

    // Get the GPIO per my DT property in the node ("bufferstatus-gpio")
    bufferstatus_gpiod = devm_gpiod_get(&spi->dev, "bufferstatus", GPIOD_IN);
    if (IS_ERR(bufferstatus_gpiod)) 
	{
        dev_err(&spi->dev, "Failed to get bufferstatus GPIO: %ld\n", PTR_ERR(bufferstatus_gpiod));
        // You can ignore or abort, up to you:
        return PTR_ERR(bufferstatus_gpiod);
    }
	
	bufferstatus_irq = gpiod_to_irq(bufferstatus_gpiod);
    if (bufferstatus_irq < 0) 
	{
        pr_err("Failed to get IRQ for bufferstatus GPIO\n");
        return bufferstatus_irq;
    }

    // Request an IRQ for rising edge (pin goes high)
    int ret = request_irq(bufferstatus_irq, bufferstatus_isr, IRQF_TRIGGER_RISING, "bufferstatus_irq", NULL);
    if (ret) 
	{
        pr_err("Failed to request IRQ %d\n", bufferstatus_irq);
        return ret;
    }

    // Example: read its value
    int value = gpiod_get_value(bufferstatus_gpiod);
    dev_info(&spi->dev, "Buffer status input initial value: %d\n", value);
	
	frameBuffer1.data = (uint8_t*)devm_kmalloc(&spi->dev, BUF_SIZE, GFP_KERNEL | GFP_DMA);
	if (!frameBuffer1.data) 
	{
        pr_err("Failed to alloc memory to buffer 1\n");
        return -ENOMEM;
    }
	frameBuffer2.data = (uint8_t*)devm_kmalloc(&spi->dev, BUF_SIZE, GFP_KERNEL | GFP_DMA);
	if (!frameBuffer2.data) 
	{
        pr_err("Failed to alloc memory to buffer 1\n");
        return -ENOMEM;
    }
	newFrameBuffer = &frameBuffer1;
	frameBuffer = &frameBuffer2;
	
    ret = misc_register(&heliospro_miscdev);
    if (ret) 
	{
        pr_err("Could not register misc device\n");
        return ret;
    }

    spi_thread = kthread_run(spi_worker, NULL, "spi_writer_thread");
    if (IS_ERR(spi_thread)) 
	{
        pr_err("Failed to create SPI writer thread\n");
        return PTR_ERR(spi_thread);
    }
    return 0;

    return 0;
}

static void my_spi_remove(struct spi_device *spi)
{
    // Cleanup
	atomic_set(&stop, 1);
	
	if (bufferstatus_irq > 0)
        free_irq(bufferstatus_irq, NULL);
	
	misc_deregister(&heliospro_miscdev);
	
	wake_up_interruptible(&newFrameWaitQueue);
	wake_up_interruptible(&statusSignalWaitQueue);
    if (spi_thread)
        kthread_stop(spi_thread);
}

static const struct of_device_id my_spi_dt_ids[] = {
    { .compatible = "custom,spi-helios" },
    {},
};
MODULE_DEVICE_TABLE(of, my_spi_dt_ids);

static const struct spi_device_id my_spi_id[] = {
    { "custom,spi-helios", 0 },
    {}
};
MODULE_DEVICE_TABLE(spi, my_spi_id);

static struct spi_driver my_spi_driver = {
    .driver = {
        .name = "spi-helios",
        .of_match_table = my_spi_dt_ids,
    },
    .id_table = my_spi_id,
    .probe = my_spi_probe,
    .remove = my_spi_remove,
};

module_spi_driver(my_spi_driver);

MODULE_AUTHOR("Gitle Mikkelsen");
MODULE_DESCRIPTION("HeliosPRO buffer chip SPI driver");
MODULE_LICENSE("GPL");