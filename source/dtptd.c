/*
    dtptd.c - main functionality for dtptd
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <libusb.h>
#include <libmtp.h>

#include "tun.h"
#include "picoppp.h"

#define VID_MICROSOFT  0x045E
#define PID_WP7_MAINOS 0x04EC

static struct libusb_context *ctx;
int init_usb()
{
    // init the libusb context
#if LIBUSB_API_VERSION >= 0x0100010A
    int r = libusb_init_context(&ctx, NULL, 0);
#else
    int r = libusb_init(&ctx);
#endif
    return r;
}

void close_usb()
{
    // close the usb library
    libusb_exit(ctx);
}

int libusb_ppp_output_func(void *arg, const uint8_t *buf, size_t len)
{
    return libusb_bulk_transfer((libusb_device_handle *)arg, 0x05, (uint8_t *)buf, len, NULL, 500);
}

int main(int argc, char **argv)
{
    ppp_state_t *state = NULL;
    LIBMTP_mtpdevice_t *mtp_dev = NULL;
    libusb_device *found_device = NULL;
    libusb_device_handle *dev = NULL;

    printf("dtptd init\n");
    init_usb();

    if (tun_start_interface() >= 0) {
        printf("started network interface on '%s'\n", tun_get_ifname());
    } else {
        printf("FAILED TO START NETWORK INTERFACE!!\n");
        printf("bad things will probably happen!\n");
    }

    printf("searching for devices...\n");
    // scan the list of devices
    libusb_device **device_list = NULL;
    ssize_t sz = libusb_get_device_list(ctx, &device_list);
    for (ssize_t i = 0; i < sz; i++) {
        // fetch the descriptor to see if it's a device in DLOAD mode
        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor(device_list[i], &desc);

        if (desc.idVendor == VID_MICROSOFT && desc.idProduct == PID_WP7_MAINOS) {
            found_device = device_list[i];
            printf("found WP7 USB device! ");
            int r = libusb_open(found_device, &dev);
            if (r != 0) {
                printf("failed to open device (%i)\n", r);
                found_device = NULL;
                dev = NULL;
            } else {
                uint8_t strdesc[256];
                if (libusb_get_string_descriptor_ascii(dev, desc.iProduct, strdesc, sizeof(strdesc)))
                    printf("%s", strdesc);
                printf("\n");
                break;
            }
        }
    }
    libusb_free_device_list(device_list, true);
    if (found_device == NULL || dev == NULL) {
        printf("couldn't find a WP7 device.\n");
        goto quit;
    }
    // reset the device so we're in a clean(?) state
    int r = libusb_reset_device(dev);
    if (r != 0) {
        printf("failed to reset device (%i)\n", r);
    }
    // start an MTP session, necessary to get it to listen for USB PPP
    bool do_mtp = true; // TODO(Emma): make this optional
    if (do_mtp) {
        LIBMTP_Init();
        LIBMTP_raw_device_t *mtp_list;
        int returned = 0;
        LIBMTP_Detect_Raw_Devices(&mtp_list, &returned);
        for (int i = 0; i < returned; i++) {
            // make sure it's the exact same device as the libusb one we got earlier
            if (mtp_list[i].bus_location == libusb_get_bus_number(found_device) &&
                mtp_list[i].devnum == libusb_get_device_address(found_device)) {
                printf("connecting to MTP device...");
                mtp_dev = LIBMTP_Open_Raw_Device(&mtp_list[i]);
                break;
            }
        }
        if (mtp_dev == NULL) {
            printf("failed to open MTP device! probably already open...\n");
        } else {
            char *model_name = LIBMTP_Get_Modelname(mtp_dev);
            if (model_name != NULL) {
                printf("%s\n", model_name);
                free(model_name);
            }
        }
    }

    // claim the interface used for the DTPT networking
    r = libusb_claim_interface(dev, 1);
    if (r != 0) {
        printf("failed to claim interface 1 (%i)\n", r);
        goto quit;
    }
    printf("telling device to init connection...\n");
    libusb_control_transfer(dev, 0x21, 0x22, 0x0001, 1, NULL, 0, 500);

    // setting up our PPP state
    state = ppp_alloc_state();
    if (state == NULL) {
        printf("failed to allocate state\n");
        goto quit;
    }
    ppp_init_state(state, libusb_ppp_output_func, (void *)dev, NULL);

    while (true) {
        uint8_t tbuf[2048]; // the PPP MTU seems to be around ~1560
        int tsize;
        r = libusb_bulk_transfer(dev, 0x84, tbuf, sizeof(tbuf), &tsize, 30);
        if (r != 0 && r != LIBUSB_ERROR_TIMEOUT) {
            printf("transfer failed (%i)\n", r);
            break;
        } else if (r == 0) {
            if (tsize == 6 && memcmp(tbuf, "CLIENT", 6) == 0) {
                printf("initiating new connection\n");
                char sendbuf[] = "CLIENTSERVER";
                libusb_bulk_transfer(dev, 0x05, (uint8_t *)sendbuf, 12, NULL, 0);
            } else {
                ppp_handle_incoming(state, tbuf, tsize);
            }
        }
    }

quit:
    printf("shutting down...\n");
    tun_close_interface();
    if (state != NULL) {
        free(state);
    }
    if (do_mtp && mtp_dev != NULL) {
        LIBMTP_Release_Device(mtp_dev);
    }
    close_usb();
    return 0;
}
