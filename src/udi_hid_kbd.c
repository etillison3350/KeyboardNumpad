/**
 * Copyright (c) 2009-2018 Microchip Technology Inc. and its subsidiaries.
 * 
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 */

#include "conf_usb.h"
#include "usb_protocol.h"
#include "udd.h"
#include "udc.h"
#include "udi_hid.h"
#include "udi_hid_kbd.h"
#include <string.h>

bool udi_hid_kbd_enable(void);
void udi_hid_kbd_disable(void);
bool udi_hid_kbd_setup(void);
uint8_t udi_hid_kbd_getsetting(void);

//! Global structure which contains standard UDI interface for UDC
UDC_DESC_STORAGE udi_api_t udi_api_hid_kbd = {
	.enable = (bool(*)(void))udi_hid_kbd_enable,
	.disable = (void (*)(void))udi_hid_kbd_disable,
	.setup = (bool(*)(void))udi_hid_kbd_setup,
	.getsetting = (uint8_t(*)(void))udi_hid_kbd_getsetting,
	.sof_notify = NULL,
};


// Changes keyboard report states (like LEDs) (?)
static bool udi_hid_kbd_setreport(void);
// Send the report
static bool udi_hid_kbd_send_report(void);
// Callback called when the report is sent
static void udi_hid_kbd_report_sent(udd_ep_status_t status, iram_size_t nb_sent, udd_ep_id_t ep);
// Callback called to update report from USB host
static void udi_hid_kbd_setreport_valid(void);


//! Size of report for standard HID keyboard
#define UDI_HID_KBD_REPORT_SIZE  8


//! To store current rate of HID keyboard
COMPILER_WORD_ALIGNED
		static uint8_t udi_hid_kbd_rate;
//! To store current protocol of HID keyboard
COMPILER_WORD_ALIGNED
		static uint8_t udi_hid_kbd_protocol;
//! To store report feedback from USB host
COMPILER_WORD_ALIGNED
		static uint8_t udi_hid_kbd_report_set;
//! To signal if a valid report is ready to send
static bool udi_hid_kbd_b_report_valid;
//! Report ready to send
static uint8_t udi_hid_kbd_report[UDI_HID_KBD_REPORT_SIZE];
//! Signal if a report transfer is on going
static bool udi_hid_kbd_b_report_trans_ongoing;
//! Buffer used to send report
COMPILER_WORD_ALIGNED
		static uint8_t
		udi_hid_kbd_report_trans[UDI_HID_KBD_REPORT_SIZE];

//! HID report descriptor for standard HID keyboard
UDC_DESC_STORAGE udi_hid_kbd_report_desc_t udi_hid_kbd_report_desc = {
	{
        0x05, 0x01,	/* Usage Page (Generic Desktop)      */
        0x09, 0x06,	/* Usage (Keyboard)                  */
        0xA1, 0x01,	/* Collection (Application)          */
        0x05, 0x07,	/* Usage Page (Keyboard)             */
        0x19, 224,	/* Usage Minimum (224)               */
        0x29, 231,	/* Usage Maximum (231)               */
        0x15, 0x00,	/* Logical Minimum (0)               */
        0x25, 0x01,	/* Logical Maximum (1)               */
        0x75, 0x01,	/* Report Size (1)                   */
        0x95, 0x08,	/* Report Count (8)                  */
        0x81, 0x02,	/* Input (Data, Variable, Absolute)  */
        0x81, 0x01,	/* Input (Constant)                  */
        0x19, 0x00,	/* Usage Minimum (0)                 */
        0x29, 101,	/* Usage Maximum (101)               */
        0x15, 0x00,	/* Logical Minimum (0)               */
        0x25, 101,	/* Logical Maximum (101)             */
        0x75, 0x08,	/* Report Size (8)                   */
        0x95, 0x06,	/* Report Count (6)                  */
        0x81, 0x00,	/* Input (Data, Array)               */
        0x05, 0x08,	/* Usage Page (LED)                  */
        0x19, 0x01,	/* Usage Minimum (1)                 */
        0x29, 0x05,	/* Usage Maximum (5)                 */
        0x15, 0x00,	/* Logical Minimum (0)               */
        0x25, 0x01,	/* Logical Maximum (1)               */
        0x75, 0x01,	/* Report Size (1)                   */
        0x95, 0x05,	/* Report Count (5)                  */
        0x91, 0x02,	/* Output (Data, Variable, Absolute) */
        0x95, 0x03,	/* Report Count (3)                  */
        0x91, 0x01,	/* Output (Constant)                 */
        0xC0	/* End Collection                    */
    }
};

// Interface for UDI HID level (UDI API Functions)

bool udi_hid_kbd_enable(void)
{
	// Initialize internal values
	udi_hid_kbd_rate = 0;
	udi_hid_kbd_protocol = 0;
	udi_hid_kbd_b_report_trans_ongoing = false;
	memset(udi_hid_kbd_report, 0, UDI_HID_KBD_REPORT_SIZE);
	udi_hid_kbd_b_report_valid = false;

	return UDI_HID_KBD_ENABLE_EXT();
}

void udi_hid_kbd_disable(void)
{
	UDI_HID_KBD_DISABLE_EXT();
}

bool udi_hid_kbd_setup(void)
{
	return udi_hid_setup(&udi_hid_kbd_rate, &udi_hid_kbd_protocol, (uint8_t *) &udi_hid_kbd_report_desc, udi_hid_kbd_setreport);
}

uint8_t udi_hid_kbd_getsetting(void)
{
	return 0;
}


static bool udi_hid_kbd_setreport(void)
{
	if ((USB_HID_REPORT_TYPE_OUTPUT == (udd_g_ctrlreq.req.wValue >> 8)) && (0 == (0xFF & udd_g_ctrlreq.req.wValue)) && (1 == udd_g_ctrlreq.req.wLength)) {
		// Report OUT type on report ID 0 from USB Host
		udd_g_ctrlreq.payload = &udi_hid_kbd_report_set;
		udd_g_ctrlreq.callback = udi_hid_kbd_setreport_valid;
		udd_g_ctrlreq.payload_size = 1;
		return true;
	}
	return false;
}


bool udi_hid_kbd_send_event(uint8_t modifier_id, uint8_t *pressed_keycodes)
{
    irqflags_t flags = cpu_irq_save();

	if (udi_hid_kbd_report[0] != modifier_id || memcmp(&udi_hid_kbd_report[2], pressed_keycodes, UDI_HID_KBD_REPORT_SIZE - 2) != 0)
	{
		// Fill report
		udi_hid_kbd_report[0] = modifier_id;
		memcpy(&udi_hid_kbd_report[2], pressed_keycodes, UDI_HID_KBD_REPORT_SIZE - 2);
		udi_hid_kbd_b_report_valid = true;

		// Send report
		udi_hid_kbd_send_report();
	}

	cpu_irq_restore(flags);
	return true;
}

// Internal routines

static bool udi_hid_kbd_send_report(void)
{
	if (udi_hid_kbd_b_report_trans_ongoing)
		return false;

	memcpy(udi_hid_kbd_report_trans, udi_hid_kbd_report, UDI_HID_KBD_REPORT_SIZE);
	udi_hid_kbd_b_report_valid = false;
	udi_hid_kbd_b_report_trans_ongoing = udd_ep_run(UDI_HID_KBD_EP_IN, false, udi_hid_kbd_report_trans, UDI_HID_KBD_REPORT_SIZE, udi_hid_kbd_report_sent);
	return udi_hid_kbd_b_report_trans_ongoing;
}

static void udi_hid_kbd_report_sent(udd_ep_status_t status, iram_size_t nb_sent, udd_ep_id_t ep)
{
	UNUSED(status);
	UNUSED(nb_sent);
	UNUSED(ep);

	udi_hid_kbd_b_report_trans_ongoing = false;
	if (udi_hid_kbd_b_report_valid) {
		udi_hid_kbd_send_report();
	}
}

static void udi_hid_kbd_setreport_valid(void)
{
	UDI_HID_KBD_CHANGE_LED(udi_hid_kbd_report_set);
}