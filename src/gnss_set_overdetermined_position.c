#include "gnss_tsip.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <tty> <lat_deg> <lon_deg> <alt_m> [--force-mode] [--save]\n"
            "       %s <tty> --query-only\n"
            "       %s --help\n"
            "\n"
            "Example:\n"
            "  %s /dev/ttyPS1 45.123456 -1.234567 1324.5 --force-mode --save\n"
            "  %s /dev/ttyPS1 --query-only\n",
            prog, prog, prog, prog, prog);
}

static const char *receiver_mode_name(uint8_t mode)
{
    switch (mode) {
    case GNSS_RECEIVER_MODE_AUTO:
        return "automatic";
    case GNSS_RECEIVER_MODE_SINGLE_SAT:
        return "single-satellite";
    case GNSS_RECEIVER_MODE_HORIZONTAL:
        return "2D";
    case GNSS_RECEIVER_MODE_FULL_POS:
        return "3D";
    case GNSS_RECEIVER_MODE_OVERDETERMINED:
        return "overdetermined-clock";
    default:
        return "unknown";
    }
}

int main(int argc, char **argv)
{
    const char *tty = NULL;
    double lat_deg = 0.0;
    double lon_deg = 0.0;
    double alt_m = 0.0;
    bool force_mode = false;
    bool save = false;
    bool query_only = false;
    int fd;
    int rc;
    int argi;
    struct gnss_tsip_timing_status status;

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        usage(argv[0]);
        return 0;
    }

    if (argc == 3 && strcmp(argv[2], "--query-only") == 0) {
        tty = argv[1];
        query_only = true;
    } else if (argc >= 5) {
        tty = argv[1];
        lat_deg = strtod(argv[2], NULL);
        lon_deg = strtod(argv[3], NULL);
        alt_m = strtod(argv[4], NULL);
    } else {
        usage(argv[0]);
        return 1;
    }

    for (argi = query_only ? 3 : 5; argi < argc; ++argi) {
        if (strcmp(argv[argi], "--force-mode") == 0) {
            force_mode = true;
        } else if (strcmp(argv[argi], "--save") == 0) {
            save = true;
        } else if (strcmp(argv[argi], "--query-only") == 0) {
            query_only = true;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    fd = gnss_tsip_open_serial(tty);
    if (fd < 0) {
        fprintf(stderr, "Failed to open GNSS serial device %s\n", tty);
        return 2;
    }

    if (!query_only) {
        /*
         * The receiver may ignore TSIP commands during roughly the first 10 seconds after power-up.
         * Sleeping here keeps the example safe for early boot execution on the DU.
         */
        sleep(11);

        rc = gnss_tsip_send_accurate_position_lla(fd, lat_deg, lon_deg, alt_m);
        if (rc != GNSS_TSIP_OK) {
            fprintf(stderr, "Failed to send TSIP 0x32 accurate position packet\n");
            gnss_tsip_close(fd);
            return 3;
        }

        if (force_mode) {
            rc = gnss_tsip_set_receiver_mode(fd, GNSS_RECEIVER_MODE_OVERDETERMINED);
            if (rc != GNSS_TSIP_OK) {
                fprintf(stderr, "Failed to send TSIP 0xBB receiver-mode packet\n");
                gnss_tsip_close(fd);
                return 4;
            }
        }
    }

    rc = gnss_tsip_request_supplemental_timing(fd, 0);
    if (rc != GNSS_TSIP_OK) {
        fprintf(stderr, "Failed to request TSIP 0x8F-AC supplemental timing packet\n");
        gnss_tsip_close(fd);
        return 5;
    }

    rc = gnss_tsip_wait_for_supplemental_timing(fd, &status, 2000);
    if (rc != GNSS_TSIP_OK) {
        fprintf(stderr, "Timed out waiting for TSIP 0x8F-AC confirmation packet\n");
        gnss_tsip_close(fd);
        return 6;
    }

    printf("Receiver mode: %s (%u)\n", receiver_mode_name(status.receiver_mode),
           (unsigned)status.receiver_mode);
    printf("Decoding status: 0x%02X\n", (unsigned)status.decoding_status);
    printf("Critical alarms: 0x%04X\n", (unsigned)status.critical_alarms);
    printf("Minor alarms: 0x%04X\n", (unsigned)status.minor_alarms);
    printf("PPS indication: %u\n", (unsigned)status.pps_indication);
    printf("PPS offset: %.3f ns\n", status.pps_offset_ns);
    printf("Clock offset: %.3f ppb\n", status.clock_offset_ppb);

    if (save) {
        rc = gnss_tsip_save_to_flash(fd);
        if (rc != GNSS_TSIP_OK) {
            fprintf(stderr, "Failed to send TSIP 0x8E-26 save-to-flash command\n");
            gnss_tsip_close(fd);
            return 7;
        }
        printf("Configuration save command sent.\n");
    }

    gnss_tsip_close(fd);
    return 0;
}
