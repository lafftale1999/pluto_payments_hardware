#ifndef _PROJECT_CONFIG_H
#define _PROJECT_CONFIG_H


// SET UP FOR I2C COMMUNICATION. USED FOR THE LCD 16X2 SCREEN.
#define I2C_MASTER_SCL_IO           GPIO_NUM_22                  /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           GPIO_NUM_21                  /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          100000                      /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000
#define I2C_DEVICE_ADDRESS_LEN      I2C_ADDR_BIT_LEN_7

// SET UP FOR SPI COMMUNICATION. USED FOR THE RFIC-RC522
#define RC522_SPI_BUS_GPIO_MISO    GPIO_NUM_19
#define RC522_SPI_BUS_GPIO_MOSI    GPIO_NUM_23
#define RC522_SPI_BUS_GPIO_SCLK    GPIO_NUM_18
#define RC522_SPI_SCANNER_GPIO_SDA GPIO_NUM_5
#define RC522_SCANNER_GPIO_RST     (-1) // soft-reset

// COLUMN AND ROW PINS USED FOR THE KEYPAD LOGIC
#define KEYPAD_ROW_PINS {GPIO_NUM_26, GPIO_NUM_25, GPIO_NUM_17, GPIO_NUM_16}
#define KEYPAD_COL_PINS {GPIO_NUM_27, GPIO_NUM_14, GPIO_NUM_12, GPIO_NUM_13}

#endif