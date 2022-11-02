#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "cm.h"
#include "gpio.h"
#include "uart.h"
#include "spi.h"
#include "pwm.h"
#include "io_peripherals.h"
#include "import_registers.h"


volatile void * import_registers( void )
{
  volatile void * return_value; /* the return value of this function */
  int             mmap_file;    /* the file descriptor used to map the memory */

  mmap_file = open( "/dev/mem", O_RDWR|O_SYNC );
  if (mmap_file != -1)
  {
    /* try to put the physical I/O space at the same address range in the virtual address space */
    return_value = mmap(
      (void *)PHYSICAL_ADDRESS,
      sizeof(struct io_peripherals),
      PROT_READ|PROT_WRITE|PROT_EXEC,
      MAP_SHARED,
      mmap_file,
      PHYSICAL_ADDRESS );
    if (return_value != MAP_FAILED)
    {
      ; /* mapped memory */
    }
    else
    {
      printf( "unable to map register space\n" );

      close( mmap_file );

      return_value = NULL;
    }
  }
  else
  {
    printf( "unable to open /dev/mem\n" );

    return_value = NULL;
  }

  return return_value;
}

