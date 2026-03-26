/**
 * @file    someip_proto.h
 * @brief   SOME/IP 协议常量与消息结构——AP/CP 双侧共享定义
 *
 * 对应 IPC-ARCH-001 V2.0 §2.3 / §2.4
 * 本文件以纯 C99 编写，可被 CP（C11）和 AP（C++17）共同包含。
 *
 *  ┌────────────────────────────────────────────────────────────┐
 *  │  SOME/IP Header（16 bytes，Big-Endian，AUTOSAR 标准格式）   │
 *  ├──────────┬──────────┬────────┬──────────┬──────────────────┤
 *  │ Service  │ Method   │ Length │ Client+  │ ProtoVer IfaceVer│
 *  │ ID  [2B] │ ID  [2B] │  [4B]  │Session[4B│ MsgType RetCode │
 *  └──────────┴──────────┴────────┴──────────┴──────────────────┘
 */

#ifndef SOMEIP_PROTO_H
#define SOMEIP_PROTO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * IPC-ARCH-001 §2.3  IP / 端口规划
 * ================================================================ */
#define SOMEIP_SD_PORT          30490u   /**< 服务发现 UDP 端口（AUTOSAR 标准） */
#define SOMEIP_EVT_BASE_PORT    30500u   /**< 事件通知 UDP 端口基址 */
#define SOMEIP_METHOD_BASE_PORT 30600u   /**< 方法调用 TCP 端口基址 */

/** 本机仿真：SOC 和 MCU 均绑定 loopback */
#define SOMEIP_LOOPBACK_ADDR    "127.0.0.1"

/* ================================================================
 * IPC-ARCH-001 §2.4  服务目录 (Service ID)
 * ================================================================ */
#define SVC_ID_VEHICLE_SIGNAL   0x1001u  /**< VehicleSignalService  MCU→SOC */
#define SVC_ID_SAFETY_STATUS    0x1002u  /**< SafetyStatusService   MCU→SOC */
#define SVC_ID_POWER_MODE       0x1003u  /**< PowerModeService      双向    */
#define SVC_ID_DIAG_PROXY       0x1004u  /**< DiagnosticProxyService双向    */
#define SVC_ID_OTA_TRANSFER     0x1005u  /**< OTATransferService    SOC→MCU */
#define SVC_ID_HMI_COMMAND      0x1006u  /**< HMICommandService     SOC→MCU */
#define SVC_ID_NETWORK_STATUS   0x1007u  /**< NetworkStatusService  MCU→SOC */

/** VehicleSignalService 端口（UDP Event） */
#define VEHICLE_SIGNAL_SVC_PORT  30501u
#define VEHICLE_SIGNAL_EVT_ID    0x8001u  /**< Event ID（高位=1 表示通知）*/
#define VEHICLE_SIGNAL_INSTANCE  0x01u

/* ================================================================
 * SOME/IP 消息类型 / 返回码
 * ================================================================ */
#define SOMEIP_MSG_REQUEST           0x00u
#define SOMEIP_MSG_REQUEST_NO_RETURN 0x01u
#define SOMEIP_MSG_NOTIFICATION      0x02u
#define SOMEIP_MSG_RESPONSE          0x80u
#define SOMEIP_MSG_ERROR             0x81u

#define SOMEIP_RC_OK                 0x00u
#define SOMEIP_RC_NOT_OK             0x01u
#define SOMEIP_RC_UNKNOWN_SVC        0x02u

#define SOMEIP_PROTO_VER             0x01u

/* ================================================================
 * SOME/IP 消息头结构（大端字节序，网络序，packed）
 * ================================================================ */
#pragma pack(push, 1)
typedef struct {
    uint16_t service_id;    /**< Service ID           */
    uint16_t method_id;     /**< Method / Event ID    */
    uint32_t length;        /**< 后续字节数（含后8B头）*/
    uint16_t client_id;     /**< 客户端 ID            */
    uint16_t session_id;    /**< 会话 ID（单调递增）   */
    uint8_t  proto_ver;     /**< 协议版本 = 0x01      */
    uint8_t  iface_ver;     /**< 接口版本             */
    uint8_t  msg_type;      /**< 消息类型             */
    uint8_t  return_code;   /**< 返回码               */
} SomeIpHeader_t;           /* 16 bytes total */
#pragma pack(pop)

/* ================================================================
 * VehicleSignalService  Payload（IPC-ARCH-001 SPI 信号表对齐）
 * ================================================================ */
#pragma pack(push, 1)
typedef struct {
    float    vehicle_speed_kmh;   /**< 车速  km/h  (MCU→SOC, 10ms) */
    float    engine_rpm;          /**< 转速  RPM   (MCU→SOC, 10ms) */
    uint8_t  brake_pedal;         /**< 制动踏板 0/1               */
    float    steering_angle_deg;  /**< 方向盘转角 deg (MCU→SOC, 20ms)*/
    uint8_t  door_status;         /**< 车门状态 bitmask            */
    float    fuel_level_pct;      /**< 燃油液位 %                  */
    uint8_t  e2e_crc;             /**< E2E Profile 2 CRC8          */
    uint8_t  e2e_counter;         /**< E2E 计数器（0-255 循环）    */
} VehicleSignalPayload_t;  /* 20 bytes */
#pragma pack(pop)

/* ================================================================
 * SOME/IP SD Entry（服务发现，OfferService/FindService）
 * ================================================================ */
#pragma pack(push, 1)
typedef struct {
    uint8_t  type;          /**< 0x01=FindService, 0x00=OfferService */
    uint8_t  index_first;
    uint8_t  index_second;
    uint8_t  num_opts;
    uint16_t service_id;
    uint16_t instance_id;
    uint8_t  major_ver;
    uint8_t  ttl[3];        /**< 3 bytes TTL（秒）*/
    uint32_t minor_ver;
} SomeIpSdEntry_t;          /* 16 bytes */
#pragma pack(pop)

/* ================================================================
 * 工具函数：字节序转换（host ↔ network，big-endian）
 * ================================================================ */
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
  #define SOMEIP_HTONS(x)  (uint16_t)(((x) >> 8) | ((x) << 8))
  #define SOMEIP_HTONL(x)  (uint32_t)(          \
        (((x) & 0xFF000000u) >> 24) |           \
        (((x) & 0x00FF0000u) >>  8) |           \
        (((x) & 0x0000FF00u) <<  8) |           \
        (((x) & 0x000000FFu) << 24))
#else
  #define SOMEIP_HTONS(x)  (x)
  #define SOMEIP_HTONL(x)  (x)
#endif
#define SOMEIP_NTOHS  SOMEIP_HTONS
#define SOMEIP_NTOHL  SOMEIP_HTONL

/* ================================================================
 * E2E Profile 2 CRC8（多项式 0x1D，汽车以太网常用）
 * ================================================================ */
static inline uint8_t e2e_crc8(const uint8_t* data, uint32_t len)
{
    uint8_t crc = 0xFF;
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x80u) ? ((uint8_t)((crc << 1) ^ 0x1Du)) : (uint8_t)(crc << 1);
        }
    }
    return crc ^ 0xFF;
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* SOMEIP_PROTO_H */
