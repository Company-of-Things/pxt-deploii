#include "deploii_service.h"

#if DEPLOII_HAS_BLE

#include "ble_advdata.h"
#include "ble_gap.h"
#include "ble_gatts.h"

using namespace pxt;

// Advertising buffers must stay valid while the SoftDevice is advertising
static uint8_t advData[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
static uint8_t scanRspData[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
static ble_uuid_t advUuid;

DeploiiService::DeploiiService()
    : receiveLength(0), serviceUUID(0), serviceAdded(false), ready(false), advertising(false),
      nrfError(0)
{
    memset(sendBuffer, 0, sizeof(sendBuffer));
    memset(receiveBuffer, 0, sizeof(receiveBuffer));
}

int DeploiiService::begin(uint16_t uuid)
{
    if (ready)
        return DEPLOII_OK;

    if (!serviceAdded)
    {
        serviceUUID = uuid;

        // 16-bit UUIDs on the Bluetooth SIG base (0000xxxx-0000-1000-8000-00805F9B34FB),
        // which is what ArduinoBLE uses for short UUID strings like "a7f3".
        // MicroBitBLEService's CreateCharacteristic() reads bs_uuid_type.
        bs_uuid_type = BLE_UUID_TYPE_BLE;

        ble_uuid_t service;
        service.uuid = uuid;
        service.type = BLE_UUID_TYPE_BLE;

        // Called directly instead of MicroBitBLEService::CreateService() so we get the error code
        uint32_t err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &service, &bs_service_handle);
        if (err != NRF_SUCCESS)
        {
            nrfError = err;
            DEPLOII_LOG("[deploii] service add failed: 0x%x\r\n", (unsigned int)err);
            return DEPLOII_ERR_SERVICE_ADD;
        }
        serviceAdded = true;

        // Same order and properties as the Arduino library
        CreateCharacteristic(IDX_SEND, DEPLOII_BLE_SEND_CHARACTERISTIC_UUID,
                             sendBuffer, 0, sizeof(sendBuffer),
                             microbit_propREAD | microbit_propNOTIFY);

        CreateCharacteristic(IDX_RECEIVE, DEPLOII_BLE_RECEIVE_CHARACTERISTIC_UUID,
                             receiveBuffer, 0, sizeof(receiveBuffer),
                             microbit_propWRITE | microbit_propNOTIFY);
    }

    // CreateCharacteristic() swallows errors, so check that the SoftDevice handed out handles
    if (valueHandle(IDX_SEND) == 0 || valueHandle(IDX_RECEIVE) == 0)
    {
        DEPLOII_LOG("[deploii] characteristic add failed\r\n");
        return DEPLOII_ERR_CHARACTERISTIC_ADD;
    }

    ready = true;
    DEPLOII_LOG("[deploii] service 0x%04x ready\r\n", (unsigned int)serviceUUID);
    return DEPLOII_OK;
}

int DeploiiService::startAdvertising()
{
    if (!ready)
        return DEPLOII_ERR_SERVICE_ADD;

    advUuid.uuid = serviceUUID;
    advUuid.type = BLE_UUID_TYPE_BLE;

    // Advertising packet: flags + complete list of 16-bit service UUIDs (our service)
    ble_advdata_t adv;
    memset(&adv, 0, sizeof(adv));
    adv.name_type = BLE_ADVDATA_NO_NAME;
    adv.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    adv.uuids_complete.uuid_cnt = 1;
    adv.uuids_complete.p_uuids = &advUuid;

    // Scan response: full device name ("BBC micro:bit [xxxxx]"), shown in the browser's device list
    ble_advdata_t scanRsp;
    memset(&scanRsp, 0, sizeof(scanRsp));
    scanRsp.name_type = BLE_ADVDATA_FULL_NAME;

    ble_gap_adv_data_t gapData;
    memset(&gapData, 0, sizeof(gapData));

    gapData.adv_data.p_data = advData;
    gapData.adv_data.len = sizeof(advData);
    uint32_t err = ble_advdata_encode(&adv, gapData.adv_data.p_data, &gapData.adv_data.len);
    if (err != NRF_SUCCESS)
    {
        nrfError = err;
        return DEPLOII_ERR_ADV_ENCODE;
    }

    gapData.scan_rsp_data.p_data = scanRspData;
    gapData.scan_rsp_data.len = sizeof(scanRspData);
    err = ble_advdata_encode(&scanRsp, gapData.scan_rsp_data.p_data, &gapData.scan_rsp_data.len);
    if (err != NRF_SUCCESS)
    {
        nrfError = err;
        return DEPLOII_ERR_ADV_ENCODE;
    }

    ble_gap_adv_params_t params;
    memset(&params, 0, sizeof(params));
    params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    params.interval = (1000 * MICROBIT_BLE_ADVERTISING_INTERVAL) / 625; // 0.625 ms units
    params.duration = BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED;
    params.filter_policy = BLE_GAP_ADV_FP_ANY;
    params.primary_phy = BLE_GAP_PHY_1MBPS;

    // The SoftDevice (S113) has a single advertising set. MicroBitBLEManager::init() created it
    // at boot as handle 0; reusing it means the manager's automatic re-advertise after a
    // disconnect keeps advertising our data.
    uint8_t handle = 0;

    // Stop the default advertising. Fails harmlessly if nothing is being advertised.
    sd_ble_gap_adv_stop(handle);

    err = sd_ble_gap_adv_set_configure(&handle, &gapData, &params);
    if (err == BLE_ERROR_INVALID_ADV_HANDLE)
    {
        // No set configured yet: let the SoftDevice create one
        handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
        err = sd_ble_gap_adv_set_configure(&handle, &gapData, &params);
    }
    if (err != NRF_SUCCESS)
    {
        nrfError = err;
        DEPLOII_LOG("[deploii] adv configure failed: 0x%x\r\n", (unsigned int)err);
        return DEPLOII_ERR_ADV_CONFIGURE;
    }

    err = sd_ble_gap_adv_start(handle, DEPLOII_BLE_CONN_CFG_TAG);
    if (err != NRF_SUCCESS)
    {
        nrfError = err;
        DEPLOII_LOG("[deploii] adv start failed: 0x%x\r\n", (unsigned int)err);
        return DEPLOII_ERR_ADV_START;
    }

    advertising = true;
    DEPLOII_LOG("[deploii] advertising 0x%04x\r\n", (unsigned int)serviceUUID);
    return DEPLOII_OK;
}

int DeploiiService::send(const uint8_t *data, size_t size)
{
    if (!getConnected() || !notifyChrValueEnabled(IDX_SEND))
        return DEPLOII_ERR_NOT_CONNECTED;

    if (size == 0)
        return DEPLOII_OK;

    const size_t payloadMax = DEPLOII_BLE_PACKET_SIZE - 1; // one byte for the header
    size_t packets = (size + payloadMax - 1) / payloadMax;
    if (packets > 0x100)
        return DEPLOII_ERR_TOO_LARGE;

    // Header byte = number of packets still to come after this one
    uint8_t header = (uint8_t)(packets - 1);
    uint8_t packet[DEPLOII_BLE_PACKET_SIZE];
    size_t remaining = size;

    while (remaining > 0)
    {
        size_t n = remaining < payloadMax ? remaining : payloadMax;
        packet[0] = header;
        memcpy(packet + 1, data, n);

        // The SoftDevice TX queue can be full; give it a moment and retry
        bool sent = false;
        for (int attempt = 0; attempt < 50 && !sent; attempt++)
        {
            sent = notifyChrValue(IDX_SEND, packet, (uint16_t)(n + 1));
            if (!sent)
            {
                if (!getConnected())
                    return DEPLOII_ERR_NOT_CONNECTED;
                fiber_sleep(10);
            }
        }
        if (!sent)
            return DEPLOII_ERR_SEND_FAILED;

        data += n;
        remaining -= n;
        header--;
    }

    return DEPLOII_OK;
}

void DeploiiService::onDataWritten(const microbit_ble_evt_write_t *params)
{
    if (params->handle != valueHandle(IDX_RECEIVE))
        return;

    // The SoftDevice has already written the value into receiveBuffer
    receiveLength = params->len < sizeof(receiveBuffer) ? params->len : sizeof(receiveBuffer);

    DEPLOII_LOG("[deploii] received %d bytes\r\n", (int)receiveLength);
    MicroBitEvent(DEPLOII_ID, DEPLOII_EVT_DATA_RECEIVED);
}

#endif // DEPLOII_HAS_BLE
