
#define APB_CLOCK 250000000
#define CS_PIN  8

#define ROUND_DIVISION(x,y) (((x) + (y)/2)/(y))

void transact_SPI_no_CS(                          /* send/receive SPI data without toggling the chip select */
    uint8_t const *                 write_data,   /* the data to send to a SPI device */
    uint8_t *                       read_data,    /* the data read from the SPI device */
    size_t                          data_length,  /* the length of data to send/receive */
    volatile struct gpio_register * gpio,         /* the GPIO address */
    volatile struct spi_register *  spi )         /* the SPI address */
{
  size_t  write_count;  /* used to index through the bytes to be sent/received */
  size_t  read_count;   /* used to index through the bytes to be sent/received */

  /* clear out anything in the RX FIFO */
  while (spi->CS.field.RXD != 0)
  {
    (void)spi->FIFO;
  }

  /* see section 10.6.1 of the BCM2835 datasheet
   * Note that the loop below is a busy-wait loop and may burn a lot of clock cycles, depending on the amount of data to be transferred
   */
  spi->CS.field.TA = 1;
  write_count = 0;
  read_count  = 0;
  do
  {
    /* transfer bytes to the device */
    while ((write_count != data_length) &&
           (spi->CS.field.TXD != 0))
    {
      spi->FIFO = (uint32_t)*write_data;

      write_data++;
      write_count++;
    }

    /* drain the RX FIFO */
    while ((read_count != data_length) &&
           (spi->CS.field.RXD != 0))
    {
      if (read_data != NULL)
      {
        *read_data = spi->FIFO;

        read_data++;
        read_count++;
      }
      else
      {
        (void)spi->FIFO;

        read_count++;
      }
    }
  } while (spi->CS.field.DONE == 0);
  spi->CS.field.TA = 0;

  return;
}

void select_CS_MPU9250(
    int                             CS_pin,       /* the pin to toggle when communicating */
    volatile struct gpio_register * gpio )        /* the GPIO address */
{
  // bus access timing for the Raspberry Pi (at least 3 and 4) is 50MHz, a period of 20ns
  // tSU, CS = 8ns
  // two bus accesses prior to data transmission will do the trick (40ns)
  GPIO_CLR( gpio, CS_pin );
  GPIO_CLR( gpio, CS_pin );

  return;
}

void deselect_CS_MPU9250(
    int                             CS_pin,       /* the pin to toggle when communicating */
    volatile struct gpio_register * gpio )        /* the GPIO address */
{
  // tHD, CS = 500ns
  // thirty bus accesses prior to deselect should do (600ns)
  unsigned int i;
  for (i = 30; i > 0; i--)
  {
    GPIO_CLR( gpio, CS_pin );
  }
  GPIO_SET( gpio, CS_pin );

  // there appears to be a maximum transaction rate that is not documented (a required idle period between transfers)
  // it appears to be suspiciously close to 400KHz (the I2C bit rate that this device can run at)
  usleep( 50 );

  return;
}

void read_MPU9250_registers(                      /* read a register */
    MPU9250_REGISTER                address,      /* the address to read from */
    uint8_t *                       read_data,    /* the data read from the SPI device */
    size_t                          data_length,  /* the length of data to send/receive */
    int                             CS_pin,       /* the pin to toggle when communicating */
    volatile struct gpio_register * gpio,         /* the GPIO address */
    volatile struct spi_register *  spi )         /* the SPI address */
{
  union MPU9250_transaction transaction;

  transaction.field.address.AD  = address;
  transaction.field.address.R_W = 1;
  transaction.value[1]          = 0;

  select_CS_MPU9250( CS_pin, gpio );
  transact_SPI_no_CS( transaction.value, transaction.value, sizeof(transaction.field.address), gpio, spi );
  transact_SPI_no_CS( read_data, read_data, data_length, gpio, spi );
  deselect_CS_MPU9250( CS_pin, gpio );

  return;
}

union MPU9250_transaction_field_data read_MPU9250_register( /* read a register, returning the read value */
    MPU9250_REGISTER                address,                /* the address to read from */
    int                             CS_pin,                 /* the pin to toggle when communicating */
    volatile struct gpio_register * gpio,                   /* the GPIO address */
    volatile struct spi_register *  spi )                   /* the SPI address */
{
  union MPU9250_transaction transaction;

  transaction.field.address.AD  = address;
  transaction.field.address.R_W = 1;
  transaction.value[1]          = 0;

  select_CS_MPU9250( CS_pin, gpio );
  transact_SPI_no_CS( transaction.value, transaction.value, sizeof(transaction.value), gpio, spi );
  deselect_CS_MPU9250( CS_pin, gpio );

  return transaction.field.data;
}

void write_MPU9250_register(                        /* write a register */
    MPU9250_REGISTER                      address,  /* the address to read from */
    union MPU9250_transaction_field_data  value,    /* the value to write */
    int                                   CS_pin,   /* the pin to toggle when communicating */
    volatile struct gpio_register *       gpio,     /* the GPIO address */
    volatile struct spi_register *        spi )     /* the SPI address */
{
  union MPU9250_transaction transaction;

  transaction.field.address.AD  = address;
  transaction.field.address.R_W = 0;
  transaction.field.data        = value;

  select_CS_MPU9250( CS_pin, gpio );
  transact_SPI_no_CS( transaction.value, transaction.value, sizeof(transaction.value), gpio, spi );
  deselect_CS_MPU9250( CS_pin, gpio );

  return;
}

void calibrate_accelerometer_and_gyroscope(
    struct calibration_data *     calibration_accelerometer,
    struct calibration_data *     calibration_gyroscope,
    volatile struct spi_register *spi,
    volatile struct gpio_register*gpio )
{
  union MPU9250_transaction_field_data  transaction;
  uint8_t                               data_block_fifo_count[2];
  union uint16_to_2uint8                reconstructor;
  uint16_t                              ii;
  uint16_t                              packet_count;
  int32_t                               gyro_bias_x;
  int32_t                               gyro_bias_y;
  int32_t                               gyro_bias_z;
  int32_t                               accel_bias_x;
  int32_t                               accel_bias_y;
  int32_t                               accel_bias_z;
  uint8_t                               data_block_fifo_packet[12];
  union uint16_to_2uint8                reconstructor_accel_x;
  union uint16_to_2uint8                reconstructor_accel_y;
  union uint16_to_2uint8                reconstructor_accel_z;
  union uint16_to_2uint8                reconstructor_gyro_x;
  union uint16_to_2uint8                reconstructor_gyro_y;
  union uint16_to_2uint8                reconstructor_gyro_z;

  // reset device
  transaction.PWR_MGMT_1.CLKSEL       = 0;
  transaction.PWR_MGMT_1.PD_PTAT      = 0;
  transaction.PWR_MGMT_1.GYRO_STANDBY = 0;
  transaction.PWR_MGMT_1.CYCLE        = 0;
  transaction.PWR_MGMT_1.SLEEP        = 0;
  transaction.PWR_MGMT_1.H_RESET      = 1;
  write_MPU9250_register( MPU9250_REGISTER_PWR_MGMT_1, transaction, CS_PIN, gpio, spi );
  usleep( 100000 );

  // get stable time source; auto select clock source to be PLL gyroscope reference if ready
  // else use the internal oscillator
  transaction.PWR_MGMT_1.CLKSEL       = 1;
  transaction.PWR_MGMT_1.PD_PTAT      = 0;
  transaction.PWR_MGMT_1.GYRO_STANDBY = 0;
  transaction.PWR_MGMT_1.CYCLE        = 0;
  transaction.PWR_MGMT_1.SLEEP        = 0;
  transaction.PWR_MGMT_1.H_RESET      = 0;
  write_MPU9250_register( MPU9250_REGISTER_PWR_MGMT_1, transaction, CS_PIN, gpio, spi );
  transaction.PWR_MGMT_2.DIS_ZG   = 0;
  transaction.PWR_MGMT_2.DIS_YG   = 0;
  transaction.PWR_MGMT_2.DIS_XG   = 0;
  transaction.PWR_MGMT_2.DIS_ZA   = 0;
  transaction.PWR_MGMT_2.DIS_YA   = 0;
  transaction.PWR_MGMT_2.DIS_XA   = 0;
  transaction.PWR_MGMT_2.reserved = 0;
  write_MPU9250_register( MPU9250_REGISTER_PWR_MGMT_2, transaction, CS_PIN, gpio, spi );
  usleep( 200000 );

  // configure device for bias calculation
  transaction.INT_ENABLE.RAW_RDY_EN    = 0; // disable all interrupts
  transaction.INT_ENABLE.reserved0     = 0;
  transaction.INT_ENABLE.FSYNC_INT_EN  = 0;
  transaction.INT_ENABLE.FIFO_OFLOW_EN = 0;
  transaction.INT_ENABLE.reserved1     = 0;
  transaction.INT_ENABLE.WOM_EN        = 0;
  transaction.INT_ENABLE.reserved2     = 0;
  write_MPU9250_register( MPU9250_REGISTER_INT_ENABLE, transaction, CS_PIN, gpio, spi );
  transaction.FIFO_EN.SLV0         = 0; // disable FIFO
  transaction.FIFO_EN.SLV1         = 0;
  transaction.FIFO_EN.SLV2         = 0;
  transaction.FIFO_EN.ACCEL        = 0;
  transaction.FIFO_EN.GYRO_ZO_UT   = 0;
  transaction.FIFO_EN.GYRO_YO_UT   = 0;
  transaction.FIFO_EN.GYRO_XO_UT   = 0;
  transaction.FIFO_EN.TEMP_FIFO_EN = 0;
  write_MPU9250_register( MPU9250_REGISTER_FIFO_EN, transaction, CS_PIN, gpio, spi );
  transaction.PWR_MGMT_1.CLKSEL       = 0;  // turn on internal clock source
  transaction.PWR_MGMT_1.PD_PTAT      = 0;
  transaction.PWR_MGMT_1.GYRO_STANDBY = 0;
  transaction.PWR_MGMT_1.CYCLE        = 0;
  transaction.PWR_MGMT_1.SLEEP        = 0;
  transaction.PWR_MGMT_1.H_RESET      = 0;
  write_MPU9250_register( MPU9250_REGISTER_PWR_MGMT_1, transaction, CS_PIN, gpio, spi );
  transaction.I2C_MST_CTRL.I2C_MST_CLK   = 0; // disable I2C master
  transaction.I2C_MST_CTRL.I2C_MST_P_NSR = 0;
  transaction.I2C_MST_CTRL.SLV_3_FIFO_EN = 0;
  transaction.I2C_MST_CTRL.WAIT_FOR_ES   = 0;
  transaction.I2C_MST_CTRL.MULT_MST_EN   = 0;
  write_MPU9250_register( MPU9250_REGISTER_I2C_MST_CTRL, transaction, CS_PIN, gpio, spi );
  transaction.USER_CTRL.SIG_COND_RST = 0; // disable FIFO and I2C master modes
  transaction.USER_CTRL.I2C_MST_RST  = 0;
  transaction.USER_CTRL.FIFO_RST     = 0;
  transaction.USER_CTRL.reserved0    = 0;
  transaction.USER_CTRL.I2C_IF_DIS   = 0;
  transaction.USER_CTRL.I2C_MST_EN   = 0;
  transaction.USER_CTRL.FIFO_EN      = 0;
  transaction.USER_CTRL.reserved1    = 0;
  write_MPU9250_register( MPU9250_REGISTER_USER_CTRL, transaction, CS_PIN, gpio, spi );
  transaction.USER_CTRL.SIG_COND_RST = 0; // reset FIFO and DMP
  transaction.USER_CTRL.I2C_MST_RST  = 0;
  transaction.USER_CTRL.FIFO_RST     = 1;
  transaction.USER_CTRL.reserved0    = 0;
  transaction.USER_CTRL.I2C_IF_DIS   = 0;
  transaction.USER_CTRL.I2C_MST_EN   = 0;
  transaction.USER_CTRL.FIFO_EN      = 0;
  transaction.USER_CTRL.reserved1    = 0;
  write_MPU9250_register( MPU9250_REGISTER_USER_CTRL, transaction, CS_PIN, gpio, spi );
  usleep( 15000 );

  // configure MPU9250 gyro and accelerometer for bias calculation
  transaction.CONFIG.DLPF_CFG     = 1;  // set low-pass filter to 188Hz
  transaction.CONFIG.EXT_SYNC_SET = 0;
  transaction.CONFIG.FIFO_MODE    = 0;
  transaction.CONFIG.reserved     = 0;
  write_MPU9250_register( MPU9250_REGISTER_CONFIG, transaction, CS_PIN, gpio, spi );
  transaction.SMPLRT_DIV.SMPLRT_DIV = 0;  // set sample rate to 1kHz
  write_MPU9250_register( MPU9250_REGISTER_SMPLRT_DIV, transaction, CS_PIN, gpio, spi );
  transaction.GYRO_CONFIG.FCHOICE_B   = 0; // set gyro full-scale to 250dps, maximum sensitivity
  transaction.GYRO_CONFIG.reserved    = 0;
  transaction.GYRO_CONFIG.GYRO_FS_SEL = 0;
  transaction.GYRO_CONFIG.ZGYRO_Cten  = 0;
  transaction.GYRO_CONFIG.YGYRO_Cten  = 0;
  transaction.GYRO_CONFIG.XGYRO_Cten  = 0;
  write_MPU9250_register( MPU9250_REGISTER_GYRO_CONFIG, transaction, CS_PIN, gpio, spi );
  transaction.ACCEL_CONFIG.reserved     = 0; // set accelerometer full-scale to 2g, maximum sensitivity
  transaction.ACCEL_CONFIG.ACCEL_FS_SEL = 0;
  transaction.ACCEL_CONFIG.az_st_en     = 0;
  transaction.ACCEL_CONFIG.ay_st_en     = 0;
  transaction.ACCEL_CONFIG.ax_st_en     = 0;
  write_MPU9250_register( MPU9250_REGISTER_ACCEL_CONFIG, transaction, CS_PIN, gpio, spi );

  calibration_accelerometer->scale = 2.0/32768.0;  // measurement scale/signed numeric range
  calibration_accelerometer->offset_x = 0;
  calibration_accelerometer->offset_y = 0;
  calibration_accelerometer->offset_z = 0;

  calibration_gyroscope->scale = 250.0/32768.0;
  calibration_gyroscope->offset_x = 0;
  calibration_gyroscope->offset_y = 0;
  calibration_gyroscope->offset_z = 0;

  // configure FIFO to capture accelerometer and gyro data for bias calculation
  transaction.USER_CTRL.SIG_COND_RST = 0; // enable FIFO
  transaction.USER_CTRL.I2C_MST_RST  = 0;
  transaction.USER_CTRL.FIFO_RST     = 0;
  transaction.USER_CTRL.reserved0    = 0;
  transaction.USER_CTRL.I2C_IF_DIS   = 0;
  transaction.USER_CTRL.I2C_MST_EN   = 0;
  transaction.USER_CTRL.FIFO_EN      = 1;
  transaction.USER_CTRL.reserved1    = 0;
  write_MPU9250_register( MPU9250_REGISTER_USER_CTRL, transaction, CS_PIN, gpio, spi );
  transaction.FIFO_EN.SLV0         = 0; // enable gyro and accelerometer sensors for FIFO (max size 512 bytes in MPU9250)
  transaction.FIFO_EN.SLV1         = 0;
  transaction.FIFO_EN.SLV2         = 0;
  transaction.FIFO_EN.ACCEL        = 1;
  transaction.FIFO_EN.GYRO_ZO_UT   = 1;
  transaction.FIFO_EN.GYRO_YO_UT   = 1;
  transaction.FIFO_EN.GYRO_XO_UT   = 1;
  transaction.FIFO_EN.TEMP_FIFO_EN = 0;
  write_MPU9250_register( MPU9250_REGISTER_FIFO_EN, transaction, CS_PIN, gpio, spi );
  usleep( 40000 );  // accumulate 40 samples in 40 milliseconds = 480 bytes

  // at end of sample accumulation, turn off FIFO sensor read
  transaction.FIFO_EN.SLV0         = 0; // disable gyro and accelerometer sensors for FIFO
  transaction.FIFO_EN.SLV1         = 0;
  transaction.FIFO_EN.SLV2         = 0;
  transaction.FIFO_EN.ACCEL        = 0;
  transaction.FIFO_EN.GYRO_ZO_UT   = 0;
  transaction.FIFO_EN.GYRO_YO_UT   = 0;
  transaction.FIFO_EN.GYRO_XO_UT   = 0;
  transaction.FIFO_EN.TEMP_FIFO_EN = 0;
  write_MPU9250_register( MPU9250_REGISTER_FIFO_EN, transaction, CS_PIN, gpio, spi );
  read_MPU9250_registers( MPU9250_REGISTER_FIFO_COUNTH, data_block_fifo_count, sizeof(data_block_fifo_count), CS_PIN, gpio, spi ); // read FIFO sample count
  reconstructor.field.H = data_block_fifo_count[0];
  reconstructor.field.L = data_block_fifo_count[1];
  packet_count = reconstructor.unsigned_value / 12; // how many sets of full gyro and accelerometer data for averaging

  accel_bias_x = 0;
  accel_bias_y = 0;
  accel_bias_z = 0;
  gyro_bias_x = 0;
  gyro_bias_y = 0;
  gyro_bias_z = 0;
  for (ii = 0; ii < packet_count; ii++)
  {
    read_MPU9250_registers( MPU9250_REGISTER_FIFO_R_W, data_block_fifo_packet, sizeof(data_block_fifo_packet), CS_PIN, gpio, spi ); // read data for averaging

    reconstructor_accel_x.field.H = data_block_fifo_packet[0];
    reconstructor_accel_x.field.L = data_block_fifo_packet[1];
    reconstructor_accel_y.field.H = data_block_fifo_packet[2];
    reconstructor_accel_y.field.L = data_block_fifo_packet[3];
    reconstructor_accel_z.field.H = data_block_fifo_packet[4];
    reconstructor_accel_z.field.L = data_block_fifo_packet[5];
    reconstructor_gyro_x.field.H  = data_block_fifo_packet[6];
    reconstructor_gyro_x.field.L  = data_block_fifo_packet[7];
    reconstructor_gyro_y.field.H  = data_block_fifo_packet[8];
    reconstructor_gyro_y.field.L  = data_block_fifo_packet[9];
    reconstructor_gyro_z.field.H  = data_block_fifo_packet[10];
    reconstructor_gyro_z.field.L  = data_block_fifo_packet[11];

    accel_bias_x += reconstructor_accel_x.signed_value; // sum individual signed 16-bit biases to get accumulated signed 32-bit biases
    accel_bias_y += reconstructor_accel_y.signed_value;
    accel_bias_z += reconstructor_accel_z.signed_value;
    gyro_bias_x  += reconstructor_gyro_x.signed_value;
    gyro_bias_y  += reconstructor_gyro_y.signed_value;
    gyro_bias_z  += reconstructor_gyro_z.signed_value;
  }
  accel_bias_x /= (int32_t)packet_count;
  accel_bias_y /= (int32_t)packet_count;
  accel_bias_z /= (int32_t)packet_count;
  gyro_bias_x /= (int32_t)packet_count;
  gyro_bias_y /= (int32_t)packet_count;
  gyro_bias_z /= (int32_t)packet_count;

  if (accel_bias_z > 0) // remove gravity from the z-axis accelerometer bias calculation
  {
    accel_bias_z -= (int32_t)(1.0/calibration_accelerometer->scale);
  }
  else
  {
    accel_bias_z += (int32_t)(1.0/calibration_accelerometer->scale);
  }

  // the code that this is based off of tried to push the bias calculation values to hardware correction registers
  // these registers do not appear to be functioning, so rely on software offset correction

  // output scaled gyro biases
  calibration_gyroscope->offset_x = ((float)gyro_bias_x)*calibration_gyroscope->scale;
  calibration_gyroscope->offset_y = ((float)gyro_bias_y)*calibration_gyroscope->scale;
  calibration_gyroscope->offset_z = ((float)gyro_bias_z)*calibration_gyroscope->scale;

  // output scaled accelerometer biases
  calibration_accelerometer->offset_x = ((float)accel_bias_x)*calibration_accelerometer->scale;
  calibration_accelerometer->offset_y = ((float)accel_bias_y)*calibration_accelerometer->scale;
  calibration_accelerometer->offset_z = ((float)accel_bias_z)*calibration_accelerometer->scale;

  return;
}

void initialize_accelerometer_and_gyroscope(
    struct calibration_data *     calibration_accelerometer,
    struct calibration_data *     calibration_gyroscope,
    volatile struct spi_register *spi,
    volatile struct gpio_register*gpio )
{
  union MPU9250_transaction_field_data  transaction;

  /* print WHO_AM_I */
  printf( "accel WHOAMI (0x71) = 0x%2.2X\n",
      read_MPU9250_register( MPU9250_REGISTER_WHO_AM_I, CS_PIN, gpio, spi ).WHO_AM_I.WHOAMI );

  // based off https://github.com/brianc118/MPU9250/blob/master/MPU9250.cpp

  calibrate_accelerometer_and_gyroscope( calibration_accelerometer, calibration_gyroscope, spi, gpio );

  // reset MPU9205
  transaction.PWR_MGMT_1.CLKSEL        = 0;
  transaction.PWR_MGMT_1.PD_PTAT       = 0;
  transaction.PWR_MGMT_1.GYRO_STANDBY  = 0;
  transaction.PWR_MGMT_1.CYCLE         = 0;
  transaction.PWR_MGMT_1.SLEEP         = 0;
  transaction.PWR_MGMT_1.H_RESET       = 1;
  write_MPU9250_register( MPU9250_REGISTER_PWR_MGMT_1, transaction, CS_PIN, gpio, spi );
  usleep( 1000 ); // wait for all registers to reset

  // clock source
  transaction.PWR_MGMT_1.CLKSEL       = 1;
  transaction.PWR_MGMT_1.PD_PTAT      = 0;
  transaction.PWR_MGMT_1.GYRO_STANDBY = 0;
  transaction.PWR_MGMT_1.CYCLE        = 0;
  transaction.PWR_MGMT_1.SLEEP        = 0;
  transaction.PWR_MGMT_1.H_RESET      = 0;
  write_MPU9250_register( MPU9250_REGISTER_PWR_MGMT_1, transaction, CS_PIN, gpio, spi );

  // enable acc & gyro
  transaction.PWR_MGMT_2.DIS_ZG   = 0;
  transaction.PWR_MGMT_2.DIS_YG   = 0;
  transaction.PWR_MGMT_2.DIS_XG   = 0;
  transaction.PWR_MGMT_2.DIS_ZA   = 0;
  transaction.PWR_MGMT_2.DIS_YA   = 0;
  transaction.PWR_MGMT_2.DIS_XA   = 0;
  transaction.PWR_MGMT_2.reserved = 0;
  write_MPU9250_register( MPU9250_REGISTER_PWR_MGMT_1, transaction, CS_PIN, gpio, spi );

  // use DLPF set gyro bandwidth 184Hz, temperature bandwidth 188Hz
  transaction.CONFIG.DLPF_CFG     = 1;
  transaction.CONFIG.EXT_SYNC_SET = 0;
  transaction.CONFIG.FIFO_MODE    = 0;
  transaction.CONFIG.reserved     = 0;
  write_MPU9250_register( MPU9250_REGISTER_CONFIG, transaction, CS_PIN, gpio, spi );

  // +-250dps
  transaction.GYRO_CONFIG.FCHOICE_B   = 0;
  transaction.GYRO_CONFIG.reserved    = 0;
  transaction.GYRO_CONFIG.GYRO_FS_SEL = 0;
  transaction.GYRO_CONFIG.ZGYRO_Cten  = 0;
  transaction.GYRO_CONFIG.YGYRO_Cten  = 0;
  transaction.GYRO_CONFIG.XGYRO_Cten  = 0;
  write_MPU9250_register( MPU9250_REGISTER_GYRO_CONFIG, transaction, CS_PIN, gpio, spi );

  // +-2G
  transaction.ACCEL_CONFIG.reserved     = 0;
  transaction.ACCEL_CONFIG.ACCEL_FS_SEL = 0;
  transaction.ACCEL_CONFIG.az_st_en     = 0;
  transaction.ACCEL_CONFIG.ay_st_en     = 0;
  transaction.ACCEL_CONFIG.ax_st_en     = 0;
  write_MPU9250_register( MPU9250_REGISTER_ACCEL_CONFIG, transaction, CS_PIN, gpio, spi );

  // set acc data rates,enable acc LPF, bandwidth 184Hz
  transaction.ACCEL_CONFIG_2.A_DLPF_CFG      = 0;
  transaction.ACCEL_CONFIG_2.ACCEL_FCHOICE_B = 0;
  transaction.ACCEL_CONFIG_2.reserved        = 0;
  write_MPU9250_register( MPU9250_REGISTER_ACCEL_CONFIG_2, transaction, CS_PIN, gpio, spi );

  // I2C master mode and set I2C_IF_DIS to disable slave mode I2C bus
  transaction.USER_CTRL.SIG_COND_RST = 0;
  transaction.USER_CTRL.I2C_MST_RST  = 0;
  transaction.USER_CTRL.FIFO_RST     = 0;
  transaction.USER_CTRL.reserved0    = 0;
  transaction.USER_CTRL.I2C_IF_DIS   = 1;
  transaction.USER_CTRL.I2C_MST_EN   = 1;
  transaction.USER_CTRL.FIFO_EN      = 0;
  transaction.USER_CTRL.reserved1    = 0;
  write_MPU9250_register( MPU9250_REGISTER_USER_CTRL, transaction, CS_PIN, gpio, spi );

  // I2C configuration multi-master IIC 400KHz
  transaction.I2C_MST_CTRL.I2C_MST_CLK   = 13;
  transaction.I2C_MST_CTRL.I2C_MST_P_NSR = 0;
  transaction.I2C_MST_CTRL.SLV_3_FIFO_EN = 0;
  transaction.I2C_MST_CTRL.WAIT_FOR_ES   = 0;
  transaction.I2C_MST_CTRL.MULT_MST_EN   = 0;
  write_MPU9250_register( MPU9250_REGISTER_I2C_MST_CTRL, transaction, CS_PIN, gpio, spi );

  return;
}

void initialize_magnetometer(
    struct calibration_data *     calibration_magnetometer,
    volatile struct spi_register *spi,
    volatile struct gpio_register*gpio )
{
  union MPU9250_transaction_field_data  transaction;
  uint8_t                               data_block[3];

  // read WHOAMI from the magnetometer
  transaction.I2C_SLV0_ADDR.I2C_ID_0      = AK8963_ADDRESS;
  transaction.I2C_SLV0_ADDR.I2C_SLV0_RNW  = 1;
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_ADDR, transaction, CS_PIN, gpio, spi );
  transaction.I2C_SLV0_REG.I2C_SLV0_REG = AK8963_REGISTER_WIA;
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_REG, transaction, CS_PIN, gpio, spi );
  transaction.I2C_SLV0_CTRL.I2C_SLV0_LENG     = 1;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_GRP      = 0;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_REG_DIS  = 0;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_BYTE_SW  = 0;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_EN       = 1;
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_CTRL, transaction, CS_PIN, gpio, spi );
  usleep( 1000 );
  transaction = read_MPU9250_register( MPU9250_REGISTER_EXT_SENS_DATA_00, CS_PIN, gpio, spi );
  printf( "mag WHOAMI (0x48): 0x%2.2X\n", transaction.WIA.WIA );

  // set the I2C slave address of AK8963 and set for write
  transaction.I2C_SLV0_ADDR.I2C_ID_0      = AK8963_ADDRESS;
  transaction.I2C_SLV0_ADDR.I2C_SLV0_RNW  = 0;
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_ADDR, transaction, CS_PIN, gpio, spi );
  // I2C slave 0 register address from where to begin data transfer
  transaction.I2C_SLV0_REG.I2C_SLV0_REG = AK8963_REGISTER_CNTL2;
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_REG, transaction, CS_PIN, gpio, spi );
  // reset AK8963
  transaction.CNTL2.SRST      = 1;
  transaction.CNTL2.reserved  = 0;
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_DO, transaction, CS_PIN, gpio, spi );
  // enable I2C and set 1 byte
  transaction.I2C_SLV0_CTRL.I2C_SLV0_LENG     = 1;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_GRP      = 0;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_REG_DIS  = 0;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_BYTE_SW  = 0;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_EN       = 1;
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_CTRL, transaction, CS_PIN, gpio, spi );
  usleep( 1000 );

  // I2C slave 0 register address from where to being data transfer
  transaction.I2C_SLV0_REG.I2C_SLV0_REG = AK8963_REGISTER_CNTL1;
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_REG, transaction, CS_PIN, gpio, spi );
  // register value to 100Hz continuous measurement in 14bit
  transaction.CNTL1.MODE      = 6;
  transaction.CNTL1.BIT       = 0;
  transaction.CNTL1.reserved  = 0;
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_DO, transaction, CS_PIN, gpio, spi );
  // enable I2C and set 1 byte
  transaction.I2C_SLV0_CTRL.I2C_SLV0_LENG     = 1;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_GRP      = 0;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_REG_DIS  = 0;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_BYTE_SW  = 0;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_EN       = 1;
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_CTRL, transaction, CS_PIN, gpio, spi );
  usleep( 1000 );

  // get the magnetometer calibration... extracted from the "calib_mag" function at https://github.com/brianc118/MPU9250/blob/master/MPU9250.cpp
  transaction.I2C_SLV0_ADDR.I2C_ID_0          = AK8963_ADDRESS;                             // set the I2C slave address of the AK8963 and set for read
  transaction.I2C_SLV0_ADDR.I2C_SLV0_RNW      = 1;
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_ADDR, transaction, CS_PIN, gpio, spi );
  transaction.I2C_SLV0_REG.I2C_SLV0_REG       = AK8963_REGISTER_ASAX;                       // I2C slave 0 register address from where to begin data transfer
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_REG, transaction, CS_PIN, gpio, spi );
  transaction.I2C_SLV0_CTRL.I2C_SLV0_LENG     = 3;                                          // read 3 bytes from the magnetometer
  transaction.I2C_SLV0_CTRL.I2C_SLV0_GRP      = 0;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_REG_DIS  = 0;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_BYTE_SW  = 0;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_EN       = 1;
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_CTRL, transaction, CS_PIN, gpio, spi );

  usleep( 200000 );

  read_MPU9250_registers( MPU9250_REGISTER_EXT_SENS_DATA_00, data_block, sizeof(data_block), CS_PIN, gpio, spi );
  calibration_magnetometer->scale = (float)1;
  calibration_magnetometer->offset_x = ((((float)data_block[0])-128.0)/256.0+1.0);
  calibration_magnetometer->offset_y = ((((float)data_block[1])-128.0)/256.0+1.0);
  calibration_magnetometer->offset_z = ((((float)data_block[2])-128.0)/256.0+1.0);

  return;
}

void read_accelerometer_gyroscope(
    struct calibration_data *     calibration_accelerometer,
    struct calibration_data *     calibration_gyroscope,
    volatile struct spi_register *spi,
    volatile struct gpio_register*gpio,
    double gyroOut[],
    double accelOut[] )
{
  uint8_t                   data_block[6+2+6];
  union uint16_to_2uint8    ACCEL_XOUT;
  union uint16_to_2uint8    ACCEL_YOUT;
  union uint16_to_2uint8    ACCEL_ZOUT;
  union uint16_to_2uint8    GYRO_XOUT;
  union uint16_to_2uint8    GYRO_YOUT;
  union uint16_to_2uint8    GYRO_ZOUT;

  /*
   * poll the interrupt status register and it tells you when it is done
   * once it is done, read the data registers
   */
  do
  {
    usleep( 1000 );
  } while (read_MPU9250_register( MPU9250_REGISTER_INT_STATUS, CS_PIN, gpio, spi ).INT_STATUS.RAW_DATA_RDY_INT == 0);

  // read the accelerometer values
  read_MPU9250_registers( MPU9250_REGISTER_ACCEL_XOUT_H, data_block, sizeof(data_block), CS_PIN, gpio, spi );
  ACCEL_XOUT.field.H  = data_block[0];
  ACCEL_XOUT.field.L  = data_block[1];
  ACCEL_YOUT.field.H  = data_block[2];
  ACCEL_YOUT.field.L  = data_block[3];
  ACCEL_ZOUT.field.H  = data_block[4];
  ACCEL_ZOUT.field.L  = data_block[5];
  // TEMP_OUT.field.H = data_block[6];
  // TEMP_OUT.field.L = data_block[7];
  GYRO_XOUT.field.H   = data_block[8];
  GYRO_XOUT.field.L   = data_block[9];
  GYRO_YOUT.field.H   = data_block[10];
  GYRO_YOUT.field.L   = data_block[11];
  GYRO_ZOUT.field.H   = data_block[12];
  GYRO_ZOUT.field.L   = data_block[13];

  /**
  printf( "Gyro X: %.2f deg\ty=%.2f deg\tz=%.2f deg\n",
      GYRO_XOUT.signed_value*calibration_gyroscope->scale - calibration_gyroscope->offset_x,
      GYRO_YOUT.signed_value*calibration_gyroscope->scale - calibration_gyroscope->offset_y,
      GYRO_ZOUT.signed_value*calibration_gyroscope->scale - calibration_gyroscope->offset_z );

  printf( "Accel X: %.2f m/s^2\ty=%.2f m/s^2\tz=%.2f m/s^2\n",
      (ACCEL_XOUT.signed_value*calibration_accelerometer->scale - calibration_accelerometer->offset_x)*9.81,
      (ACCEL_YOUT.signed_value*calibration_accelerometer->scale - calibration_accelerometer->offset_y)*9.81,
      (ACCEL_ZOUT.signed_value*calibration_accelerometer->scale - calibration_accelerometer->offset_z)*9.81 );
  **/
  
  gyroOut[0] = GYRO_XOUT.signed_value*calibration_gyroscope->scale - calibration_gyroscope->offset_x;
  gyroOut[1] = GYRO_YOUT.signed_value*calibration_gyroscope->scale - calibration_gyroscope->offset_y;
  gyroOut[2] = GYRO_ZOUT.signed_value*calibration_gyroscope->scale - calibration_gyroscope->offset_z;
  
  accelOut[0] = (ACCEL_XOUT.signed_value*calibration_accelerometer->scale - calibration_accelerometer->offset_x)*9.81;
  accelOut[1] = (ACCEL_YOUT.signed_value*calibration_accelerometer->scale - calibration_accelerometer->offset_y)*9.81;
  accelOut[2] = (ACCEL_ZOUT.signed_value*calibration_accelerometer->scale - calibration_accelerometer->offset_z)*9.81;

  return;
}

void read_magnetometer(
    struct calibration_data *     calibration_magnetometer,
    volatile struct spi_register *spi,
    volatile struct gpio_register*gpio,
    double magOut[] )
{
  uint8_t                               data_block[7];
  union uint16_to_2uint8                MAG_XOUT;
  union uint16_to_2uint8                MAG_YOUT;
  union uint16_to_2uint8                MAG_ZOUT;
  union MPU9250_transaction_field_data  transaction;

  transaction.I2C_SLV0_ADDR.I2C_ID_0      = AK8963_ADDRESS; // set the I2C slave address of the AK8963 and set for read
  transaction.I2C_SLV0_ADDR.I2C_SLV0_RNW  = 1;
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_ADDR, transaction, CS_PIN, gpio, spi );
  transaction.I2C_SLV0_REG.I2C_SLV0_REG   = AK8963_REGISTER_HXL;  // I2C slave 0 register address from where to begin data transfer
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_REG, transaction, CS_PIN, gpio, spi );
  transaction.I2C_SLV0_CTRL.I2C_SLV0_LENG     = 7;  // read 7 bytes from the magnetometer
  transaction.I2C_SLV0_CTRL.I2C_SLV0_GRP      = 0;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_REG_DIS  = 0;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_BYTE_SW  = 0;
  transaction.I2C_SLV0_CTRL.I2C_SLV0_EN       = 1;
  write_MPU9250_register( MPU9250_REGISTER_I2C_SLV0_CTRL, transaction, CS_PIN, gpio, spi );
  usleep( 1000 );

  read_MPU9250_registers( MPU9250_REGISTER_EXT_SENS_DATA_00, data_block, 7, CS_PIN, gpio, spi );
  // read must start from HXL and read seven bytes so that ST2 is read and the AK8963 will start the next conversion
  MAG_XOUT.field.L = data_block[0];
  MAG_XOUT.field.H = data_block[1];
  MAG_YOUT.field.L = data_block[2];
  MAG_YOUT.field.H = data_block[3];
  MAG_ZOUT.field.L = data_block[4];
  MAG_ZOUT.field.H = data_block[5];
  /**
  printf( "Mag X: %.2f uT\ty=%.2f uT\tz=%.2f uT\n",
      MAG_XOUT.signed_value*calibration_magnetometer->offset_x,
      MAG_YOUT.signed_value*calibration_magnetometer->offset_y,
      MAG_ZOUT.signed_value*calibration_magnetometer->offset_z );
  **/
  
  magOut[0] = MAG_XOUT.signed_value*calibration_magnetometer->offset_x;
  magOut[1] = MAG_YOUT.signed_value*calibration_magnetometer->offset_y;
  magOut[2] = MAG_ZOUT.signed_value*calibration_magnetometer->offset_z;
  return;
}


//int main( void )
//{
//  volatile struct io_peripherals *io;
//  struct calibration_data         calibration_accelerometer;
//  struct calibration_data         calibration_gyroscope;
//  struct calibration_data         calibration_magnetometer;

//  io = import_registers();
//  if (io != NULL)
//  {
    /* print where the I/O memory was actually mapped to */
//    printf( "mem at 0x%8.8X\n", (unsigned int)io );

    /* set the pin function to alternate function 0 for GPIO09 (SPI, MISO) */
    /* set the pin function to alternate function 0 for GPIO10 (SPI, MOSI) */
    /* set the pin function to alternate function 0 for GPIO11 (SPI, SCK) */
    /* set the pin function to output for GPIO08 (SPI, CS) */
//    io->gpio.GPFSEL0.field.FSEL9 = GPFSEL_ALTERNATE_FUNCTION0;
//    io->gpio.GPFSEL1.field.FSEL0 = GPFSEL_ALTERNATE_FUNCTION0;
//    io->gpio.GPFSEL1.field.FSEL1 = GPFSEL_ALTERNATE_FUNCTION0;
//    io->gpio.GPFSEL0.field.FSEL8 = GPFSEL_OUTPUT;

    /* set initial output state */
//    GPIO_SET(&(io->gpio), CS_PIN);
//    usleep( 100000 );

    /* set up the SPI parameters */
//    io->spi.CLK.field.CDIV = ((ROUND_DIVISION(APB_CLOCK,800000))>>1)<<1; /* this number must be even, so shift the LSb into oblivion */
//    io->spi.CS.field.CS       = 0;
//    io->spi.CS.field.CPHA     = 1;  /* clock needs to idle high and clock in data on the rising edge */
//    io->spi.CS.field.CPOL     = 1;
//    io->spi.CS.field.CLEAR    = 0;
//    io->spi.CS.field.CSPOL    = 0;
//    io->spi.CS.field.TA       = 0;
//    io->spi.CS.field.DMAEN    = 0;
//    io->spi.CS.field.INTD     = 0;
//    io->spi.CS.field.INTR     = 0;
//    io->spi.CS.field.ADCS     = 0;
//    io->spi.CS.field.REN      = 0;
//    io->spi.CS.field.LEN      = 0;
    /* io->spi.CS.field.LMONO */
    /* io->spi.CS.field.TE_EN */
    /* io->spi.CS.field.DONE */
    /* io->spi.CS.field.RXD */
    /* io->spi.CS.field.TXD */
    /* io->spi.CS.field.RXR */
    /* io->spi.CS.field.RXF */
//    io->spi.CS.field.CSPOL0   = 0;
//    io->spi.CS.field.CSPOL1   = 0;
//    io->spi.CS.field.CSPOL2   = 0;
//    io->spi.CS.field.DMA_LEN  = 0;
//    io->spi.CS.field.LEN_LONG = 0;

//    initialize_accelerometer_and_gyroscope( &calibration_accelerometer, &calibration_gyroscope, &(io->spi), &(io->gpio) );
//    initialize_magnetometer( &calibration_magnetometer, &(io->spi), &(io->gpio) );
//    do
//    {
//      read_accelerometer_gyroscope( &calibration_accelerometer, &calibration_gyroscope, &(io->spi), &(io->gpio) );
//      read_magnetometer( &calibration_magnetometer, &(io->spi), &(io->gpio) );
//      printf( "\n" );
//    } while (!wait_key( 100, 0 ));
//  }
//  else
//  {
//    ; /* warning message already issued */
//  }

//  return 0;
//}
