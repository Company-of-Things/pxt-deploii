#ifndef DEPLOII_SERVICE_H
#define DEPLOII_SERVICE_H

#include "pxt.h"
#include "deploii_config.h"

// Bluetooth is only available on micro:bit V2 (CODAL) with the BLE stack enabled.
// pxt.json turns it on through the "microbit-dal.bluetooth.enabled" setting.
#if MICROBIT_CODAL && CONFIG_ENABLED(DEVICE_BLE) && CONFIG_ENABLED(MICROBIT_BLE_ENABLED)
#define DEPLOII_HAS_BLE 1
#else
#define DEPLOII_HAS_BLE 0
#endif

#if DEPLOII_HAS_BLE

#include "MicroBitBLEManager.h"
#include "MicroBitBLEService.h"

/*
 * The Deploii GATT service.
 *
 * Mirrors DeploiiHandler::connect() in the Arduino library
 * (src/handler/arduino_uno_r4_wifi/deploii_handler_unoR4_bluetooth_ble.h):
 *
 *   service  <hash(boardID)>   16-bit UUID on the Bluetooth SIG base
 *     0x4C1D  send     read | notify   micro:bit -> Deploii
 *     0xA7F3  receive  write | notify  Deploii -> micro:bit
 *
 * and advertises the service UUID so Deploii can find the board.
 */
class DeploiiService : public MicroBitBLEService
{
  public:
    DeploiiService();

    /*
     * Add the service and its characteristics to the GATT table.
     * Safe to call again after a failure; does nothing once it has succeeded.
     * Returns DEPLOII_OK or a DEPLOII_ERR_* code.
     */
    int begin(uint16_t serviceUUID);

    /*
     * Replace the default micro:bit advertising data with
     * flags + this service's UUID (scan response: device name), then start advertising.
     * MicroBitBLEManager restarts advertising with the same data after a disconnect.
     * Returns DEPLOII_OK or a DEPLOII_ERR_* code.
     */
    int startAdvertising();

    /*
     * Send data to Deploii using the Arduino library's packet format:
     * each packet is [packets remaining - 1][payload...].
     * Returns DEPLOII_OK or a DEPLOII_ERR_* code.
     */
    int send(const uint8_t *data, size_t size);

    /*
     * The last value Deploii wrote to the receive characteristic.
     */
    const uint8_t *receivedData() const { return receiveBuffer; }
    uint16_t receivedLength() const { return receiveLength; }

    uint16_t uuid() const { return serviceUUID; }
    bool isReady() const { return ready; }
    bool isAdvertising() const { return advertising; }

    // Last raw nRF / SoftDevice error code, for debugging (0 = none)
    uint32_t lastNrfError() const { return nrfError; }

    // MicroBitBLEService interface
    int characteristicCount() override { return IDX_COUNT; }
    MicroBitBLEChar *characteristicPtr(int idx) override { return &chars[idx]; }

  protected:
    void onDataWritten(const microbit_ble_evt_write_t *params) override;

  private:
    enum
    {
        IDX_SEND,
        IDX_RECEIVE,
        IDX_COUNT
    };

    MicroBitBLEChar chars[IDX_COUNT];

    // Characteristic values live here (CODAL stores values in application memory)
    uint8_t sendBuffer[DEPLOII_BLE_BUFFER_MAX_SIZE];
    uint8_t receiveBuffer[DEPLOII_BLE_BUFFER_MAX_SIZE];
    uint16_t receiveLength;

    uint16_t serviceUUID;
    bool serviceAdded;
    bool ready;
    bool advertising;
    uint32_t nrfError;
};

#endif // DEPLOII_HAS_BLE

#endif // DEPLOII_SERVICE_H
