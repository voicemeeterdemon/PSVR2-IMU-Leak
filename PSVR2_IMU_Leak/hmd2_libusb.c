#include "hmd2_libusb.h"
#include "libusb/libusb.h"

#define HID_REQ_GET_REPORT 0x01
#define HID_REQ_SET_REPORT 0x09

libusb_context* hmd2_libusb_init(int use_usbdk) {
  libusb_context* ctx;
  if (libusb_init(&ctx) < 0) {
    return NULL;
  }

#ifdef _WIN32
  // This allows us to do larger control transfers, bypassing normal Windows USB driver limits.
  /*if (use_usbdk) {
    libusb_set_option(ctx, LIBUSB_OPTION_USE_USBDK);
  }*/
#endif

  return ctx;
}

libusb_device_handle* hmd2_libusb_open(libusb_context* ctx) {
  libusb_device_handle* dev_handle = libusb_open_device_with_vid_pid(ctx, HMD2_USB_VID, HMD2_USB_PID);
  if (!dev_handle) {
    return NULL;
  }

  libusb_device* dev = libusb_get_device(dev_handle);

  struct libusb_device_descriptor dev_desc;
  if (libusb_get_device_descriptor(dev, &dev_desc) != 0) {
    libusb_close(dev_handle);
    return NULL;
  }
  if (dev_desc.bNumConfigurations > 0) {
    if (libusb_set_configuration(dev_handle, 1) != 0) {
      libusb_close(dev_handle);
      return NULL;
    }
  }

  struct libusb_config_descriptor* config_desc;
  if (libusb_get_active_config_descriptor(dev, &config_desc) != 0) {
    libusb_close(dev_handle);
    return NULL;
  }

  for (int i = 0; i < config_desc->bNumInterfaces; i++) {
    const struct libusb_interface* iface = &config_desc->interface[i];
    int iface_num = iface->altsetting[0].bInterfaceNumber;

#ifndef _WIN32
    // Linux and macOS need this.
    if (libusb_kernel_driver_active(dev_handle, iface_num) == 1) {
      if (libusb_detach_kernel_driver(dev_handle, iface_num) != 0) {
        libusb_free_config_descriptor(config_desc);
        libusb_close(dev_handle);
        return NULL;
      }
    }
#endif

    // Not required for WinUSB?
    /*if (libusb_claim_interface(dev_handle, iface_num) != 0) {
      libusb_free_config_descriptor(config_desc);
      libusb_close(dev_handle);
      return NULL;
    }*/
  }

  return dev_handle;
}

unsigned char* hmd2_libusb_hid_get_report(libusb_device_handle* dev_handle, uint16_t iface, uint8_t report_id, uint8_t sub_id, uint16_t* length) {
  if (!dev_handle && !length) {
    if (length) {
      *length = 0;
    }
    return NULL;
  }

  uint8_t bmRequestType = (uint8_t)LIBUSB_ENDPOINT_IN | (uint8_t)LIBUSB_REQUEST_TYPE_CLASS | (uint8_t)LIBUSB_RECIPIENT_INTERFACE;
  uint16_t wValue = (sub_id << 8) | report_id;
  unsigned char* data = (unsigned char*)malloc(*length);
  int result = libusb_control_transfer(dev_handle, bmRequestType, HID_REQ_GET_REPORT, wValue, iface, data, *length, 0);
  if (result < 0) {
    free(data);
    *length = 0;
    return NULL;
  }

  *length = (uint16_t)result;
  return data;
}

uint16_t hmd2_libusb_hid_set_report(libusb_device_handle* dev_handle, uint16_t iface, uint8_t report_id, uint8_t sub_id, unsigned char* data, uint16_t length) {
  if (!dev_handle && !data) {
    return 0;
  }

  uint8_t bmRequestType = (uint8_t)LIBUSB_ENDPOINT_OUT | (uint8_t)LIBUSB_REQUEST_TYPE_CLASS | (uint8_t)LIBUSB_RECIPIENT_INTERFACE;
  uint16_t wValue = (sub_id << 8) | report_id;
  int result = libusb_control_transfer(dev_handle, bmRequestType, HID_REQ_SET_REPORT, wValue, iface, data, length, 0);
  if (result < 0) {
    return 0;
  }

  return result;
}
