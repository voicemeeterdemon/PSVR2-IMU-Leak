#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <libusb.h>

#define HMD2_USB_VID 0x054C
#define HMD2_USB_PID 0x0CDE

// Initializes LibUSB, optionally using UsbDK on Windows.
libusb_context* hmd2_libusb_init(int use_usbdk);

// Opens HMD2 device.
libusb_device_handle* hmd2_libusb_open(libusb_context* ctx);

// Gets HID report for HMD2, you must free the transferred data if it is not null. The transferred length will be returned through the parameter.
unsigned char* hmd2_libusb_hid_get_report(libusb_device_handle* dev_handle, uint16_t iface, uint8_t report_id, uint8_t sub_id, uint16_t* length);

// Sets HID report for HMD2, returns transferred length.
uint16_t hmd2_libusb_hid_set_report(libusb_device_handle* dev_handle, uint16_t iface, uint8_t report_id, uint8_t sub_id, unsigned char* data, uint16_t length);
