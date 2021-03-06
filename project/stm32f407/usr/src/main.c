/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * 
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. 
 *
 * @file      main.c
 * @brief     main source file
 * @version   1.0.0
 * @author    Shifeng Li
 * @date      2021-02-21
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2021/02/21  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

#include "driver_amg8833_read_test.h"
#include "driver_amg8833_register_test.h"
#include "driver_amg8833_interrupt_test.h"
#include "driver_amg8833_basic.h"
#include "driver_amg8833_interrupt.h"
#include "shell.h"
#include "clock.h"
#include "delay.h"
#include "gpio.h"
#include "uart.h"
#include <stdlib.h>

/**
 * @brief global var definition
 */
uint8_t g_buf[256];                        /**< uart buffer */
uint16_t g_len;                            /**< uart buffer length */
uint8_t (*g_gpio_irq)(void) = NULL;        /**< gpio irq */

/**
 * @brief exti 0 irq
 * @note  none
 */
void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

/**
 * @brief     gpio exti callback
 * @param[in] pin is the gpio pin
 * @note      none
 */
void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == GPIO_PIN_0)
    {
        if (g_gpio_irq)
        {
            g_gpio_irq();
        }
    }
}

/**
 * @brief     interface receive callback
 * @param[in] type is the interrupt type
 * @note      none
 */
static void a_callback(uint8_t type)
{
    switch (type)
    {
        case AMG8833_STATUS_OVF_THS :
        {
            amg8833_interface_debug_print("amg8833: irq thermistor temperature output overflow.\n");
            
            break;
        }
        case AMG8833_STATUS_OVF_IRS :
        {
            amg8833_interface_debug_print("amg8833: irq temperature output overflow.\n");
            
            break;
        }
        case AMG8833_STATUS_INTF :
        {
            uint8_t res;
            uint8_t i, j;
            uint8_t level;
            uint8_t table[8][1];
            
            amg8833_interface_debug_print("amg8833: irq interrupt outbreak.\n");
            
            /* get table */
            res = amg8833_interrupt_get_table((uint8_t (*)[1])table);
            if (res != 0)
            {
                amg8833_interface_debug_print("amg8833: get table failed.\n");
            }
            else
            {
                for (i = 0; i < 8; i++)
                {
                    level = table[i][0];
                    for (j = 0; j < 8; j++)
                    {
                        if (((level >> (7 - j)) & 0x01) != 0)
                        {
                            amg8833_interface_debug_print("%d  ", 1);
                        }
                        else
                        {
                            amg8833_interface_debug_print("%d  ", 0);
                        }
                    }
                    amg8833_interface_debug_print("\n");
                }
            }
            
            break;
        }
        default :
        {
            amg8833_interface_debug_print("amg8833: unknown code.\n");
            
            break;
        }
    }
}

/**
 * @brief     amg8833 full function
 * @param[in] argc is arg numbers
 * @param[in] **argv is the arg address
 * @return    status code
 *             - 0 success
 *             - 1 run failed
 *             - 5 param is invalid
 * @note      none
 */
uint8_t amg8833(uint8_t argc, char **argv)
{
    if (argc == 1)
    {
        goto help;
    }
    else if (argc == 2)
    {
        if (strcmp("-i", argv[1]) == 0)
        {
            amg8833_info_t info;
            
            /* print amg8833 info */
            amg8833_info(&info);
            amg8833_interface_debug_print("amg8833: chip is %s.\n", info.chip_name);
            amg8833_interface_debug_print("amg8833: manufacturer is %s.\n", info.manufacturer_name);
            amg8833_interface_debug_print("amg8833: interface is %s.\n", info.interface);
            amg8833_interface_debug_print("amg8833: driver version is %d.%d.\n", info.driver_version / 1000, (info.driver_version % 1000) / 100);
            amg8833_interface_debug_print("amg8833: min supply voltage is %0.1fV.\n", info.supply_voltage_min_v);
            amg8833_interface_debug_print("amg8833: max supply voltage is %0.1fV.\n", info.supply_voltage_max_v);
            amg8833_interface_debug_print("amg8833: max current is %0.2fmA.\n", info.max_current_ma);
            amg8833_interface_debug_print("amg8833: max temperature is %0.1fC.\n", info.temperature_max);
            amg8833_interface_debug_print("amg8833: min temperature is %0.1fC.\n", info.temperature_min);
            
            return 0;
        }
        else if (strcmp("-p", argv[1]) == 0)
        {
            /* print pin connection */
            amg8833_interface_debug_print("amg8833: SCL connected to GPIOB PIN8.\n");
            amg8833_interface_debug_print("amg8833: SDA connected to GPIOB PIN9.\n");
            amg8833_interface_debug_print("amg8833: INT connected to GPIOB PIN0.\n");
            
            return 0;
        }
        else if (strcmp("-h", argv[1]) == 0)
        {
            /* show amg8833 help */
            
            help:
            amg8833_interface_debug_print("amg8833 -i\n\tshow amg8833 chip and driver information.\n");
            amg8833_interface_debug_print("amg8833 -h\n\tshow amg8833 help.\n");
            amg8833_interface_debug_print("amg8833 -p\n\tshow amg8833 pin connections of the current board.\n");
            amg8833_interface_debug_print("amg8833 -t reg (0 | 1)\n\trun amg8833 register test.\n");
            amg8833_interface_debug_print("amg8833 -t read (0 | 1) <times>\n\trun amg8833 read test.times means test times.\n");
            amg8833_interface_debug_print("amg8833 -t int (0 | 1) <times> <mode> <high> <low> <hysteresis>\n\trun amg8833 interrupt test."
                                          "times means test times.mode is the interrupt mode and it can be \"abs\" and \"diff\".");
            amg8833_interface_debug_print("high is the interrupt high level.low is the interrupt low level.hysteresis is the hysteresis level.\n");
            amg8833_interface_debug_print("amg8833 -c read (0 | 1) <times>\n\trun amg8833 read function.times means test times.\n");
            amg8833_interface_debug_print("amg8833 -c int (0 | 1) <times> <mode> <high> <low> <hysteresis>\n\trun amg8833 interrupt function."
                                          "times means test times.mode is the interrupt mode and it can be \"abs\" and \"diff\"."); 
            amg8833_interface_debug_print("high is the interrupt high level.low is the interrupt low level.hysteresis is the hysteresis level.\n");
            
            return 0;
        }
        else
        {
            return 5;
        }
    }
    else if (argc == 4)
    {
        /* run test */
        if (strcmp("-t", argv[1]) == 0)
        {
             /* reg test */
            if (strcmp("reg", argv[2]) == 0)
            {
                amg8833_address_t addr;
                
                if (strcmp("0", argv[3]) == 0)
                {
                    addr = AMG8833_ADDRESS_0;
                }
                else if (strcmp("1", argv[3]) == 0)
                {
                    addr = AMG8833_ADDRESS_1;
                }
                else
                {
                    return 5;
                }
                
                /* run reg test */
                if (amg8833_register_test(addr) != 0)
                {
                    return 1;
                }
                else
                {
                    return 0;
                }
            }
            
            /* param is invalid */
            else
            {
                return 5;
            }
        }
        
        /* param is invalid */
        else
        {
            return 5;
        }
    }
    else if (argc == 5)
    {
        /* run test */
        if (strcmp("-t", argv[1]) == 0)
        {
             /* read test */
            if (strcmp("read", argv[2]) == 0)
            {
                amg8833_address_t addr;
                
                if (strcmp("0", argv[3]) == 0)
                {
                    addr = AMG8833_ADDRESS_0;
                }
                else if (strcmp("1", argv[3]) == 0)
                {
                    addr = AMG8833_ADDRESS_1;
                }
                else
                {
                    return 5;
                }
                
                /* run read test */
                if (amg8833_read_test(addr, atoi(argv[4])) != 0)
                {
                    return 1;
                }
                else
                {
                    return 0;
                }
            }
            /* param is invalid */
            else
            {
                return 5;
            }
        }
        else if (strcmp("-c", argv[1]) == 0)
        {
             /* read function */
            if (strcmp("read", argv[2]) == 0)
            {
                amg8833_address_t addr;
                uint32_t i, j, k, times;
                uint8_t res;
                
                if (strcmp("0", argv[3]) == 0)
                {
                    addr = AMG8833_ADDRESS_0;
                }
                else if (strcmp("1", argv[3]) == 0)
                {
                    addr = AMG8833_ADDRESS_1;
                }
                else
                {
                    return 5;
                }
                
                /* get times */
                times = atoi(argv[4]);
                
                /* init */
                res = amg8833_basic_init(addr);
                if (res != 0)
                {
                    return 1;
                }
                
                /* delay 1000 ms */
                amg8833_interface_delay_ms(1000);
                
                for (i = 0; i < times; i++)
                {
                    float temp[8][8];
                    float tmp;
                    
                    /* read temperature array */
                    res = amg8833_basic_read_temperature_array(temp);
                    if (res != 0)
                    {
                        amg8833_interface_debug_print("amg8833: read temperature array failed.\n");
                        (void)amg8833_basic_deinit();
                        
                        return 1;
                    }
                    else
                    {
                        for (j = 0; j < 8; j++)
                        {
                            for (k = 0; k < 8; k++)
                            {
                                amg8833_interface_debug_print("%0.2f  ", temp[j][k]);
                            }
                            amg8833_interface_debug_print("\n");
                        }
                    }
                    
                    /* read temperature */
                    res = amg8833_basic_read_temperature((float *)&tmp);
                    if (res != 0)
                    {
                        (void)amg8833_basic_deinit();
                       
                        return 1;
                    }
                    else
                    {
                        amg8833_interface_debug_print("amg8833: temperature is %0.3fC.\n", tmp);
                    }
                    
                    /* delay 1000 ms */
                    amg8833_interface_delay_ms(1000);
                }
                
                return amg8833_basic_deinit();
            }
            /* param is invalid */
            else
            {
                return 5;
            }
        }
        /* param is invalid */
        else
        {
            return 5;
        }
    }
    else if (argc == 9)
    {
        /* run test */
        if (strcmp("-t", argv[1]) == 0)
        {
             /* read test */
            if (strcmp("int", argv[2]) == 0)
            {
                amg8833_address_t addr;
                amg8833_interrupt_mode_t mode;
                
                if (strcmp("0", argv[3]) == 0)
                {
                    addr = AMG8833_ADDRESS_0;
                }
                else if (strcmp("1", argv[3]) == 0)
                {
                    addr = AMG8833_ADDRESS_1;
                }
                else
                {
                    return 5;
                }
                
                if (strcmp("abs", argv[5]) == 0)
                {
                    mode = AMG8833_INTERRUPT_MODE_ABSOLUTE;
                    amg8833_interface_debug_print("amg8833: absolute mode.\n");
                }
                else if (strcmp("diff", argv[5]) == 0)
                {
                    mode = AMG8833_INTERRUPT_MODE_DIFFERENCE;
                    amg8833_interface_debug_print("amg8833: difference mode.\n");
                }
                else
                {
                    return 5;
                }
                
                /* run interrupt test */
                g_gpio_irq = amg8833_interrupt_test_irq_handler;
                if (gpio_interrupt_init() != 0)
                {
                    g_gpio_irq = NULL;
                }
                if (amg8833_interrupt_test(addr, mode,
                                          (float)atof(argv[6]),
                                          (float)atof(argv[7]),
                                          (float)atof(argv[8]),
                                           atoi(argv[4])) != 0)
                {
                    g_gpio_irq = NULL;
                    (void)gpio_interrupt_deinit();
                    
                    return 1;
                }
                else
                {
                    g_gpio_irq = NULL;
                    (void)gpio_interrupt_deinit();
                    
                    return 0;
                }
            }
            /* param is invalid */
            else
            {
                return 5;
            }
        }
        /* run function */
        else if (strcmp("-c", argv[1]) == 0)
        {
             /* read function */
            if (strcmp("int", argv[2]) == 0)
            {
                uint32_t i, times;
                uint8_t res;
                amg8833_address_t addr;
                amg8833_interrupt_mode_t mode;
                
                if (strcmp("0", argv[3]) == 0)
                {
                    addr = AMG8833_ADDRESS_0;
                }
                else if (strcmp("1", argv[3]) == 0)
                {
                    addr = AMG8833_ADDRESS_1;
                }
                else
                {
                    return 5;
                }
                
                if (strcmp("abs", argv[5]) == 0)
                {
                    mode = AMG8833_INTERRUPT_MODE_ABSOLUTE;
                    amg8833_interface_debug_print("amg8833: absolute mode.\n");
                }
                else if (strcmp("diff", argv[5]) == 0)
                {
                    mode = AMG8833_INTERRUPT_MODE_DIFFERENCE;
                    amg8833_interface_debug_print("amg8833: difference mode.\n");
                }
                else
                {
                    return 5;
                }
                
                /* run interrupt test */
                g_gpio_irq = amg8833_interrupt_irq_handler;
                if (gpio_interrupt_init() != 0)
                {
                    g_gpio_irq = NULL;
                }
                times = atoi(argv[4]);
                if (interrupt_interrupt_init(addr, 
                                             mode,
                                            (float)atof(argv[6]),
                                            (float)atof(argv[7]),
                                            (float)atof(argv[8]),
                                             a_callback) != 0)
                {
                    g_gpio_irq = NULL;
                    (void)gpio_interrupt_deinit();
                    
                    return 1;
                }
                
                /* delay 1000 ms */
                amg8833_interface_delay_ms(1000);
                for (i = 0; i < times; i++)
                {
                    float temp;
                    
                    res = amg8833_interrupt_read_temperature((float *)&temp);
                    if (res != 0)
                    {
                        (void)amg8833_interrupt_deinit();
                        g_gpio_irq = NULL;
                        (void)gpio_interrupt_deinit();
                    }
                    else
                    {
                        amg8833_interface_debug_print("amg8833: temperature is %0.3fC.\n", temp);
                    }
                    
                    /* delay 1000 ms */
                    amg8833_interface_delay_ms(1000);
                }
                
                /* deinit */
                (void)amg8833_interrupt_deinit();
                g_gpio_irq = NULL;
                (void)gpio_interrupt_deinit();
                
                return 0;
            }
            /* param is invalid */
            else
            {
                return 5;
            }
        }
        /* param is invalid */
        else
        {
            return 5;
        }
    }
    /* param is invalid */
    else
    {
        return 5;
    }
}

/**
 * @brief main function
 * @note  none
 */
int main(void)
{
    uint8_t res;
    
    /* stm32f407 clock init and hal init */
    clock_init();
    
    /* delay init */
    delay_init();
    
    /* uart1 init */
    uart1_init(115200);
    
    /* shell init && register amg8833 fuction */
    shell_init();
    shell_register("amg8833", amg8833);
    uart1_print("amg8833: welcome to libdriver amg8833.\n");

    while (1)
    {
        /* read uart */
        g_len = uart1_read(g_buf, 256);
        if (g_len)
        {
            /* run shell */
            res = shell_parse((char *)g_buf, g_len);
            if (res == 0)
            {
                /* run success */
            }
            else if (res == 1)
            {
                uart1_print("amg8833: run failed.\n");
            }
            else if (res == 2)
            {
                uart1_print("amg8833: unknow command.\n");
            }
            else if (res == 3)
            {
                uart1_print("amg8833: length is too long.\n");
            }
            else if (res == 4)
            {
                uart1_print("amg8833: pretreat failed.\n");
            }
            else if (res == 5)
            {
                uart1_print("amg8833: param is invalid.\n");
            }
            else
            {
                uart1_print("amg8833: unknow status code.\n");
            }
            uart1_flush();
        }
        delay_ms(100);
    }
}
