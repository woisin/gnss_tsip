#ifndef GRAND_GNSS_TSIP_H
#define GRAND_GNSS_TSIP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GNSS_TSIP_DLE 0x10u
#define GNSS_TSIP_ETX 0x03u

enum {
    GNSS_TSIP_OK = 0,
    GNSS_TSIP_ERR_ARG = -1,
    GNSS_TSIP_ERR_IO = -2,
    GNSS_TSIP_ERR_TIMEOUT = -3,
    GNSS_TSIP_ERR_PROTOCOL = -4,
    GNSS_TSIP_ERR_OVERFLOW = -5
};

enum {
    GNSS_RECEIVER_MODE_AUTO = 0,
    GNSS_RECEIVER_MODE_SINGLE_SAT = 1,
    GNSS_RECEIVER_MODE_HORIZONTAL = 3,
    GNSS_RECEIVER_MODE_FULL_POS = 4,
    GNSS_RECEIVER_MODE_OVERDETERMINED = 7,
    GNSS_RECEIVER_MODE_DO_NOT_ALTER = 0xFF
};

struct gnss_tsip_timing_status {
    uint8_t receiver_mode;
    uint8_t disciplining_mode;
    uint16_t critical_alarms;
    uint16_t minor_alarms;
    uint8_t decoding_status;
    uint8_t pps_indication;
    float pps_offset_ns;
    float clock_offset_ppb;
};

int gnss_tsip_open_serial(const char *device);
void gnss_tsip_close(int fd);

int gnss_tsip_send_packet(int fd, const uint8_t *payload, size_t payload_len);
int gnss_tsip_read_packet(int fd, uint8_t *payload, size_t payload_cap, size_t *payload_len,
                          int timeout_ms);

int gnss_tsip_send_accurate_position_lla(int fd, double latitude_deg, double longitude_deg,
                                         double altitude_m);
int gnss_tsip_set_receiver_mode(int fd, uint8_t receiver_mode);
int gnss_tsip_save_to_flash(int fd);
int gnss_tsip_request_primary_timing(int fd, uint8_t request_type);
int gnss_tsip_request_supplemental_timing(int fd, uint8_t request_type);
int gnss_tsip_wait_for_supplemental_timing(int fd, struct gnss_tsip_timing_status *status,
                                           int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
