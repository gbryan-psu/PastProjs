/**************************************************
* CMPEN 473, Spring 2021, Penn State University
* 
* Homework 4 Program 2
* On 2/19/2021
* By Gabien Bryan
* 
***************************************************/

/* Homework 4 Program 2
 * LED dimming program in C for 
 * Raspberry Pi 4 computer with 
 * red    LED on GPIO12 (with 330 ohm resistor in series)
 * green  LED on GPIO13 (with 330 ohm resistor in series)
 * blue   LED on GPIO22 (with 330 ohm resistor in series)
 * orange LED on GPIO23 (with 330 ohm resistor in series)
 * 
 * (1) Red    LED at GPIO12 goes from 0% light level to 100% light level in 2 seconds,
 * (2) Green  LED at GPIO13 goes from 0% light level to 75% light level in 3 seconds,
 * (3) Blue   LED at GPIO22 goes from 0% light level to 50% light level in 4 seconds,
 * (4) Orange LED at GPIO23 goes from 0% light level to 25% light level in 5 seconds,
 * (5) Red    LED at GPIO12 goes from 100% light level to 0% light level in 2 seconds,
 * (6) Green  LED at GPIO13 goes from 75% light level to 0% light level in 3 seconds,
 * (7) Blue   LED at GPIO22 goes from 50% light level to 0% light level in 4 seconds,
 * (8) Orange LED at GPIO23 goes from 25% light level to 0% light level in 5 seconds,
 * 
 * 'r' => red    LED on/off, hit 'r' key to toggle red    LED pause/unpause
 * 'g' => green  LED on/off  hit 'g' key to toggle green  LED pause/unpause
 * 'b' => blue   LED on/off  hit 'b' key to toggle blue   LED pause/unpause
 * 'o' => orange LED on/off  hit 'o' key to toggle orange LED pause/unpause
 * 
 * Comment: I tried to time the LEDs to as close to the right second as possible.
 * I attempted to calculate the exact values needed but all my calculations caused
 * the time to be drastically different than what was required.
 * 
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <termios.h>
#include <fcntl.h>
#include <stdlib.h>
#include <softPwm.h>
#include <pthread.h>
#include <signal.h>
#include "import_registers.h"
#include "gpio.h"
#include "cm.h"
#include "pwm.h"
#include "spi.h"
#include "io_peripherals.h"
#include "enable_pwm_clock.h"


struct thread_parameter
{
  volatile struct gpio_register * gpio;
  int                             pin;
  int                             lightLevel;
  int                             Tstep;  /* PWM time resolution, number used for usleep(Tstep) */ 
  int                             Tlevel;  /* repetition count of each light level, eg. repeat 12% light level for 2 times. */
  bool                            pause;
}typedef thread_parameter;

pthread_t                       thread12_handle;
pthread_t                       thread13_handle;
pthread_t                       thread22_handle;
pthread_t                       thread23_handle;
struct thread_parameter         *thread12_parameter;
struct thread_parameter         *thread13_parameter;
struct thread_parameter         *thread22_parameter;
struct thread_parameter         *thread23_parameter;


bool done = false;

int get_pressed_key(void) //get keyboard input
{
  struct termios  original_attributes;
  struct termios  modified_attributes;
  int             ch;

  tcgetattr( STDIN_FILENO, &original_attributes );
  modified_attributes = original_attributes;
  modified_attributes.c_lflag &= ~(ICANON | ECHO);
  modified_attributes.c_cc[VMIN] = 1;
  modified_attributes.c_cc[VTIME] = 0;
  tcsetattr( STDIN_FILENO, TCSANOW, &modified_attributes );

  ch = getchar();

  tcsetattr( STDIN_FILENO, TCSANOW, &original_attributes );

  return ch;
}

void *looopKeyboard( void * arg)
{
  while(!done)
  {
    char key = get_pressed_key();
    if (key == 'q')
    {
      done = true;
      volatile struct io_peripherals *io;
      io = import_registers();
    
      io->gpio.GPFSEL1.field.FSEL2 = GPFSEL_INPUT;
      io->gpio.GPFSEL1.field.FSEL3 = GPFSEL_INPUT;
      io->gpio.GPFSEL2.field.FSEL2 = GPFSEL_INPUT;
      io->gpio.GPFSEL2.field.FSEL3 = GPFSEL_INPUT;
    }
    else if (key == 'r')
    {
      if (!thread12_parameter->pause)
      {
        printf("pause red\n");
        thread12_parameter->pause = true;
      }
      else
      {
        printf("unpause red\n");
        thread12_parameter->pause = false;
      }
    }
    else if (key == 'g')
    {
      if (!thread13_parameter->pause)
      {
        printf("pause green\n");
        thread13_parameter->pause = true;
      }
      else
      {
        printf("unpause green\n");
        thread13_parameter->pause = false;
      }
    }
    else if (key == 'b')
    {
      if (!thread22_parameter->pause)
      {
        printf("pause blue\n");
        thread22_parameter->pause = true;
      }
      else
      {
        printf("unpause blue\n");
        thread22_parameter->pause = false;
      }
    }
    else if (key == 'o')
    {
      if (!thread23_parameter->pause)
      {
        printf("pause orange\n");
        thread23_parameter->pause = true;
      }
      else
      {
        printf("unpause orange\n");
        thread23_parameter->pause = false;
      }
    }
  }
  return 0;
}

void DimLevUnit(int Level, int pin, int Tstep, volatile struct gpio_register *gpio)
{
  int ONcount, OFFcount;

  ONcount = Level;
  OFFcount = 100 - Level;

  /* create the output pin signal duty cycle, same as Level */
  GPIO_SET( gpio, pin ); 
  while (ONcount > 0)
  {
    usleep( Tstep );
    ONcount = ONcount - 1;
  }
  GPIO_CLR( gpio, pin ); 
  while (OFFcount > 0)
  {
    usleep( Tstep );
    OFFcount = OFFcount - 1;
  }
}

void *looopLED( void * arg )
{
  int                       Timeu;      /* dimming repetition of each level */
  int                       DLevel;     /* dimming level as duty cycle, 0 to 100 percent */
  struct thread_parameter * parameter = (struct thread_parameter *)arg;

  while(!done)
  {
    DLevel = 0;  /* dim up, sweep the light level from 0 to 100 */
    while(DLevel < parameter->lightLevel)
    {
      Timeu = parameter->Tlevel;   /* repeat the dimming level for 5 times if Tlevel = 5 */
      while(Timeu>0)
      {
        DimLevUnit(DLevel, parameter->pin, parameter->Tstep, parameter->gpio);
        if (parameter->pause == true)
        {
          while(parameter->pause == true)
          {
            DimLevUnit(DLevel, parameter->pin, parameter->Tstep, parameter->gpio);
          }
        }
        Timeu = Timeu - 1;
      }
      DLevel = DLevel + 1;
    }

    DLevel = parameter->lightLevel;  /* dim down, sweep the light level from 100 to 0 */
    while(DLevel>0)
    {
      Timeu = parameter->Tlevel;   /* repeat the dimming level for 5 times if Tlevel = 5 */
      while(Timeu>0)
      {
        DimLevUnit(DLevel, parameter->pin, parameter->Tstep, parameter->gpio);
        if (parameter->pause == true)
        {
          while(parameter->pause == true)
          {
            DimLevUnit(DLevel, parameter->pin, parameter->Tstep, parameter->gpio);
          }
        }
        Timeu = Timeu - 1;
      }
      DLevel = DLevel - 1;
    }
  }

  return 0;
}

int main( void )
{
  pthread_t                       KEYthread;

  volatile struct io_peripherals *io;
  io = import_registers();
  
  thread12_parameter = (thread_parameter *)malloc(sizeof(thread_parameter));
  thread13_parameter = (thread_parameter *)malloc(sizeof(thread_parameter));
  thread22_parameter = (thread_parameter *)malloc(sizeof(thread_parameter));
  thread23_parameter = (thread_parameter *)malloc(sizeof(thread_parameter));
  
  if (io != NULL)
  {
    /* print where the I/O memory was actually mapped to */
    printf( "mem at 0x%8.8X\n", (unsigned long)io );

    /* set the pin function to GPIO for GPIO12, GPIO13, GPIO18, GPIO19 */
    io->gpio.GPFSEL1.field.FSEL2 = GPFSEL_OUTPUT;
    io->gpio.GPFSEL1.field.FSEL3 = GPFSEL_OUTPUT;
    io->gpio.GPFSEL2.field.FSEL2 = GPFSEL_OUTPUT;
    io->gpio.GPFSEL2.field.FSEL3 = GPFSEL_OUTPUT;
    
    printf("Press 'q' to quit program.\n");

#if 0
    Thread22( (void *)io );
#else
    thread12_parameter->pin = 12;
    thread12_parameter->gpio = &(io->gpio);
    thread12_parameter->lightLevel = 100;
    thread12_parameter->Tstep = 35;
    thread12_parameter->Tlevel = 2;
    thread12_parameter->pause = false;
    
    thread13_parameter->pin = 13;
    thread13_parameter->gpio = &(io->gpio);
    thread13_parameter->lightLevel = 75;
    thread13_parameter->Tstep = 39;
    thread13_parameter->Tlevel = 4;
    thread13_parameter->pause = false;
    
    thread22_parameter->pin = 22;
    thread22_parameter->gpio = &(io->gpio);
    thread22_parameter->lightLevel = 50;
    thread22_parameter->Tstep = 68;
    thread22_parameter->Tlevel = 6;
    thread22_parameter->pause = false;
    
    thread23_parameter->pin = 23;
    thread23_parameter->gpio = &(io->gpio);
    thread23_parameter->lightLevel = 25;
    thread23_parameter->Tstep = 98;
    thread23_parameter->Tlevel = 13;
    thread23_parameter->pause = false;
    
    pthread_create( &thread12_handle, 0, looopLED, (void *)thread12_parameter );
    pthread_create( &thread13_handle, 0, looopLED, (void *)thread13_parameter );
    pthread_create( &thread22_handle, 0, looopLED, (void *)thread22_parameter );
    pthread_create( &thread23_handle, 0, looopLED, (void *)thread23_parameter );
    pthread_create( &KEYthread, NULL, looopKeyboard, NULL);
    pthread_join( KEYthread, 0 );
#endif

    printf("'q' pressed... program end \n \n");

  }
  else
  {
    ; /* warning message already issued */
  }

  return 0;
}
