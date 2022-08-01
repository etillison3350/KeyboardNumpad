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
#include "udi_hid_multimedia.h"
#include <string.h>

bool udi_hid_multimedia_enable(void);
void udi_hid_multimedia_disable(void);
bool udi_hid_multimedia_setup(void);
uint8_t udi_hid_multimedia_getsetting(void);

//! Global structure which contains standard UDI interface for UDC
UDC_DESC_STORAGE udi_api_t udi_api_hid_multimedia = {
	.enable = (bool(*)(void))udi_hid_multimedia_enable,
	.disable = (void (*)(void))udi_hid_multimedia_disable,
	.setup = (bool(*)(void))udi_hid_multimedia_setup,
	.getsetting = (uint8_t(*)(void))udi_hid_multimedia_getsetting,
	.sof_notify = NULL,
};


// Changes keyboard report states (like LEDs) (?)
static bool udi_hid_multimedia_setreport(void);
// Send the report
static bool udi_hid_multimedia_send_report(void);
// Callback called when the report is sent
static void udi_hid_multimedia_report_sent(udd_ep_status_t status, iram_size_t nb_sent, udd_ep_id_t ep);


//! Size of report for standard HID keyboard
#define UDI_HID_MULTIMEDIA_REPORT_SIZE  1


//! To store current rate of HID keyboard
COMPILER_WORD_ALIGNED
		static uint8_t udi_hid_multimedia_rate;
//! To store current protocol of HID keyboard
COMPILER_WORD_ALIGNED
		static uint8_t udi_hid_multimedia_protocol;
//! To signal if a valid report is ready to send
static bool udi_hid_multimedia_b_report_valid;
//! Report ready to send
static uint8_t udi_hid_multimedia_report[UDI_HID_MULTIMEDIA_REPORT_SIZE];
//! Signal if a report transfer is on going
static bool udi_hid_multimedia_b_report_trans_ongoing;
//! Buffer used to send report
COMPILER_WORD_ALIGNED
		static uint8_t
		udi_hid_multimedia_report_trans[UDI_HID_MULTIMEDIA_REPORT_SIZE];

//! HID report descriptor for standard HID keyboard
UDC_DESC_STORAGE udi_hid_multimedia_report_desc_t udi_hid_multimedia_report_desc = {
	{
		0x05, 0x0c,                    // USAGE_PAGE (Consumer Devices)
		0x09, 0x01,                    // USAGE (Consumer Control)
		0xa1, 0x01,                    // COLLECTION (Application)
		0x05, 0x0c,                    //   USAGE_PAGE (Consumer Devices)
		0x09, 0xb6,                    //   USAGE (Scan Previous Track)
		0x09, 0xb5,                    //   USAGE (Scan Next Track)
		0x09, 0xcd,                    //   USAGE (Play/Pause)
		0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
		0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
		0x75, 0x01,                    //   REPORT_SIZE (1)
		0x95, 0x03,                    //   REPORT_COUNT (3)
		0x81, 0x06,                    //   INPUT (Data,Var,Rel)
		0x75, 0x01,                    //   REPORT_SIZE (1)
		0x95, 0x01,                    //   REPORT_COUNT (1)
		0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
		0x09, 0xe2,                    //   USAGE (Mute)
		0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
		0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
		0x75, 0x01,                    //   REPORT_SIZE (1)
		0x95, 0x01,                    //   REPORT_COUNT (1)
		0x81, 0x06,                    //   INPUT (Data,Var,Rel)
		0x09, 0xe9,                    //   USAGE (Volume Up)
		0x09, 0xea,                    //   USAGE (Volume Down)
		0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
		0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
		0x75, 0x01,                    //   REPORT_SIZE (1)
		0x95, 0x02,                    //   REPORT_COUNT (2)
		0x81, 0x02,                    //   INPUT (Data,Var,Abs)
		0x75, 0x01,                    //   REPORT_SIZE (1)
		0x95, 0x01,                    //   REPORT_COUNT (1)
		0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
		0xc0                           // END_COLLECTION
    }
};

// Interface for UDI HID level (UDI API Functions)

bool udi_hid_multimedia_enable(void)
{
	// Initialize internal values
	udi_hid_multimedia_rate = 0;
	udi_hid_multimedia_protocol = 0;
	udi_hid_multimedia_b_report_trans_ongoing = false;
	memset(udi_hid_multimedia_report, 0, UDI_HID_MULTIMEDIA_REPORT_SIZE);
	udi_hid_multimedia_b_report_valid = false;

	return UDI_HID_MULTIMEDIA_ENABLE_EXT();
}

void udi_hid_multimedia_disable(void)
{
	UDI_HID_MULTIMEDIA_DISABLE_EXT();
}

bool udi_hid_multimedia_setup(void)
{
	return udi_hid_setup(&udi_hid_multimedia_rate, &udi_hid_multimedia_protocol, (uint8_t *) &udi_hid_multimedia_report_desc, udi_hid_multimedia_setreport);
}

uint8_t udi_hid_multimedia_getsetting(void)
{
	return 0;
}


static bool udi_hid_multimedia_setreport(void)
{
	return false;
}


bool udi_hid_multimedia_send_event(uint8_t media_id)
{
    irqflags_t flags = cpu_irq_save();

	if (udi_hid_multimedia_report[0] != media_id)
	{
		// Fill report
		udi_hid_multimedia_report[0] = media_id;
		udi_hid_multimedia_b_report_valid = true;
		
		// Send report
		udi_hid_multimedia_send_report();
	}

	cpu_irq_restore(flags);
	return true;
}

// Internal routines

static bool udi_hid_multimedia_send_report(void)
{
	if (udi_hid_multimedia_b_report_trans_ongoing)
		return false;

	memcpy(udi_hid_multimedia_report_trans, udi_hid_multimedia_report, UDI_HID_MULTIMEDIA_REPORT_SIZE);
	udi_hid_multimedia_b_report_valid = false;
	udi_hid_multimedia_b_report_trans_ongoing = udd_ep_run(UDI_HID_MULTIMEDIA_EP_IN, false, udi_hid_multimedia_report_trans,
            UDI_HID_MULTIMEDIA_REPORT_SIZE, udi_hid_multimedia_report_sent);
	return udi_hid_multimedia_b_report_trans_ongoing;
}

static void udi_hid_multimedia_report_sent(udd_ep_status_t status, iram_size_t nb_sent, udd_ep_id_t ep)
{
	UNUSED(status);
	UNUSED(nb_sent);
	UNUSED(ep);

	udi_hid_multimedia_b_report_trans_ongoing = false;
	if (udi_hid_multimedia_b_report_valid) {
		udi_hid_multimedia_send_report();
	}
}
