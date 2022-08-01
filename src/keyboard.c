#include "keyboard.h"

#include <string.h>

#define MAX_KEYPRESSES 6

static bool g_isUsbOperationMode;

static struct
{
	uint32_t row_mask;
	uint32_t col_mask;

	struct port_config row_read_config;
	struct port_config row_disable_config;
} g_keyPinConsts;

struct kbd_keypress_info
{
	uint8_t modifier_code;
	uint8_t multimedia_code;
	uint8_t keypress_array[MAX_KEYPRESSES];
	uint8_t num_keypresses;
};

static volatile bool g_enableKeyboard = false;
static volatile bool g_enableMultimedia = false;

static volatile struct
{
	bool last_pin_a;
	int8_t pressed;
	int8_t event;
} g_encoderState = { true, -1, -1 };

struct adc_module g_adcInstance;
static uint16_t g_adcResult;
static volatile bool g_adcReadDone = false;

static struct tc_module g_tcKbdInstance;
static struct tc_module g_tcEncInstance;

static struct dac_module g_dacInstance;

void configure_pins(void)
{
	// Calculate the row mask (for configuring multiple rows at a time)
	g_keyPinConsts.row_mask = 0;
	for (unsigned r = 0; r < NUM_ROWS; r++)
	{
		g_keyPinConsts.row_mask |= (1 << ROWMAP[r]);
	}

	// Fill config data for rows that are disabled (not currently being read)
	// These rows are tri-stated
	port_get_config_defaults(&g_keyPinConsts.row_disable_config);
	g_keyPinConsts.row_disable_config.powersave = true;
	port_group_set_config(&PORTA, g_keyPinConsts.row_mask, &g_keyPinConsts.row_disable_config);

	// Fill config data for the actively-read row
	// This row is set as an output
	port_get_config_defaults(&g_keyPinConsts.row_read_config);
	g_keyPinConsts.row_read_config.direction = PORT_PIN_DIR_OUTPUT;

	// Calculate the column mask (for configuring/reading multiple columns at a time)
	g_keyPinConsts.col_mask = 0;
	for (unsigned c = 0; c < NUM_COLS; c++)
	{
		g_keyPinConsts.col_mask |= (1 << COLMAP[c]);
	}
	
	// Configure columns. Columns are set as inputs with pull-ups enabled
	struct port_config colconfig;
	port_get_config_defaults(&colconfig);
	colconfig.direction = PORT_PIN_DIR_INPUT;
	colconfig.input_pull = PORT_PIN_PULL_UP;
	port_group_set_config(&PORTA, g_keyPinConsts.col_mask, &colconfig);
	
	// Set rotary encoder pins as input w/ pullup
	struct port_config encconfig;
	port_get_config_defaults(&encconfig);
	encconfig.direction = PORT_PIN_DIR_INPUT;
	encconfig.input_pull = PORT_PIN_PULL_UP;
	port_group_set_config(&PORTA, (1 << PIN_KBD_ENC_A) | (1 << PIN_KBD_ENC_B), &encconfig);
}

void configure_adc(void)
{
	struct adc_config adcconfig;
	adc_get_config_defaults(&adcconfig);
	
	adcconfig.gain_factor = ADC_GAIN_FACTOR_1X;
	adcconfig.clock_prescaler = ADC_CLOCK_PRESCALER_DIV8;
	adcconfig.reference = ADC_REFERENCE_INTVCC0;
	adcconfig.resolution = ADC_RESOLUTION_8BIT;
	adcconfig.positive_input = PIN_KBD_ID_CHAN;
	
	adcconfig.clock_source = GCLK_GENERATOR_3;
	
	adc_init(&g_adcInstance, ADC, &adcconfig);
	adc_enable(&g_adcInstance);
	
	adc_register_callback(&g_adcInstance, adc_complete_callback, ADC_CALLBACK_READ_BUFFER);
	adc_enable_callback(&g_adcInstance, ADC_CALLBACK_READ_BUFFER);
}

void configure_i2c(void)
{
	i2c_kbd_data_register_callback(i2c_data_callback);
	i2c_kbd_data_enable_callback();
}

void configure_tc(void)
{
	struct tc_config timerconfig;
	tc_get_config_defaults(&timerconfig);
		
	timerconfig.counter_size = TC_COUNTER_SIZE_8BIT;
	timerconfig.clock_source = GCLK_GENERATOR_3;
	timerconfig.clock_prescaler = TC_CLOCK_PRESCALER_DIV256;
	timerconfig.counter_8_bit.period = 125;
	timerconfig.counter_8_bit.compare_capture_channel[0] = 100;
		
	tc_init(&g_tcKbdInstance, TC3, &timerconfig);
	tc_enable(&g_tcKbdInstance);
		
	tc_register_callback(&g_tcKbdInstance, keyboard_scan_tc_callback, TC_CALLBACK_CC_CHANNEL0);
	tc_enable_callback(&g_tcKbdInstance, TC_CALLBACK_CC_CHANNEL0);

	struct tc_config encconfig;
	tc_get_config_defaults(&encconfig);
		
	encconfig.counter_size = TC_COUNTER_SIZE_8BIT;
	encconfig.clock_source = GCLK_GENERATOR_3;
	encconfig.clock_prescaler = TC_CLOCK_PRESCALER_DIV16;
	encconfig.counter_8_bit.period = 250;
	encconfig.counter_8_bit.compare_capture_channel[0] = 33;
		
	tc_init(&g_tcEncInstance, TC4, &encconfig);
	tc_enable(&g_tcEncInstance);
		
	tc_register_callback(&g_tcEncInstance, rot_enc_scan_tc_callback, TC_CALLBACK_CC_CHANNEL0);
	tc_enable_callback(&g_tcEncInstance, TC_CALLBACK_CC_CHANNEL0);
}

void configure_usb_hid(void)
{
	udc_start();
}

void configure_dac(void)
{
	struct dac_config dacconfig;
	dac_get_config_defaults(&dacconfig);
	
	dacconfig.reference = DAC_REFERENCE_AVCC;
	dacconfig.left_adjust = false;
	dacconfig.clock_source = GCLK_GENERATOR_3;
	
	dac_init(&g_dacInstance, DAC, &dacconfig);
	dac_enable(&g_dacInstance);
	
	
	struct dac_chan_config chanconfig;
	dac_chan_get_config_defaults(&chanconfig);
	
	dac_chan_set_config(&g_dacInstance, PIN_KBD_LED_CHAN, &chanconfig);
	dac_chan_enable(&g_dacInstance, PIN_KBD_LED_CHAN);
}

void disable_adc(void)
{
	adc_disable(&g_adcInstance);
}

static inline void handle_keypress(uint16_t key_id, struct kbd_keypress_info* keyinfo)
{
	switch (key_id & 0xF000)
	{
		// case KEY_LAYER_META:
		//	// Not implemented
		// 	break;
		case KEY_LAYER_MULTIMEDIA:
			keyinfo->multimedia_code |= key_id & 0xFF;
			break;
		default:
		{
			uint8_t modcode = (key_id >> 8) & 0xF;
			if (modcode > 0)
			{
				keyinfo->modifier_code |= (1 << (modcode - 1));
			}

			uint8_t keycode = key_id & 0xFF;
			if (keycode > 0)
			{
				if (keyinfo->num_keypresses < 6)
				{
					keyinfo->keypress_array[keyinfo->num_keypresses] = keycode;
				}
				keyinfo->num_keypresses++;
			}

			break;
		}
	}
}

void keyboard_scan_tc_callback(struct tc_module *const module)
{
	if (!g_enableKeyboard && !g_enableMultimedia)
	{
		return;
	}
	
	struct kbd_keypress_info keyinfo;
	memset(&keyinfo, 0, sizeof(struct kbd_keypress_info));

	const int8_t encevt = g_encoderState.event;
	if (encevt >= 0)
	{
		handle_keypress(ENCMAP[encevt], &keyinfo);
		
		g_encoderState.event = -1;
	}

	for (unsigned r = 0; r < NUM_ROWS; r++)
	{
		port_group_set_config(&PORTA, g_keyPinConsts.row_mask & (~(1 << ROWMAP[r])), &g_keyPinConsts.row_disable_config);
		port_group_set_config(&PORTA, 1 << ROWMAP[r], &g_keyPinConsts.row_read_config);
		port_group_set_output_level(&PORTA, 1 << ROWMAP[r], 0);

		const uint32_t col_vals = port_group_get_input_level(&PORTA, g_keyPinConsts.col_mask);
		for (unsigned c = 0; c < NUM_COLS; c++)
		{
			bool is_pressed = (col_vals & (1 << COLMAP[c])) == 0;
			if (is_pressed)
			{
				handle_keypress(KEYMAP[r][c], &keyinfo);
			}
		}
	}
	port_group_set_config(&PORTA, g_keyPinConsts.row_mask, &g_keyPinConsts.row_disable_config);
	
	if (g_isUsbOperationMode)
	{
		if (g_enableKeyboard)
		{
			if (keyinfo.num_keypresses > 6)
			{
				memset(keyinfo.keypress_array, 0x01, 6);
			}
			udi_hid_kbd_send_event(keyinfo.modifier_code, keyinfo.keypress_array);
		}

		if (g_enableMultimedia)
		{
			udi_hid_multimedia_send_event(keyinfo.multimedia_code);
		}
	}
	else
	{
		set_i2c_kbd_data((uint8_t *) &keyinfo);
	}

	UNUSED(module);
}

void rot_enc_scan_tc_callback(struct tc_module *const module)
{
	uint8_t enc_input = port_group_get_input_level(&PORTA, (1 << PIN_KBD_ENC_A) | (1 << PIN_KBD_ENC_B));

	bool enc_a = (enc_input & (1 << PIN_KBD_ENC_A)) == 0;
	bool enc_b = (enc_input & (1 << PIN_KBD_ENC_B)) == 0;

	if (enc_a != g_encoderState.last_pin_a)
	{
		if (enc_a)
		{
			g_encoderState.pressed = enc_b;
		}
		else 
		{
			if (g_encoderState.pressed != enc_b)
			{
				g_encoderState.event = g_encoderState.pressed;
			}
			g_encoderState.pressed = -1;
		}

		g_encoderState.last_pin_a = enc_a;
	}

	UNUSED(module);
}


bool hid_keyboard_enable_callback(void)
{
	g_enableKeyboard = true;
	return true;
}

void hid_keyboard_disable_callback(void)
{
	g_enableKeyboard = false;
}


bool hid_multimedia_enable_callback(void)
{
	g_enableMultimedia = true;
	return true;
}

void hid_multimedia_disable_callback(void)
{
	g_enableMultimedia = false;
}


void i2c_data_callback(uint8_t address, uint8_t value)
{
	switch (address)
	{
		case KBD_I2C_REG_BACKLIGHT:
			dac_chan_write(&g_dacInstance, PIN_KBD_LED_CHAN, value << 2);
			break;
	}
}


void adc_complete_callback(struct adc_module *const module)
{
	g_adcReadDone = true;
}

void detect_operation_mode(void)
{
	// Set RDY HIGH
	struct port_config rdyconfig;
	port_get_config_defaults(&rdyconfig);
	
	rdyconfig.direction = PORT_PIN_DIR_OUTPUT;
	port_group_set_config(&PORTA, 1 << PIN_KBD_RDY, &rdyconfig);

	port_group_set_output_level(&PORTA, 1 << PIN_KBD_RDY, 1 << PIN_KBD_RDY);
	
	// Get value of ADC
	g_adcReadDone = false;
	adc_read_buffer_job(&g_adcInstance, &g_adcResult, 1);
	while (!g_adcReadDone) {}
	
	// Value of 3.3V is USB mode, 1.65V is I2C
	g_isUsbOperationMode = g_adcResult > 0xF0;
	if (!g_isUsbOperationMode)
	{
		g_enableKeyboard = true;
	}
}

bool is_usb_operation_mode(void)
{
	return g_isUsbOperationMode;
}
