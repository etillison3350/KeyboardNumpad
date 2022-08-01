#include "keyboard_i2c.h"

#include <string.h>

static struct i2c_slave_module g_I2CPeripheralInstance;

static uint8_t g_I2CData1[KBD_I2C_DATA_LEN];
static uint8_t g_I2CData2[KBD_I2C_DATA_LEN];

static uint8_t *g_I2CDataProduce = g_I2CData1;
static uint8_t *g_I2CDataTransmit = g_I2CData2;

static struct i2c_slave_packet g_I2CPacketSend;
static struct i2c_slave_packet g_I2CPacketReceive;

static uint8_t g_I2CRegisterAddress = 0;

static uint8_t g_I2CReceiveBuffer[KBD_I2C_RCV_SIZE];

static i2c_kbd_data_callback_t g_I2CDataCallback;
static bool g_I2CDataCallbackEnable = false;

void configure_i2c_peripheral(void)
{
	struct i2c_slave_config i2cconfig;
	i2c_slave_get_config_defaults(&i2cconfig);

	i2cconfig.address = KBD_I2C_PERIPHERAL_ADDR;
	i2cconfig.address_mode = I2C_SLAVE_ADDRESS_MODE_MASK;
	
	i2cconfig.generator_source = GCLK_GENERATOR_3;
	
	i2c_slave_init(&g_I2CPeripheralInstance, KBD_I2C_SERCOM_IFACE, &i2cconfig);
	i2c_slave_enable(&g_I2CPeripheralInstance);

	i2c_slave_register_callback(&g_I2CPeripheralInstance, i2c_read_request_callback, I2C_SLAVE_CALLBACK_READ_REQUEST);
	i2c_slave_enable_callback(&g_I2CPeripheralInstance, I2C_SLAVE_CALLBACK_READ_REQUEST);

	i2c_slave_register_callback(&g_I2CPeripheralInstance, i2c_write_request_callback, I2C_SLAVE_CALLBACK_WRITE_REQUEST);
	i2c_slave_enable_callback(&g_I2CPeripheralInstance, I2C_SLAVE_CALLBACK_WRITE_REQUEST);
	i2c_slave_register_callback(&g_I2CPeripheralInstance, i2c_read_complete_callback, I2C_SLAVE_CALLBACK_READ_COMPLETE);
}

void set_i2c_kbd_data(uint8_t *data)
{
    system_interrupt_enter_critical_section();
    memcpy(g_I2CDataProduce, data, KBD_I2C_DATA_LEN);
    system_interrupt_leave_critical_section();
}

void i2c_read_request_callback(struct i2c_slave_module *const module)
{
    if (g_I2CRegisterAddress == KBD_I2C_REG_KEY_DATA)
    {
        system_interrupt_enter_critical_section();
        
        uint8_t *tmp = g_I2CDataProduce;
        g_I2CDataProduce = g_I2CDataTransmit;
        g_I2CDataTransmit = tmp;

        system_interrupt_leave_critical_section();

        g_I2CPacketSend.data_length = KBD_I2C_DATA_LEN;
        g_I2CPacketSend.data = g_I2CDataTransmit;

        i2c_slave_write_packet_job(module, &g_I2CPacketSend);
    }
}

void i2c_write_request_callback(struct i2c_slave_module *const module)
{
    g_I2CPacketReceive.data_length = KBD_I2C_RCV_SIZE;
    g_I2CPacketReceive.data = g_I2CReceiveBuffer;

    if (i2c_slave_read_packet_job(module, &g_I2CPacketReceive) == STATUS_OK)
    {
        i2c_slave_enable_callback(&g_I2CPeripheralInstance, I2C_SLAVE_CALLBACK_READ_COMPLETE);
    }
}

void i2c_read_complete_callback(struct i2c_slave_module *const module)
{
    i2c_slave_disable_callback(&g_I2CPeripheralInstance, I2C_SLAVE_CALLBACK_READ_COMPLETE);

    g_I2CRegisterAddress = g_I2CReceiveBuffer[0];
    if (g_I2CDataCallbackEnable && g_I2CRegisterAddress >= 0x80)
    {
        g_I2CDataCallback(g_I2CRegisterAddress, g_I2CReceiveBuffer[1]);
    }
}

void i2c_kbd_data_register_callback(i2c_kbd_data_callback_t callback)
{
    g_I2CDataCallback = callback;
}

void i2c_kbd_data_enable_callback(void)
{
    g_I2CDataCallbackEnable = true;
}

void i2c_kbd_data_disable_callback(void)
{
    g_I2CDataCallbackEnable = false;
}
