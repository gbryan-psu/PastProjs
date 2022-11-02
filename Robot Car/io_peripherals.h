/*
 * io_peripherals.h
 *
 *  Created on: Feb 3, 2018
 *      Author: steveb
 */

#ifndef IO_PERIPHERALS_H_
#define IO_PERIPHERALS_H_

#if 0
#define PHYSICAL_ADDRESS  0x7E000000 /* base for BCM2835, PI1 */
#define PERIPHERAL_CLOCK  250000000
#elif 0
#define PHYSICAL_ADDRESS  0x20000000 /* base for BCM2708, PI2 */
#define PERIPHERAL_CLOCK  333333333
#elif 0
#define PHYSICAL_ADDRESS  0x3F000000 /* base for BCM2709, PI3 */
#define PERIPHERAL_CLOCK  50000000
#define FUART_CLOCK       48000000
#else
#define PHYSICAL_ADDRESS  0xFE000000 /* base for BCM2711, PI4B */
#define PERIPHERAL_CLOCK  50000000  /* peripheral clock frequency = AXI clock frequency */
#define FUART_CLOCK       48000000
#endif
#define IO_REGISTERS      ((volatile struct io_peripherals *)PHYSICAL_ADDRESS)

struct pcm_register
{
  uint8_t unused; /* empty structure */
};

struct io_peripherals
{
  uint8_t               unused0[0x101000-(0x000000/*+sizeof([])*/)];
  struct cm_register    cm;               /* offset = 0x101000, width = 0xA8 */
  uint8_t               unused1[0x200000-(0x101000+sizeof(struct cm_register))];
  struct gpio_register  gpio;             /* offset = 0x200000, width = 0x84 */
  uint8_t               unused2[0x201000-(0x200000+sizeof(struct gpio_register))];
  struct uart_register  uart;             /* offset - 0x201000, width = 0x90 */
  uint8_t               unused3[0x203000-(0x201000+sizeof(struct uart_register))];
  struct pcm_register   pcm;              /* offset = 0x203000, width = 0x24 */
  uint8_t               unused4[0x204000-(0x203000+sizeof(struct pcm_register))];
  struct spi_register   spi;              /* offset = 0x204000, width = 0x18 */
  uint8_t               unused5[0x20c000-(0x204000+sizeof(struct spi_register))];
  struct pwm_register   pwm;              /* offset = 0x20c000, width = 0x28 */
};

#endif /* IO_PERIPHERALS_H_ */
