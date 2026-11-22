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

// Memory the SoftDevice stores prepared writes in during a long write
static uint8_t queuedWriteMem[DEPLOII_QUEUED_WRITE_MEM_SIZE];
static ble_user_mem_block_t queuedWriteBlock = {queuedWriteMem, sizeof(queuedWriteMem)};

DeploiiService::DeploiiService()
    : rxHead(0), rxCount(0), rxDropped(0),
      serviceUUID(0), serviceAdded(false), ready(false), advertising(false), nrfError(0)
{
    memset(sendBuffer, 0, sizeof(sendBuffer));
    memset(receiveBuffer, 0, sizeof(receiveBuffer));
    memset(rxLength, 0, sizeof(rxLength));
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

    // A plain write holds the whole message, like on the Arduino (no header byte).
    // Long writes arrive as BLE_GATTS_OP_EXEC_WRITE_REQ_NOW and are handled in onExecuteWrite().
    if ((params->op == BLE_GATTS_OP_WRITE_REQ || params->op == BLE_GATTS_OP_WRITE_CMD) && params->offset == 0)
        queueMessage(params->data, params->len);
}

bool DeploiiService::onBleEvent(const microbit_ble_evt_t *p_ble_evt)
{
    switch (p_ble_evt->header.evt_id)
    {
    case BLE_EVT_USER_MEM_REQUEST:
        // The peer started a long write. Lend the SoftDevice memory for the prepared writes;
        // without a reply the write would stall. No other CODAL service answers this event.
        if (p_ble_evt->evt.common_evt.params.user_mem_request.type == BLE_USER_MEM_TYPE_GATTS_QUEUED_WRITES)
        {
            memset(queuedWriteMem, 0, sizeof(queuedWriteMem));
            uint32_t err = sd_ble_user_mem_reply(p_ble_evt->evt.common_evt.conn_handle, &queuedWriteBlock);
            if (err != NRF_SUCCESS)
                nrfError = err;
        }
        break;

    case BLE_GATTS_EVT_WRITE:
        if (p_ble_evt->evt.gatts_evt.params.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW)
            onExecuteWrite(p_ble_evt->evt.gatts_evt.conn_handle);
        break;

    default:
        break;
    }

    return MicroBitBLEService::onBleEvent(p_ble_evt);
}

void DeploiiService::onExecuteWrite(uint16_t connHandle)
{
    // The SoftDevice has applied the prepared writes to the attribute table (so to receiveBuffer).
    // Check the queued-write memory to see whether our characteristic was written. Its layout is
    // a list of [handle:2][offset:2][length:2][data:length] (little-endian), ended by handle 0.
    uint16_t ours = valueHandle(IDX_RECEIVE);
    bool found = false;
    bool wellFormed = true;
    size_t i = 0;

    while (i + 2 <= sizeof(queuedWriteMem))
    {
        uint16_t handle = queuedWriteMem[i] | (queuedWriteMem[i + 1] << 8);
        if (handle == BLE_GATT_HANDLE_INVALID)
            break;
        if (i + 6 > sizeof(queuedWriteMem))
        {
            wellFormed = false;
            break;
        }
        uint16_t length = queuedWriteMem[i + 4] | (queuedWriteMem[i + 5] << 8);
        i += 6 + length;
        if (i > sizeof(queuedWriteMem))
        {
            wellFormed = false;
            break;
        }
        if (handle == ours)
            found = true;
    }

    // Only another characteristic was written: not for us
    if (wellFormed && !found)
        return;

    // Read back the complete value, its length is the total length of the long write
    uint8_t value[DEPLOII_BLE_BUFFER_MAX_SIZE];
    ble_gatts_value_t v;
    memset(&v, 0, sizeof(v));
    v.len = sizeof(value);
    v.offset = 0;
    v.p_value = value;

    uint32_t err = sd_ble_gatts_value_get(connHandle, ours, &v);
    if (err != NRF_SUCCESS)
    {
        nrfError = err;
        return;
    }

    queueMessage(value, v.len < sizeof(value) ? v.len : sizeof(value));
}

void DeploiiService::queueMessage(const uint8_t *data, uint16_t len)
{
    if (len == 0)
        return;
    if (len > DEPLOII_BLE_BUFFER_MAX_SIZE)
        len = DEPLOII_BLE_BUFFER_MAX_SIZE;

    target_disable_irq();
    if (rxCount == DEPLOII_RX_QUEUE_SIZE)
    {
        rxDropped++;
        target_enable_irq();
        return;
    }
    uint8_t slot = (rxHead + rxCount) % DEPLOII_RX_QUEUE_SIZE;
    memcpy(rxQueue[slot], data, len);
    rxLength[slot] = len;
    rxCount++;
    target_enable_irq();

    // One event per message: the TypeScript handler pops one message each time it runs
    MicroBitEvent(DEPLOII_ID, DEPLOII_EVT_DATA_RECEIVED);
}

bool DeploiiService::popMessage(uint8_t *dst, uint16_t *len)
{
    target_disable_irq();
    if (rxCount == 0)
    {
        target_enable_irq();
        return false;
    }
    *len = rxLength[rxHead];
    memcpy(dst, rxQueue[rxHead], *len);
    rxHead = (rxHead + 1) % DEPLOII_RX_QUEUE_SIZE;
    rxCount--;
    target_enable_irq();
    return true;
}

#endif // DEPLOII_HAS_BLE
