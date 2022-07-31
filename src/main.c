#include <asf.h>

#include "keyboard.h"

int main (void)
{
	system_init();
	
	delay_init();
	
	delay_ms(500);
	
	configure_pins();
	configure_adc();
	system_interrupt_enable_global();
	detect_operation_mode();
	disable_adc();
	
	if (is_usb_operation_mode())
	{
		configure_usb_hid();
	}
	else
	{
		configure_i2c_peripheral();
	}
	
	configure_tc();
	configure_extint();
	configure_dac();
	
	while (1)
	{
		// sleepmgr_enter_sleep();
	}	
}
