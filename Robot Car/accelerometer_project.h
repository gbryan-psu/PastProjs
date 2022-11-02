/* Had a lot of troubles trying to 
 * include accelerometer_project.c in my 
 * hw7bryan.c file. I read that creating a header file 
 * for the structs would most likely eliviate any 
 * errors I was running into. 
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "import_registers.h"
#include "cm.h"
#include "uart.h"
#include "spi.h"
#include "pwm.h"
#include "io_peripherals.h"
#include "MPU9250.h"
#include "wait_key.h"
#include  "accelerometer_project.c"

void transact_SPI_no_CS(                          /* send/receive SPI data without toggling the chip select */
    uint8_t const *                 write_data,   /* the data to send to a SPI device */
    uint8_t *                       read_data,    /* the data read from the SPI device */
    size_t                          data_length,  /* the length of data to send/receive */
    volatile struct gpio_register * gpio,         /* the GPIO address */
    volatile struct spi_register *  spi );         /* the SPI address */
    
void select_CS_MPU9250(
    int                             CS_pin,       /* the pin to toggle when communicating */
    volatile struct gpio_register * gpio );        /* the GPIO address */
    
void deselect_CS_MPU9250(
    int                             CS_pin,       /* the pin to toggle when communicating */
    volatile struct gpio_register * gpio );        /* the GPIO address */
    
void read_MPU9250_registers(                      /* read a register */
    MPU9250_REGISTER                address,      /* the address to read from */
    uint8_t *                       read_data,    /* the data read from the SPI device */
    size_t                          data_length,  /* the length of data to send/receive */
    int                             CS_pin,       /* the pin to toggle when communicating */
    volatile struct gpio_register * gpio,         /* the GPIO address */
    volatile struct spi_register *  spi );         /* the SPI address */

union MPU9250_transaction_field_data read_MPU9250_register( /* read a register, returning the read value */
    MPU9250_REGISTER                address,                /* the address to read from */
    int                             CS_pin,                 /* the pin to toggle when communicating */
    volatile struct gpio_register * gpio,                   /* the GPIO address */
    volatile struct spi_register *  spi );                   /* the SPI address */
    
void write_MPU9250_register(                        /* write a register */
    MPU9250_REGISTER                      address,  /* the address to read from */
    union MPU9250_transaction_field_data  value,    /* the value to write */
    int                                   CS_pin,   /* the pin to toggle when communicating */
    volatile struct gpio_register *       gpio,     /* the GPIO address */
    volatile struct spi_register *        spi );     /* the SPI address */
    
void calibrate_accelerometer_and_gyroscope(
    struct calibration_data *     calibration_accelerometer,
    struct calibration_data *     calibration_gyroscope,
    volatile struct spi_register *spi,
    volatile struct gpio_register*gpio );
    
void initialize_accelerometer_and_gyroscope(
    struct calibration_data *     calibration_accelerometer,
    struct calibration_data *     calibration_gyroscope,
    volatile struct spi_register *spi,
    volatile struct gpio_register*gpio );
    
void initialize_magnetometer(
    struct calibration_data *     calibration_magnetometer,
    volatile struct spi_register *spi,
    volatile struct gpio_register*gpio );
    
void read_accelerometer_gyroscope(
    struct calibration_data *     calibration_accelerometer,
    struct calibration_data *     calibration_gyroscope,
    volatile struct spi_register *spi,
    volatile struct gpio_register*gpio,
    double gyroOut[],
    double accelOut[] );
    
void read_magnetometer(
    struct calibration_data *     calibration_magnetometer,
    volatile struct spi_register *spi,
    volatile struct gpio_register*gpio,
    double magOut[] );
