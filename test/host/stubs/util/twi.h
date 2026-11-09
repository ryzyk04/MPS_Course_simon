#pragma once

#define TW_STATUS (TWSR & 0xF8)
#define TW_BUS_ERROR 0x00
#define TW_SR_SLA_ACK 0x60
#define TW_SR_ARB_LOST_SLA_ACK 0x68
#define TW_SR_DATA_ACK 0x80
#define TW_SR_DATA_NACK 0x88
#define TW_SR_STOP 0xA0
#define TW_ST_SLA_ACK 0xA8
#define TW_ST_ARB_LOST_SLA_ACK 0xB0
#define TW_ST_DATA_ACK 0xB8
#define TW_ST_DATA_NACK 0xC0
#define TW_ST_LAST_DATA 0xC8
#define TW_NO_INFO 0xF8
