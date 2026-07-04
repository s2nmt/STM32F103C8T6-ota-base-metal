/*
 * ota.h
 *
 *  Created on: Jun 28, 2026
 *      Author: Minh Tuan
 */

#ifndef OTA_H_
#define OTA_H_

#include <stdint.h>

#define OTA_FRAME_STX           0x02U
#define OTA_FRAME_ETX           0x03U

#define OTA_CMD_START           0x01U
#define OTA_CMD_DATA            0x02U
#define OTA_CMD_END             0x03U

#define OTA_RSP_ACK             0x06U
#define OTA_RSP_NACK            0x15U

#define OTA_CHUNK_SIZE          256U
#define OTA_MAX_PAYLOAD         (2U + OTA_CHUNK_SIZE)

#define OTA_IDLE_TIMEOUT_MS     5000U

#ifndef OTA_DEBUG
#define OTA_DEBUG               0
#endif

/* Chi tra ve khi idle timeout; thanh cong thi jump trong ota (khong return) */
void ota_run(uint32_t idle_timeout_ms);

#endif /* OTA_H_ */
