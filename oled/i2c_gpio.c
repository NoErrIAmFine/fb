#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include "i2c_gpio.h"

/* 使用i2c接口总是出现莫名的错误，这里使用gpio来软件模拟i2c */

static inline void i2c_gpio_start(struct gpio_desc *scl_gpio,struct gpio_desc *sda_gpio)
{
    I2C_GPIO_SET(sda_gpio);
    I2C_GPIO_SET(scl_gpio);
    udelay(50);
    I2C_GPIO_CLR(sda_gpio);
    udelay(50);
    I2C_GPIO_CLR(scl_gpio);
}

static inline void i2c_gpio_stop(struct gpio_desc *scl_gpio,struct gpio_desc *sda_gpio)
{
    
}

static inline int i2c_gpio_wait_ack(struct gpio_desc *scl_gpio,struct gpio_desc *sda_gpio)
{
    I2C_GPIO_SET(scl_gpio);
    udelay(50);
    gpiod_direction_input(sda_gpio);
    if(gpiod_get_value(sda_gpio)){
        printk("no ack!\n");
        return -1;
    }
    I2C_GPIO_CLR(sda_gpio);
    gpiod_direction_output(sda_gpio,1);
    return 0;
}

static inline void i2c_gpio_write_byte(struct gpio_desc *scl_gpio,struct gpio_desc *sda_gpio,u8 data)
{
    u8 i = 0;
    for(i = 0 ; i < 8 ; i++){
        I2C_GPIO_CLR(scl_gpio);
        udelay(50);
        if(data &0x80){
            I2C_GPIO_SET(sda_gpio);
            udelay(50);
        }else{
            I2C_GPIO_CLR(sda_gpio);
            udelay(50);
        }
        I2C_GPIO_SET(scl_gpio);
        udelay(50);
        I2C_GPIO_CLR(scl_gpio);
        udelay(50);
        data <<= 1;
    }
}

/* 
 * @param : desc - 指向一个指针数组，数组中有两个gpio_desc指针，分别对应scl和sda
 */
int i2c_gpio_transfer(struct gpio_desc **desc,struct i2c_msg *msgs,int num)
{
    struct gpio_desc *scl_gpio = *desc++;
    struct gpio_desc *sda_gpio = *desc;
    struct i2c_msg *msg;
    u8 data;
    u8 *buf;
    int i,j;
    

    if(!scl_gpio || !sda_gpio){
        printk("%s:invalid gpio!\n",__func__);
        return -1;
    }

    /* 依次处理各个i2c_msg */
    for(i = 0 ; i < num ; i++){
        msg = msgs + i;
        buf = msg->buf;
        /* 先构造第一个要发送的字节 */
        data = (msg->addr << 1) | (msg->flags ? 1 : 0);

        /* 发送第一个设备地址字节 */
        i2c_gpio_start(scl_gpio,sda_gpio);
        i2c_gpio_write_byte(scl_gpio,sda_gpio,data);
        if(i2c_gpio_wait_ack(scl_gpio,sda_gpio)){
            printk("%s:transfer failed! no ack\n",__func__);
            return -1;
        }
        
        if(msg->flags == 0){
            /* 写模式 */
            for(j = 0 ; j < msg->len ; j++){
                i2c_gpio_write_byte(scl_gpio,sda_gpio,*buf++);
                if(i2c_gpio_wait_ack(scl_gpio,sda_gpio)){
                    printk("%s:transfer failed! no ack\n",__func__);
                    return -1;
                }
            }
            i2c_gpio_stop(scl_gpio,sda_gpio);
        }else{
            /* 读模式 */
            printk("read mode is not supported now\n");
        }
    }
    return i;
}