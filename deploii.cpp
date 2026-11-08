#include "pxt.h"
#include "deploii_config.h"
#include "deploii_hash.h"
#include "deploii_service.h"

using namespace pxt;

namespace deploii {

#if DEPLOII_HAS_BLE
static DeploiiService *service = NULL;
#endif

/*
 * C++ side of deploii.connect() in main.ts.
 * Hashes the board ID into the service UUID, creates the Deploii service and starts advertising.
 * Returns DEPLOII_OK (0) or a negative DEPLOII_ERR_* code from deploii_config.h.
 *
 * (A //% line with no doc comment exports the function without adding it to shims.d.ts,
 * because main.ts already declares it.)
 */
//%
int connect(String boardID) {
#if DEPLOII_HAS_BLE
    if (service != NULL && service->isAdvertising())
        return DEPLOII_ERR_ALREADY_CONNECTED;

    if (!(uBit.bleManager.status & DEVICE_COMPONENT_RUNNING))
        return DEPLOII_ERR_BLE_UNAVAILABLE;

    if (boardID == NULL || boardID->getUTF8Size() == 0)
        return DEPLOII_ERR_INVALID_BOARD_ID;

    uint16_t uuid = deploii_hash(boardID->getUTF8Data(), boardID->getUTF8Size());
    DEPLOII_LOG("[deploii] board ID \"%s\" -> UUID 0x%04x\r\n", boardID->getUTF8Data(), (unsigned int)uuid);

    // A service cannot be removed from the GATT table, so it is created once and
    // reused if connect() is called again after a failure.
    if (service == NULL)
        service = new DeploiiService();
    else if (service->isReady() && service->uuid() != uuid)
        return DEPLOII_ERR_ALREADY_CONNECTED;

    int result = service->begin(uuid);
    if (result != DEPLOII_OK)
        return result;

    return service->startAdvertising();
#else
    (void)boardID;
    return DEPLOII_ERR_BLE_UNAVAILABLE;
#endif
}

/*
 * C++ side of deploii.sendRaw() in main.ts.
 * Sends an already encoded message using the Arduino packet format.
 * Returns DEPLOII_OK (0) or a negative DEPLOII_ERR_* code.
 */
//%
int sendRaw(Buffer data) {
#if DEPLOII_HAS_BLE
    if (service == NULL || !service->isReady())
        return DEPLOII_ERR_NOT_CONNECTED;
    if (data == NULL)
        return DEPLOII_OK;
    return service->send(data->data, data->length);
#else
    (void)data;
    return DEPLOII_ERR_BLE_UNAVAILABLE;
#endif
}

} // namespace deploii
