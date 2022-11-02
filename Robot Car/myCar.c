/**************************************************
* CMPEN 473, Spring 2021, Penn State University
* 
* Homework 7 Main Program
* On 3/29/2021
* By Gabien Bryan
* 
***************************************************/

/* Homework 7 Main Program
 * Controlling a small car in C for 
 * Raspberry Pi 4 computer
 * Key hit controls are as follows:
 * 
 * For recording IMU data (' m0 '):
 * 
 * For manual control mode (' m1 '):
 * (1) Stop:      ' s '
 * (2) Forward:   ' w '
 * (3) Backward:  ' x '
 * (4) Faster:    ' i '
 * (5) Slower:    ' j '
 * (6) Left:      ' a '
 * (7) Right:     ' d '
 * (8) Quit:      ' q ' to quit all program proper (without cnt’l c, and without an Enter key)
 * (9) Display IMU:    ' p '
 * (10) Display Calculated Data:     ' n '
 * 
 * For self driving mode (' m2 '):
 * Car will line trace utilizing two front IR sensors
 * (1)  Quit:      ' q ' to quit all program proper (without cnt’l c, and without an Enter key)
 * (2)  Start:     ' w '
 * (3)  Stop:      ' s '
 * (4) Display IMU:    ' p '
 * (5) Display Calculated Data:     ' n '
 * (6) Display Traveled Path:     ' s '
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
#include <pigpio.h>
#include <signal.h>
#include <math.h>
#include "import_registers.h"
#include "gpio.h"
#include "cm.h"
#include "uart.h"
#include "pwm.h"
#include "spi.h"
#include "io_peripherals.h"
#include "enable_pwm_clock.h"
#include "accelerometer_project.h"

#define APB_CLOCK 250000000
#define CS_PIN  8

#define ROUND_DIVISION(x,y) (((x) + (y)/2)/(y))

struct thread_parameter
{
  int                             rPin;
  int                             lPin;
  int                             rSpeed;
  int                             lSpeed;
  bool                            run;
}typedef thread_parameter;

struct imu_parameter
{
  struct calibration_data *calibration_accelerometer;
  struct calibration_data *calibration_gyroscope;
  struct calibration_data *calibration_magnetometer;
  FILE *saveFile;
  int sampleCount;
  bool run;
  int arraySize;
  volatile struct io_peripherals *io;
  
  double *accelX;
  double *accelY;
  double *accelZ;
  double *gyroX;
  double *gyroY;
  double *gyroZ;
  double *magX;
  double *magY;
  double *magZ;
  
}typedef imu_parameter;

//threads global to be accessed in multiple functions
pthread_t                   forward_handle;
pthread_t                   backward_handle;
pthread_t                   KEYthread;
pthread_t                   m0_handle;
pthread_t                   m1_handle;
pthread_t                   m2_handle;
struct thread_parameter     *forward_parameter;
struct thread_parameter     *backward_parameter;
struct imu_parameter        *m0_parameter;
struct imu_parameter        *m1_parameter;
struct imu_parameter        *m2_parameter;

volatile struct io_peripherals *io;
//global speed variables to keep track of car current overal speed
int glob_rSpeed;
int glob_lSpeed;

bool done;
bool manD = false;
bool selfD = false;
bool turnLeft = false;
bool turnRight = false;
bool self_drive = false;
bool m0_record = false;
bool m1_record = false;
bool m2_record = false;

/* For the steps, start speed is 100, top speed is 255
 * 32 steps: increase/decrease of 5 /this is the steps for increasing/decreasing speed/
 * 16 steps: increase/decrease of 10
*/

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

int get_printNum(double *modeArr, double modeVal)
{
  int ret;
  if(modeVal == modeArr[0] || (modeVal <= modeArr[1] && modeVal > modeArr[0]))
  {
    ret = 0;
  }
  else if(modeVal <= modeArr[2] && modeVal > modeArr[1])
  {
    ret = 1;
  }
  else if(modeVal <= modeArr[3] && modeVal > modeArr[2])
  {
    ret = 2;
  }
  else if(modeVal <= modeArr[4] && modeVal > modeArr[3])
  {
    ret = 3;
  }
  else if(modeVal <= modeArr[5] && modeVal > modeArr[4])
  {
    ret = 4;
  }
  else if(modeVal <= modeArr[6] && modeVal > modeArr[5])
  {
    ret = 5;
  }
  else if(modeVal <= modeArr[7] && modeVal > modeArr[6])
  {
    ret = 6;
  }
  else if(modeVal <= modeArr[8] && modeVal > modeArr[7])
  {
    ret = 7;
  }
  else if(modeVal <= modeArr[9] && modeVal > modeArr[8])
  {
    ret = 8;
  }
  else if(modeVal > modeArr[9])
  {
    ret = 9;
  }
  return ret;
}

void displayIMUdata() // ' p ' command
{
  imu_parameter *curr_mode = (imu_parameter *)malloc(sizeof(imu_parameter));
  if(m0_record == true)
  {
    curr_mode = m0_parameter;
  }
  else if(m1_record == true)
  {
    curr_mode = m1_parameter;
  }
  else if(m2_record == true)
  {
    curr_mode = m2_parameter;
  }
  else
  {
    printf("\nAttempted to Display IMU data with no current mode running!\n");
  }
  
  int minAX = curr_mode->accelX[0];
  int maxAX = curr_mode->accelX[0];
  int minAY = curr_mode->accelY[0];
  int maxAY = curr_mode->accelY[0];
  int minAZ = curr_mode->accelZ[0];
  int maxAZ = curr_mode->accelZ[0];
  int minGX = curr_mode->gyroX[0];
  int maxGX = curr_mode->gyroX[0];
  int minGY = curr_mode->gyroY[0];
  int maxGY = curr_mode->gyroY[0];
  int minGZ = curr_mode->gyroZ[0];
  int maxGZ = curr_mode->gyroZ[0];
  int minMX = curr_mode->magX[0];
  int maxMX = curr_mode->magX[0];
  int minMY = curr_mode->magY[0];
  int maxMY = curr_mode->magY[0];
  int minMZ = curr_mode->magZ[0];
  int maxMZ = curr_mode->magZ[0];
  
  //Find all min and max
  for(int i = 1; i < curr_mode->sampleCount; i++)
  {
    if(curr_mode->accelX[i] < minAX)
    {
      minAX = curr_mode->accelX[i];
    }
    if(curr_mode->accelX[i] > maxAX)
    {
      maxAX = curr_mode->accelX[i];
    }
    if(curr_mode->accelY[i] < minAY)
    {
      minAY = curr_mode->accelX[i];
    }
    if(curr_mode->accelY[i] > maxAY)
    {
      maxAY = curr_mode->accelY[i];
    }
    if(curr_mode->accelZ[i] < minAZ)
    {
      minAZ = curr_mode->accelZ[i];
    }
    if(curr_mode->accelZ[i] > maxAZ)
    {
      maxAZ = curr_mode->accelZ[i];
    }
    if(curr_mode->gyroX[i] < minGX)
    {
      minGX = curr_mode->gyroX[i];
    }
    if(curr_mode->gyroX[i] > maxGX)
    {
      maxGX = curr_mode->gyroX[i];
    }
    if(curr_mode->gyroY[i] < minGY)
    {
      minGY = curr_mode->gyroX[i];
    }
    if(curr_mode->gyroY[i] > maxGY)
    {
      maxGY = curr_mode->gyroY[i];
    }
    if(curr_mode->gyroZ[i] < minGZ)
    {
      minGZ = curr_mode->gyroZ[i];
    }
    if(curr_mode->gyroZ[i] > maxGZ)
    {
      maxGZ = curr_mode->gyroZ[i];
    }
    if(curr_mode->magX[i] < minMX)
    {
      minMX = curr_mode->magX[i];
    }
    if(curr_mode->magX[i] > maxMX)
    {
      maxMX = curr_mode->magX[i];
    }
    if(curr_mode->magY[i] < minMY)
    {
      minMY = curr_mode->magX[i];
    }
    if(curr_mode->magY[i] > maxMY)
    {
      maxMY = curr_mode->magY[i];
    }
    if(curr_mode->magZ[i] < minMZ)
    {
      minMZ = curr_mode->magZ[i];
    }
    if(curr_mode->accelZ[i] > maxAZ)
    {
      maxMZ = curr_mode->magZ[i];
    }
  }
  
  double AXarr[10];
  double AYarr[10];
  double AZarr[10];
  double GXarr[10];
  double GYarr[10];
  double GZarr[10];
  double MXarr[10];
  double MYarr[10];
  double MZarr[10];
  double AXval = (abs(maxAX) + abs(minAX))/10;
  double AYval = (abs(maxAY) + abs(minAY))/10;
  double AZval = (abs(maxAZ) + abs(minAZ))/10;
  double GXval = (abs(maxGX) + abs(minGX))/10;
  double GYval = (abs(maxGY) + abs(minGY))/10;
  double GZval = (abs(maxGZ) + abs(minGZ))/10;
  double MXval = (abs(maxMX) + abs(minMX))/10;
  double MYval = (abs(maxMY) + abs(minMY))/10;
  double MZval = (abs(maxMZ) + abs(minMZ))/10;
  
  for(int i = 0; i < 10; i++)
  {
    AXarr[i] += AXval * i;
    AYarr[i] += AYval * i;
    AZarr[i] += AZval * i;
    GXarr[i] += GXval * i;
    GYarr[i] += GYval * i;
    GZarr[i] += GZval * i;
    MXarr[i] += MXval * i;
    MYarr[i] += MYval * i;
    MZarr[i] += MZval * i;
  }
  printf("\nAC GY MG\n");
  for(int i = 0; i < curr_mode->sampleCount; i++)
  {
    printf("%d%d%d%d%d%d%d%d%d\n", get_printNum(AXarr, curr_mode->accelX[i]), get_printNum(AYarr, curr_mode->accelY[i]), get_printNum(AZarr, curr_mode->accelZ[i]),
      get_printNum(GXarr, curr_mode->gyroX[i]), get_printNum(GYarr, curr_mode->gyroY[i]), get_printNum(GZarr, curr_mode->gyroZ[i]),
      get_printNum(MXarr, curr_mode->magX[i]), get_printNum(MYarr, curr_mode->magY[i]), get_printNum(MYarr, curr_mode->magY[i]));
  }
}

void calc_dist_speed() // ' n ' command
{
  imu_parameter *curr_mode = (imu_parameter *)malloc(sizeof(imu_parameter));
  if(m0_record == true)
  {
    curr_mode = m0_parameter;
  }
  else if(m1_record == true)
  {
    curr_mode = m1_parameter;
  }
  else if(m2_record == true)
  {
    curr_mode = m2_parameter;
  }
  else
  {
    printf("\nAttempted to Display Distance and Speed with no current mode running!\n");
  }
  
  double distX = 0; //Distance traveled in x direction
  double distY = 0; //Distance traveled in y direction
  double distZ = 0; //Distance traveled in z direction
  double veloX = 0; //Velocity in x direction
  double veloY = 0; //Velocity in y direction
  double veloZ = 0; //Velocity in z direction
  double avgSpeed = 0; 
  double totDist = 0;
  double time = 0.05; //Time rate that samples are taken (every .05s)
  double count = 0; //Used to get avg speed after for loop end, counts 
  
  for(int i = 0; i < curr_mode->sampleCount; i++)
  {
    veloX = veloX + (curr_mode->accelX[i] * time);
    veloY = veloY + (curr_mode->accelY[i] * time);
    veloZ = veloZ + (curr_mode->accelZ[i] * time);
    distX = distX + (veloX * time);
    distY = distY + (veloY * time);
    distZ = distZ + (veloZ * time);
    
    if((i % 20) == 0 || (i + 1) == curr_mode->sampleCount)
    {
      count += 1;
      avgSpeed = avgSpeed + sqrt(pow(veloX, 2) + pow(veloY, 2));
      totDist = totDist + sqrt(pow(distX, 2) + pow(distX, 2));
      distX = 0;
      distY = 0;
      distZ = 0;
      veloX = 0;
      veloY = 0;
      veloZ = 0;
    }
  }
  avgSpeed = avgSpeed / count;
  printf("\nTotal Distance: %f meters\nAverage Speed: %f m/s\n", totDist, avgSpeed);
}

void *looopMotors( void * arg)
{
  struct thread_parameter * parameter = (struct thread_parameter *)arg;
  volatile struct io_peripherals *io;
  io = import_registers();
  
  io->gpio.GPFSEL2.field.FSEL4 = GPFSEL_INPUT;
  io->gpio.GPFSEL2.field.FSEL5 = GPFSEL_INPUT;
  
  while(!done)
  {
    while(parameter->run && manD) // checks if moving forward or backward 
    {
      gpioPWM(parameter->rPin, 255);
      gpioPWM(parameter->lPin, 0); // not sure why this is happening 
      gpioPWM(13, parameter->rSpeed);
      gpioPWM(12, parameter->lSpeed);
    }
    while(parameter->run && selfD)
    {
      gpioPWM(parameter->rPin, 255);
      gpioPWM(parameter->lPin, 0); // not sure why this is happening 
      gpioPWM(13, parameter->rSpeed);
      gpioPWM(12, parameter->lSpeed);
      
      if (gpioRead(24) != 0)// left turn
      {
        turnLeft = true;
        if (!turnRight)
        {
          forward_parameter->rSpeed = 255;
          forward_parameter->lSpeed = 0;
          gpioPWM(13, parameter->rSpeed);
          gpioPWM(12, parameter->lSpeed);
          usleep(220000);
          forward_parameter->rSpeed = glob_rSpeed;
          forward_parameter->lSpeed = glob_lSpeed;
        }
        turnLeft = false;
      }
      if (gpioRead(25) != 0)// right turn
      {
        turnRight = true;
        if (!turnLeft)
        {
          forward_parameter->rSpeed = 0;
          forward_parameter->lSpeed = 225;
          gpioPWM(13, parameter->rSpeed);
          gpioPWM(12, parameter->lSpeed);
          usleep(210000);
          forward_parameter->rSpeed = glob_rSpeed;
          forward_parameter->lSpeed = glob_lSpeed;
        }
        turnRight = false;
      }
      usleep(500000);
    }
  }
  return 0;
}

void *looopRecord( void * arg) // collect and store imu data
{
  struct imu_parameter *parameter = (imu_parameter *)arg;
  
  parameter->accelX = (double *)malloc(parameter->arraySize * sizeof(double));
  parameter->accelY = (double *)malloc(parameter->arraySize * sizeof(double));
  parameter->accelZ = (double *)malloc(parameter->arraySize * sizeof(double));
  parameter->gyroX = (double *)malloc(parameter->arraySize * sizeof(double));
  parameter->gyroY = (double *)malloc(parameter->arraySize * sizeof(double));
  parameter->gyroZ = (double *)malloc(parameter->arraySize * sizeof(double));
  parameter->magX = (double *)malloc(parameter->arraySize * sizeof(double));
  parameter->magY = (double *)malloc(parameter->arraySize * sizeof(double));
  parameter->magZ = (double *)malloc(parameter->arraySize * sizeof(double));
  int count = 0;
  double tempA[3];
  double tempG[3];
  double tempM[3];
  parameter->sampleCount = 0;
  
  while(!done)
  {
    while(parameter->run)
    {
      read_accelerometer_gyroscope(parameter->calibration_accelerometer, parameter->calibration_gyroscope, &(parameter->io->spi), &(parameter->io->gpio), tempA, tempG);
      read_magnetometer(parameter->calibration_magnetometer, &(parameter->io->spi), &(parameter->io->gpio), tempM);
      
      parameter->accelX[count] = tempA[0];
      parameter->accelY[count] = tempA[1];
      parameter->accelZ[count] = tempA[2];
      parameter->gyroX[count] = tempG[0];
      parameter->gyroY[count] = tempG[1];
      parameter->gyroZ[count] = tempG[2];
      parameter->magX[count] = tempM[0];
      parameter->magY[count] = tempM[1];
      parameter->magZ[count] = tempM[2];
      
      fprintf(parameter->saveFile, "%f %f %f %f %f %f %f %f %f\n", parameter->accelX[count], parameter->accelZ[count], parameter->accelZ[count], 
        parameter->gyroX[count], parameter->gyroY[count], parameter->gyroZ[count], 
        parameter->magX[count], parameter->magY[count], parameter->magZ[count]);
      
      parameter->sampleCount += 1;  
      count += 1;
      usleep(50000);
      if(!parameter->run || count >= parameter->arraySize)
      {
        fclose(parameter->saveFile);
        parameter->run = false;
        count = 0;
      }
    }
    usleep(500000);
  }
  return 0;
}

void *looopKeyboard( void * arg) // the control for car
{
  
  printf("\nHW7> ");
  while (!done)
  {
    char key = get_pressed_key();
    if(key == 'm')
    {
      char key2 = get_pressed_key();
      if (key2 == '0')
      {
        printf("\nHW7m0> Record Data\n");
        forward_parameter->run = false;
        backward_parameter->run = false;
        m1_parameter->run = false;
        m2_parameter->run = false;
        usleep(500000);
        m0_parameter->sampleCount = 0;
        m0_parameter->saveFile = fopen("hw7m0data.txt", "w");
        m0_parameter->run = true;
        sleep(5);
        m0_parameter->run = false;
        printf("\nm0 recording done\n");
      }
      if (key2 == '1')
      {
        printf("\nManual Driving Mode \nPress 'w' or 'x' to start motion\n");
        printf("\nHW7m1>");
        manD = true;
        selfD = false;
        forward_parameter->run = false;
        backward_parameter->run = false;
        usleep(500000);
        m0_record = false;
        m1_record = true;
        m2_record = false;
      }
      if (key2 == '2')
      {
        printf("\nSelf Driving Mode\nPress 'w' to start and 's' to stop");
        printf("\nHW7m2>");
        manD = false;
        selfD = true;
        forward_parameter->run = false;
        backward_parameter->run = false;
        usleep(500000);
        m0_record = false;
        m1_record = false;
        m2_record = true;
        glob_rSpeed = 180;
        glob_lSpeed = 192;
        forward_parameter->rSpeed = glob_rSpeed;
        forward_parameter->lSpeed = glob_lSpeed;
      }
    }
    if(key == 's') //stop
    {
      printf("%c\n", key);
      forward_parameter->run = false;
      backward_parameter->run = false;
      m0_parameter->run = false;
      m1_parameter->run = false;
      m2_parameter->run = false;
      gpioPWM(12, 0);
      gpioPWM(13, 0);
      usleep(500000);
    }
    if(key == 'w') //forward
    {
      printf("%c\n", key);
      if (!backward_parameter->run)
      {
        forward_parameter->run = true;
      }
      else
      {
        backward_parameter->run = false;
        usleep(1000000);
        forward_parameter->run = true;
      }
      if (m1_record == true)
      {
        m1_parameter->sampleCount = 0;
        m1_parameter->saveFile = fopen("hw7m1data.txt", "w");
        m1_parameter->run = true;
      }
      if (m2_record == true)
      {
        m2_parameter->sampleCount = 0;
        m2_parameter->saveFile = fopen("hw7m2data.txt", "w");
        m2_parameter->run = true;
      }
    }
    if (key == 'x' && manD) //backward
    {
      printf("%c\n", key);
      if (!forward_parameter->run)
      {
        backward_parameter->run = true;
      }
      else
      {
        forward_parameter->run = false;
        usleep(1000000);
        backward_parameter->run = true;
      }
    } 
    if (key == 'i' && manD) //faster
    {
      printf("%c\n", key);
      glob_rSpeed += 5;
      glob_lSpeed += 5;
      if(glob_rSpeed > 240 && glob_lSpeed > 255)
      {
        printf("Max Speed");
        glob_rSpeed = 240;
        glob_lSpeed = 255;
      }
        
      if (forward_parameter->run)
      {
        forward_parameter->rSpeed = glob_rSpeed;
        forward_parameter->lSpeed = glob_lSpeed;
      }
      else
      {
        backward_parameter->rSpeed = glob_rSpeed;
        backward_parameter->lSpeed = glob_lSpeed;
      }
    }
    if (key == 'j' && manD) //slower
    {
      printf("%c\n", key);
      glob_rSpeed -= 5;
      glob_lSpeed -= 5;
      if(glob_rSpeed < 100 && glob_lSpeed < 100)
      {
        printf("MIN Speed");
        glob_rSpeed = 100;
        glob_lSpeed = 95;
      }
      
      if (forward_parameter->run)
      {
        forward_parameter->rSpeed = glob_rSpeed;
        forward_parameter->lSpeed = glob_lSpeed;
      }
      else
      {
        backward_parameter->rSpeed = glob_rSpeed;
        backward_parameter->lSpeed = glob_lSpeed;
      }
    }
    if (key == 'a' && manD) //left
    {
      printf("%c\n", key);
      turnLeft = true;
      if (!turnRight)
      {
        if(forward_parameter->run)
        {
          forward_parameter->rSpeed = 255;
          forward_parameter->lSpeed = 0;
          usleep(190000);
          forward_parameter->rSpeed = glob_rSpeed;
          forward_parameter->lSpeed = glob_lSpeed;
        }
        else
        {
          backward_parameter->rSpeed = 0;
          backward_parameter->lSpeed = 225;
          usleep(200000);
          backward_parameter->rSpeed = glob_rSpeed;
          backward_parameter->lSpeed = glob_lSpeed;
        }
      }
      turnLeft = false;
    }
    if (key == 'd' && manD) //right
    {
      printf("%c\n", key);
      turnRight = true;
      if (!turnLeft)
      {
        if(forward_parameter->run)
        {
          forward_parameter->rSpeed = 0;
          forward_parameter->lSpeed = 225;
          usleep(200000);
          forward_parameter->rSpeed = glob_rSpeed;
          forward_parameter->lSpeed = glob_lSpeed;
        }
        else
        {
          backward_parameter->rSpeed = 255;
          backward_parameter->lSpeed = 0;
          usleep(190000);
          backward_parameter->rSpeed = glob_rSpeed;
          backward_parameter->lSpeed = glob_lSpeed;
        }
      }     
      turnRight = false;
    }
    if (key == 'p')
    {
      displayIMUdata();
    }
    if (key == 'n')
    {
      calc_dist_speed();
    }
    if (key == 't')
    {
      continue;
    }
    if (key == 'q') //quit
    {
      forward_parameter->run = false;
      backward_parameter->run = false;
      gpioPWM(12, 0);
      gpioPWM(13, 0);
      usleep(500000);
      done = true;
    }
    usleep(500000); 
  } 
  return 0;
}



int main( void )
{
  volatile struct io_peripherals *io;
  io = import_registers();
  
  forward_parameter = (thread_parameter *)malloc(sizeof(thread_parameter));
  backward_parameter = (thread_parameter *)malloc(sizeof(thread_parameter));
  m0_parameter = (imu_parameter *)malloc(sizeof(imu_parameter));
  m1_parameter = (imu_parameter *)malloc(sizeof(imu_parameter));
  m2_parameter = (imu_parameter *)malloc(sizeof(imu_parameter));
  struct calibration_data calibration_accelerometer;
  struct calibration_data calibration_gyroscope;
  struct calibration_data calibration_magnetometer;
  glob_rSpeed = 100;
  glob_lSpeed = 95;
  done = false;
  
  
  if (io != NULL)
  {
    /* print where the I/O memory was actually mapped to */
    printf( "mem at 0x%8.8X\n", (unsigned long)io );
    
    /* pigpio allows for use of 'gpioInitialise' and 'gpioPWM' */
    gpioCfgSetInternals(1<<10); //Kept getting a segfault and this prevents the flag
    gpioInitialise();
    
    io->gpio.GPFSEL2.field.FSEL4 = GPFSEL_INPUT;
    io->gpio.GPFSEL2.field.FSEL5 = GPFSEL_INPUT;
    
    //pulled from accelerometer_project.c
    /* set the pin function to alternate function 0 for GPIO09 (SPI, MISO) */
    /* set the pin function to alternate function 0 for GPIO10 (SPI, MOSI) */
    /* set the pin function to alternate function 0 for GPIO11 (SPI, SCK) */
    /* set the pin function to output for GPIO08 (SPI, CS) */
    io->gpio.GPFSEL0.field.FSEL9 = GPFSEL_ALTERNATE_FUNCTION0;
    io->gpio.GPFSEL1.field.FSEL0 = GPFSEL_ALTERNATE_FUNCTION0;
    io->gpio.GPFSEL1.field.FSEL1 = GPFSEL_ALTERNATE_FUNCTION0;
    io->gpio.GPFSEL0.field.FSEL8 = GPFSEL_OUTPUT;

    /* set initial output state */
    GPIO_SET(&(io->gpio), CS_PIN);
    usleep( 100000 );

    /* set up the SPI parameters */
    io->spi.CLK.field.CDIV = ((ROUND_DIVISION(APB_CLOCK,800000))>>1)<<1; /* this number must be even, so shift the LSb into oblivion */
    io->spi.CS.field.CS       = 0;
    io->spi.CS.field.CPHA     = 1;  /* clock needs to idle high and clock in data on the rising edge */
    io->spi.CS.field.CPOL     = 1;
    io->spi.CS.field.CLEAR    = 0;
    io->spi.CS.field.CSPOL    = 0;
    io->spi.CS.field.TA       = 0;
    io->spi.CS.field.DMAEN    = 0;
    io->spi.CS.field.INTD     = 0;
    io->spi.CS.field.INTR     = 0;
    io->spi.CS.field.ADCS     = 0;
    io->spi.CS.field.REN      = 0;
    io->spi.CS.field.LEN      = 0;
    /* io->spi.CS.field.LMONO */
    /* io->spi.CS.field.TE_EN */
    /* io->spi.CS.field.DONE */
    /* io->spi.CS.field.RXD */
    /* io->spi.CS.field.TXD */
    /* io->spi.CS.field.RXR */
    /* io->spi.CS.field.RXF */
    io->spi.CS.field.CSPOL0   = 0;
    io->spi.CS.field.CSPOL1   = 0;
    io->spi.CS.field.CSPOL2   = 0;
    io->spi.CS.field.DMA_LEN  = 0;
    io->spi.CS.field.LEN_LONG = 0;

    initialize_accelerometer_and_gyroscope( &calibration_accelerometer, &calibration_gyroscope, &(io->spi), &(io->gpio) );
    initialize_magnetometer( &calibration_magnetometer, &(io->spi), &(io->gpio) );
    
    printf("Press 'q' to quit program.\n");

#if 0
    //Thread13( (void *)io );
#else
    forward_parameter->rPin = 5;
    forward_parameter->lPin = 22;
    forward_parameter->rSpeed = glob_rSpeed;
    forward_parameter->lSpeed = glob_lSpeed;
    forward_parameter->run = false;
    
    backward_parameter->rPin = 6;
    backward_parameter->lPin = 23;
    backward_parameter->rSpeed = glob_rSpeed;
    backward_parameter->lSpeed = glob_lSpeed;
    backward_parameter->run = false;
    
    m0_parameter->calibration_accelerometer = &(calibration_accelerometer);
    m0_parameter->calibration_gyroscope = &(calibration_gyroscope);
    m0_parameter->calibration_magnetometer = &(calibration_magnetometer);
    m0_parameter->arraySize = 100;
    m0_parameter->io = io;
    m0_parameter->run = false;
    
    m1_parameter->calibration_accelerometer = &(calibration_accelerometer);
    m1_parameter->calibration_gyroscope = &(calibration_gyroscope);
    m1_parameter->calibration_magnetometer = &(calibration_magnetometer);
    m1_parameter->arraySize = 24000;
    m1_parameter->io = io;
    m1_parameter->run = false;
    
    m2_parameter->calibration_accelerometer = &(calibration_accelerometer);
    m2_parameter->calibration_gyroscope = &(calibration_gyroscope);
    m2_parameter->calibration_magnetometer = &(calibration_magnetometer);
    m2_parameter->arraySize = 24000;
    m2_parameter->io = io;
    m2_parameter->run = false;
    
    pthread_create( &forward_handle, 0, (void *)looopMotors, (void *)forward_parameter);
    pthread_create( &backward_handle, 0, (void *)looopMotors, (void *)backward_parameter);
    pthread_create( &KEYthread, NULL, looopKeyboard, NULL);
    pthread_create( &m0_handle, 0, (void *)looopRecord, (void *)m0_parameter);
    pthread_create( &m1_handle, 0, (void *)looopRecord, (void *)m1_parameter);
    pthread_create( &m2_handle, 0, (void *)looopRecord, (void *)m2_parameter); 
    pthread_join( KEYthread, 0 );
#endif
    printf("'q' pressed... program end \n \n");
    free(forward_parameter);
    free(backward_parameter);
    free(m0_parameter);
    free(m1_parameter);
    free(m2_parameter);

  }
  else
  {
    ; /* warning message already issued */
  }

  return 0;
}
