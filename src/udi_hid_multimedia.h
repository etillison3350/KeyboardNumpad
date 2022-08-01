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

#ifndef UDI_HID_MULTIMEDIA_H_
#define UDI_HID_MULTIMEDIA_H_

#include "udc_desc.h"
#include "udi_hid.h"

#define HID_MULTIMEDIA_KEY_SCAN_PREVIOUS 0x01
#define HID_MULTIMEDIA_KEY_SCAN_NEXT     0x02
#define HID_MULTIMEDIA_KEY_PLAY_PAUSE    0x04
#define HID_MULTIMEDIA_KEY_MUTE          0x10
#define HID_MULTIMEDIA_KEY_VOLUME_UP     0x20
#define HID_MULTIMEDIA_KEY_VOLUME_DOWN   0x40

extern UDC_DESC_STORAGE udi_api_t udi_api_hid_multimedia;

//! Interface descriptor structure for HID keyboard
typedef struct {
	usb_iface_desc_t iface;
	usb_hid_descriptor_t hid;
	usb_ep_desc_t ep;
} udi_hid_multimedia_desc_t;


//! Report descriptor for HID keyboard
typedef struct {
	uint8_t array[63];
} udi_hid_multimedia_report_desc_t;


//! By default no string associated to this interface
#ifndef UDI_HID_MULTIMEDIA_STRING_ID
#define UDI_HID_MULTIMEDIA_STRING_ID 0
#endif

#define UDI_HID_MULTIMEDIA_IFACE_NUMBER 0

//! HID keyboard endpoints size
#define UDI_HID_MULTIMEDIA_EP_SIZE  8

#define UDI_HID_MULTIMEDIA_EP_IN    (1 | USB_EP_DIR_IN)

//! Content of HID keyboard interface descriptor for all speed
#define UDI_HID_MULTIMEDIA_DESC    {\
	.iface.bLength             = sizeof(usb_iface_desc_t),\
	.iface.bDescriptorType     = USB_DT_INTERFACE,\
	.iface.bInterfaceNumber    = UDI_HID_MULTIMEDIA_IFACE_NUMBER,\
	.iface.bAlternateSetting   = 0,\
	.iface.bNumEndpoints       = 1,\
	.iface.bInterfaceClass     = HID_CLASS,\
	.iface.bInterfaceSubClass  = HID_SUB_CLASS_NOBOOT,\
	.iface.bInterfaceProtocol  = HID_PROTOCOL_GENERIC,\
	.iface.iInterface          = UDI_HID_MULTIMEDIA_STRING_ID,\
	.hid.bLength               = sizeof(usb_hid_descriptor_t),\
	.hid.bDescriptorType       = USB_DT_HID,\
	.hid.bcdHID                = LE16(USB_HID_BDC_V1_11),\
	.hid.bCountryCode          = USB_HID_NO_COUNTRY_CODE,\
	.hid.bNumDescriptors       = USB_HID_NUM_DESC,\
	.hid.bRDescriptorType      = USB_DT_HID_REPORT,\
	.hid.wDescriptorLength     = LE16(sizeof(udi_hid_multimedia_report_desc_t)),\
	.ep.bLength                = sizeof(usb_ep_desc_t),\
	.ep.bDescriptorType        = USB_DT_ENDPOINT,\
	.ep.bEndpointAddress       = UDI_HID_MULTIMEDIA_EP_IN,\
	.ep.bmAttributes           = USB_EP_TYPE_INTERRUPT,\
	.ep.wMaxPacketSize         = LE16(UDI_HID_MULTIMEDIA_EP_SIZE),\
	.ep.bInterval              = 2,\
}


bool udi_hid_multimedia_send_event(uint8_t media_id);

#endif /* UDI_HID_MULTIMEDIA_H_ */