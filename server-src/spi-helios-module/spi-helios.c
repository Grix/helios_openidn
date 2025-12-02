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

#define COMMAND_RESET_MCU 5

struct heliospro_buffer {
	uint8_t* data;
	uint16_t size;
};

static atomic_t stop = ATOMIC_INIT(0);
static struct task_struct* spi_thread;
static struct gpio_desc* bufferstatus_gpiod;
static struct gpio_desc* mcureset_gpiod;
static struct spi_device* spi;

static struct heliospro_buffer frameBuffer1;
static struct heliospro_buffer frameBuffer2;

static volatile struct heliospro_buffer* frameBuffer;
static volatile struct heliospro_buffer* newFrameBuffer;

DECLARE_WAIT_QUEUE_HEAD(newFrameWaitQueue);
DECLARE_WAIT_QUEUE_HEAD(statusSignalWaitQueue);

//static DEFINE_MUTEX(lock);
static atomic_t newFrameReady = ATOMIC_INIT(0);

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
		
		if (!atomic_read(&newFrameReady))
			continue;

        //mutex_lock(&lock);
		volatile struct heliospro_buffer* previousFrameBuffer = frameBuffer;
		frameBuffer = newFrameBuffer;
		newFrameBuffer = previousFrameBuffer;
		atomic_set(&newFrameReady, 0);
        //mutex_unlock(&lock);
		wake_up_interruptible(&newFrameWaitQueue);
		int ret = spi_write(spi, frameBuffer->data, frameBuffer->size);
		if (ret)
			pr_warn("Failed to write SPI message: %d\n", ret);

    }
    return 0;
}

static ssize_t reset_mcu(void)
{
	int ret = gpiod_direction_output(mcureset_gpiod, 0);
	if (!ret)
		return ret;
	fsleep(50000);
	gpiod_direction_input(mcureset_gpiod);
	bool ok = false;
	for (int i = 0; i < 15; i++)
	{
		int status = gpiod_get_value(bufferstatus_gpiod);
		if (status == 1)
		{
			ok = true;
			break;
		}
		fsleep(200000);
	}
	if (!ok)
	{
		pr_err("Failed to read status from MCU afte reset");
		return -ETIME;
	}
	else
		return 2;
}

static ssize_t heliospro_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	if (count > BUF_SIZE || count < 2)
        return -EINVAL;
	
	if (count == 2 && buf[0] == 'C')
	{
		// Command write, do not forward to SPI
		if (atomic_read(&stop))
			return -ESHUTDOWN;
		
		if (buf[1] == COMMAND_RESET_MCU)
		{
			atomic_set(&newFrameReady, 0);
			int ret = reset_mcu();
			wake_up_interruptible(&newFrameWaitQueue);
			return ret;
		}
		
		return -EBADMSG;
	}
	
	wait_event_interruptible(newFrameWaitQueue, !atomic_read(&newFrameReady) || atomic_read(&stop));
	
	if (atomic_read(&stop))
		return -ESHUTDOWN;
	
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

#define READ_BUFSIZE 10

static ssize_t heliospro_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	uint8_t data[READ_BUFSIZE];
	int i;

	// Only support reads at offset 0, exactly READ_BUFSIZE bytes
	if (*ppos != 0)
		return 0; // No more data to read (mimic EOF after first successful read)

	if (count != READ_BUFSIZE)
		return -EINVAL; // Only allow reads of exactly 10 bytes

	for (i = 0; i < READ_BUFSIZE; i++)
		data[i] = 0;
	
	data[0] = 'G';
	data[1] = gpiod_get_value(bufferstatus_gpiod);
	data[2] = atomic_read(&newFrameReady);

	if (copy_to_user(buf, data, READ_BUFSIZE))
		return -EFAULT;

    *ppos += READ_BUFSIZE;

    return READ_BUFSIZE;
}

static const struct file_operations myspi_fileops = {
    .owner  = THIS_MODULE,
    .write  = heliospro_write,
	.read 	= heliospro_read
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

    // Get the GPIO from device tree
    bufferstatus_gpiod = devm_gpiod_get(&spi->dev, "bufferstatus", GPIOD_IN);
    if (IS_ERR(bufferstatus_gpiod)) 
	{
        dev_err(&spi->dev, "Failed to get bufferstatus GPIO: %ld\n", PTR_ERR(bufferstatus_gpiod));
        return PTR_ERR(bufferstatus_gpiod);
    }
    mcureset_gpiod = devm_gpiod_get(&spi->dev, "mcureset", GPIOD_IN);
    if (IS_ERR(mcureset_gpiod)) 
	{
        dev_err(&spi->dev, "Failed to get mcu reset GPIO: %ld\n", PTR_ERR(mcureset_gpiod));
        return PTR_ERR(mcureset_gpiod);
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
	
	ret = gpiod_direction_input(bufferstatus_gpiod);
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