#ifndef DEPLOII_CONFIG_H
#define DEPLOII_CONFIG_H

/*
 * Constants shared with the Deploii Arduino library
 * (github.com/Company-of-Things/deploii-library, src/deploii_config.h).
 * Keep these in sync with that library so Deploii sees the same protocol.
 */

// Characteristic UUIDs (16-bit, on the Bluetooth SIG base UUID, same as ArduinoBLE "a7f3"/"4c1d")
#define DEPLOII_BLE_RECEIVE_CHARACTERISTIC_UUID 0xA7F3 // Deploii -> micro:bit (write | notify)
#define DEPLOII_BLE_SEND_CHARACTERISTIC_UUID    0x4C1D // micro:bit -> Deploii (read | notify)

// Maximum characteristic value size, same as the Arduino library
#define DEPLOII_BLE_BUFFER_MAX_SIZE 128

// Size of one transmitted packet, header byte included.
// CODAL v2 runs with the default ATT MTU of 23 bytes (NRF_SDH_BLE_GATT_MAX_MTU_SIZE),
// so one notification can carry at most 23 - 3 = 20 bytes. The Arduino library sends
// up to 128 bytes per packet; the header byte (packets remaining) lets Deploii
// reassemble either size. Raise this only if you also raise the MTU (see README).
#ifndef DEPLOII_BLE_PACKET_SIZE
#define DEPLOII_BLE_PACKET_SIZE 20
#endif

// Connection configuration tag used by MicroBitBLEManager (microbit_ble_CONN_CFG_TAG)
#define DEPLOII_BLE_CONN_CFG_TAG 1

// Message bus ID for events raised by the Deploii service
#define DEPLOII_ID 9620
#define DEPLOII_EVT_DATA_RECEIVED 1

// Set to 1 to print debug messages on the USB serial port (115200 baud)
#ifndef DEPLOII_DEBUG
#define DEPLOII_DEBUG 0
#endif

#if DEPLOII_DEBUG
#define DEPLOII_LOG(...) uBit.serial.printf(__VA_ARGS__)
#else
#define DEPLOII_LOG(...) ((void)0)
#endif

// Return codes for deploii::connect (0 = success)
#define DEPLOII_OK                      0
#define DEPLOII_ERR_BLE_UNAVAILABLE    -1 // built without Bluetooth, or BLE stack not running
#define DEPLOII_ERR_ALREADY_CONNECTED  -2 // connect() was already called successfully
#define DEPLOII_ERR_INVALID_BOARD_ID   -3 // empty board ID
#define DEPLOII_ERR_SERVICE_ADD        -4 // sd_ble_gatts_service_add failed
#define DEPLOII_ERR_CHARACTERISTIC_ADD -5 // a characteristic could not be added
#define DEPLOII_ERR_ADV_ENCODE         -6 // advertising/scan response data too large
#define DEPLOII_ERR_ADV_CONFIGURE      -7 // sd_ble_gap_adv_set_configure failed
#define DEPLOII_ERR_ADV_START          -8 // sd_ble_gap_adv_start failed (e.g. already connected)
#define DEPLOII_ERR_NOT_CONNECTED      -9 // send() with no central connected / not subscribed
#define DEPLOII_ERR_TOO_LARGE         -10 // send() payload needs more than 256 packets
#define DEPLOII_ERR_SEND_FAILED       -11 // notification could not be queued

#endif // DEPLOII_CONFIG_H
