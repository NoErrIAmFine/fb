#ifndef __I2C_GPIO_H
#define __I2C_GPIO_H

#define I2C_GPIO_CLR(scl_gpio)          \
do{                                     \
    if(scl_gpio)                        \
        gpiod_set_value(scl_gpio,0);    \
}while(0)

#define I2C_GPIO_SET(scl_gpio)          \
do{                                     \
    if(scl_gpio)                        \
        gpiod_set_value(scl_gpio,1);    \
}while(0)



int i2c_gpio_transfer(struct gpio_desc **desc,struct i2c_msg *msgs,int num);

#endif // !__I2C_GPIO_H