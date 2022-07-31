#ifndef KEYBOARD_H_
#define KEYBOARD_H_

#include <asf.h>

#include "udi_hid_kbd.h"
#include "udi_hid_multimedia.h"

#define KEY_LAYER_DEFAULT    0x0000
#define KEY_LAYER_MULTIMEDIA 0xC000
#define KEY_LAYER_META       0xF000

#define NUM_ROWS 6
#define NUM_COLS 4

// Convience definitions
#define K_PREV KEY_LAYER_MULTIMEDIA | HID_MULTIMEDIA_KEY_SCAN_PREVIOUS
#define K_PLAY KEY_LAYER_MULTIMEDIA | HID_MULTIMEDIA_KEY_PLAY_PAUSE
#define K_NEXT KEY_LAYER_MULTIMEDIA | HID_MULTIMEDIA_KEY_SCAN_NEXT
#define K_MUTE KEY_LAYER_MULTIMEDIA | HID_MULTIMEDIA_KEY_MUTE

static const uint16_t KEYMAP[NUM_ROWS][NUM_COLS] = {
	{ K_PREV, K_PLAY, K_NEXT, K_MUTE },
	{   0x53,   0x54,   0x55,   0x56 },
	{   0x5f,   0x60,   0x61,   0x57 },
	{   0x5c,   0x5d,   0x5e,      0 },
	{   0x59,   0x5a,   0x5b,   0x58 },
	{   0x62,      0,   0x63,      0 }
};

#undef K_PREV
#undef K_PLAY
#undef K_NEXT
#undef K_MUTE

static const uint8_t ROWMAP[NUM_ROWS] = { 0, 18, 22, 15, 14, 11 };
static const uint8_t COLMAP[NUM_COLS] = { 19, 6, 7, 10 };
	
#define PIN_KBD_LED 2
#define PIN_KBD_LED_CHAN DAC_CHANNEL_0

static const uint16_t ENCMAP[2] = { KEY_LAYER_MULTIMEDIA | HID_MULTIMEDIA_KEY_VOLUME_DOWN, KEY_LAYER_MULTIMEDIA | HID_MULTIMEDIA_KEY_VOLUME_UP };

#define PIN_KBD_ENC_A    4
#define EXTINT_KBD_ENC_A PIN_PA04A_EIC_EXTINT4
#define MUX_KBD_ENC_A    PINMUX_PA04A_EIC_EXTINT4
#define CHAN_KBD_ENC_A   4
#define PIN_KBD_ENC_B    3

#define PIN_KBD_ID 8
#define PIN_KBD_ID_CHAN ADC_POSITIVE_INPUT_PIN16

#define PIN_KBD_RDY 9

#define KBD_I2C_SERCOM_IFACE SERCOM3
#define KBD_I2C_PERIPHERAL_ADDR 0x74


void configure_pins(void);
void configure_adc(void);
void configure_tc(void);
void configure_usb_hid(void);
void configure_extint(void);
void configure_i2c_peripheral(void);
void configure_dac(void);

void disable_adc(void);

void keyboard_scan_tc_callback(struct tc_module *const module);
void rot_enc_extint_callback(void);
void i2c_read_request_callback(struct i2c_slave_module *const module);
// void i2c_write_complete_callback(struct i2c_slave_module *const module);
// bool usb_keyboard_enable_callback(void);
// void usb_keyboard_disable_callback(void);
// void usb_keyboard_led_callback(uint8_t value);
void adc_complete_callback(struct adc_module *const module);

void detect_operation_mode(void);
bool is_usb_operation_mode(void);

#endif /* KEYBOARD_H_ */