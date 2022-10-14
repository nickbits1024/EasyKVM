#define I2C_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000     /*!< I2C master clock frequency */

#define DDC_SCL_GPIO_NUM    GPIO_NUM_1
#define DDC_SDA_GPIO_NUM    GPIO_NUM_2
#define DDC_I2C_PORT        I2C_NUM_0
#define DDC_I2C_ADDRESS     0x37

#define DDC_I2C_TIMEOUT     (1000 / portTICK_PERIOD_MS)
