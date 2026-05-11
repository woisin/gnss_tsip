#include "gnss_tsip.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static void gnss_tsip_store_be32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)((value >> 24) & 0xFFu);
    dst[1] = (uint8_t)((value >> 16) & 0xFFu);
    dst[2] = (uint8_t)((value >> 8) & 0xFFu);
    dst[3] = (uint8_t)(value & 0xFFu);
}

static void gnss_tsip_store_be_float(uint8_t *dst, float value)
{
    uint32_t raw = 0u;
    memcpy(&raw, &value, sizeof(raw));
    gnss_tsip_store_be32(dst, raw);
}

static void gnss_tsip_store_be_double(uint8_t *dst, double value)
{
    uint64_t raw = 0u;
    size_t i;

    memcpy(&raw, &value, sizeof(raw));
    for (i = 0; i < sizeof(raw); ++i) {
        dst[i] = (uint8_t)((raw >> ((sizeof(raw) - 1u - i) * 8u)) & 0xFFu);
    }
}

static uint16_t gnss_tsip_load_be16(const uint8_t *src)
{
    return (uint16_t)(((uint16_t)src[0] << 8) | (uint16_t)src[1]);
}

static float gnss_tsip_load_be_float(const uint8_t *src)
{
    uint32_t raw = ((uint32_t)src[0] << 24) |
                   ((uint32_t)src[1] << 16) |
                   ((uint32_t)src[2] << 8) |
                   (uint32_t)src[3];
    float value = 0.0f;
    memcpy(&value, &raw, sizeof(value));
    return value;
}

static int gnss_tsip_write_all(int fd, const uint8_t *buf, size_t len)
{
    size_t off = 0u;

    while (off < len) {
        ssize_t written = write(fd, buf + off, len - off);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return GNSS_TSIP_ERR_IO;
        }
        off += (size_t)written;
    }
    return GNSS_TSIP_OK;
}

int gnss_tsip_open_serial(const char *device)
{
    struct termios tio;
    int fd;

    if (device == NULL) {
        return GNSS_TSIP_ERR_ARG;
    }

    fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        return GNSS_TSIP_ERR_IO;
    }

    memset(&tio, 0, sizeof(tio));
    if (tcgetattr(fd, &tio) != 0) {
        close(fd);
        return GNSS_TSIP_ERR_IO;
    }

    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);

    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag |= PARENB;
    tio.c_cflag |= PARODD;
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~CRTSCTS;

    tio.c_iflag = IGNPAR;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        close(fd);
        return GNSS_TSIP_ERR_IO;
    }

    if (tcflush(fd, TCIOFLUSH) != 0) {
        close(fd);
        return GNSS_TSIP_ERR_IO;
    }

    return fd;
}

void gnss_tsip_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

int gnss_tsip_send_packet(int fd, const uint8_t *payload, size_t payload_len)
{
    uint8_t frame[256];
    size_t in_idx;
    size_t out_idx = 0u;

    if (fd < 0 || payload == NULL || payload_len == 0u) {
        return GNSS_TSIP_ERR_ARG;
    }

    frame[out_idx++] = GNSS_TSIP_DLE;
    for (in_idx = 0u; in_idx < payload_len; ++in_idx) {
        if (out_idx >= sizeof(frame) - 3u) {
            return GNSS_TSIP_ERR_OVERFLOW;
        }
        frame[out_idx++] = payload[in_idx];
        if (payload[in_idx] == GNSS_TSIP_DLE) {
            frame[out_idx++] = GNSS_TSIP_DLE;
        }
    }
    frame[out_idx++] = GNSS_TSIP_DLE;
    frame[out_idx++] = GNSS_TSIP_ETX;

    return gnss_tsip_write_all(fd, frame, out_idx);
}

int gnss_tsip_read_packet(int fd, uint8_t *payload, size_t payload_cap, size_t *payload_len,
                          int timeout_ms)
{
    struct pollfd pfd;
    bool in_frame = false;
    bool last_was_dle = false;
    size_t out_idx = 0u;

    if (fd < 0 || payload == NULL || payload_len == NULL || payload_cap == 0u) {
        return GNSS_TSIP_ERR_ARG;
    }

    *payload_len = 0u;
    pfd.fd = fd;
    pfd.events = POLLIN;

    for (;;) {
        uint8_t ch = 0u;
        int pr = poll(&pfd, 1, timeout_ms);
        if (pr == 0) {
            return GNSS_TSIP_ERR_TIMEOUT;
        }
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            return GNSS_TSIP_ERR_IO;
        }

        if (read(fd, &ch, 1) != 1) {
            return GNSS_TSIP_ERR_IO;
        }

        if (!in_frame) {
            if (ch == GNSS_TSIP_DLE) {
                in_frame = true;
                last_was_dle = true;
                out_idx = 0u;
            }
            continue;
        }

        if (last_was_dle) {
            if (out_idx == 0u && ch != GNSS_TSIP_ETX) {
                if (out_idx >= payload_cap) {
                    return GNSS_TSIP_ERR_OVERFLOW;
                }
                payload[out_idx++] = ch;
                last_was_dle = false;
                continue;
            }

            if (ch == GNSS_TSIP_DLE) {
                if (out_idx >= payload_cap) {
                    return GNSS_TSIP_ERR_OVERFLOW;
                }
                payload[out_idx++] = GNSS_TSIP_DLE;
                last_was_dle = false;
                continue;
            }

            if (ch == GNSS_TSIP_ETX) {
                *payload_len = out_idx;
                return GNSS_TSIP_OK;
            }

            if (out_idx >= payload_cap) {
                return GNSS_TSIP_ERR_OVERFLOW;
            }
            payload[out_idx++] = ch;
            last_was_dle = false;
            continue;
        }

        if (ch == GNSS_TSIP_DLE) {
            last_was_dle = true;
        } else {
            if (out_idx >= payload_cap) {
                return GNSS_TSIP_ERR_OVERFLOW;
            }
            payload[out_idx++] = ch;
        }
    }
}

int gnss_tsip_send_accurate_position_lla(int fd, double latitude_deg, double longitude_deg,
                                         double altitude_m)
{
    uint8_t packet[1 + 8 + 8 + 8];
    const double deg_to_rad = M_PI / 180.0;

    packet[0] = 0x32u;
    gnss_tsip_store_be_double(&packet[1], latitude_deg * deg_to_rad);
    gnss_tsip_store_be_double(&packet[9], longitude_deg * deg_to_rad);
    gnss_tsip_store_be_double(&packet[17], altitude_m);

    return gnss_tsip_send_packet(fd, packet, sizeof(packet));
}

int gnss_tsip_set_receiver_mode(int fd, uint8_t receiver_mode)
{
    uint8_t packet[41];

    memset(packet, 0xFF, sizeof(packet));
    packet[0] = 0xBBu;
    packet[1] = 0x00u;
    packet[2] = receiver_mode;

    /* The spec uses 1.0f as the "ignore" sentinel for floating-point fields in 0xBB. */
    gnss_tsip_store_be_float(&packet[6], 1.0f);
    gnss_tsip_store_be_float(&packet[10], 1.0f);
    gnss_tsip_store_be_float(&packet[14], 1.0f);
    gnss_tsip_store_be_float(&packet[18], 1.0f);

    return gnss_tsip_send_packet(fd, packet, sizeof(packet));
}

int gnss_tsip_save_to_flash(int fd)
{
    const uint8_t packet[] = {0x8Eu, 0x26u};
    return gnss_tsip_send_packet(fd, packet, sizeof(packet));
}

int gnss_tsip_request_primary_timing(int fd, uint8_t request_type)
{
    const uint8_t packet[] = {0x8Eu, 0xABu, request_type};
    return gnss_tsip_send_packet(fd, packet, sizeof(packet));
}

int gnss_tsip_request_supplemental_timing(int fd, uint8_t request_type)
{
    const uint8_t packet[] = {0x8Eu, 0xACu, request_type};
    return gnss_tsip_send_packet(fd, packet, sizeof(packet));
}

int gnss_tsip_wait_for_supplemental_timing(int fd, struct gnss_tsip_timing_status *status,
                                           int timeout_ms)
{
    uint8_t packet[128];
    size_t packet_len = 0u;
    int rc;

    if (status == NULL) {
        return GNSS_TSIP_ERR_ARG;
    }

    for (;;) {
        rc = gnss_tsip_read_packet(fd, packet, sizeof(packet), &packet_len, timeout_ms);
        if (rc != GNSS_TSIP_OK) {
            return rc;
        }
        if (packet_len >= 25u && packet[0] == 0x8Fu && packet[1] == 0xACu) {
            status->receiver_mode = packet[2];
            status->disciplining_mode = packet[3];
            status->critical_alarms = gnss_tsip_load_be16(&packet[9]);
            status->minor_alarms = gnss_tsip_load_be16(&packet[11]);
            status->decoding_status = packet[13];
            status->pps_indication = packet[15];
            status->pps_offset_ns = gnss_tsip_load_be_float(&packet[17]);
            status->clock_offset_ppb = gnss_tsip_load_be_float(&packet[21]);
            return GNSS_TSIP_OK;
        }
    }
}
