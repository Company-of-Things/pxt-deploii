/**
 * Deploii: connect your micro:bit to deploii.no over Bluetooth Low Energy.
 *
 * The micro:bit speaks the same BLE protocol as the Deploii Arduino library
 * (Arduino UNO R4 handler): one primary service whose 16-bit UUID is a hash
 * of the board ID, with a "send" (0x4C1D) and a "receive" (0xA7F3)
 * characteristic.
 */
//% color="#2b6cb0" weight=95 icon="" block="Deploii"
namespace deploii {

    // Handler registered with onReceive (only one, like the Arduino library)
    let receiveHandler: (moduleId: string, data: any) => void = undefined;
    let receiveRegistered = false;

    /**
     * Create the Deploii Bluetooth service for this board and start advertising.
     * Returns 0 on success, or a negative error code (see deploii_config.h):
     * -1 Bluetooth unavailable, -2 already connected, -3 invalid board ID,
     * -4 service add failed, -5 characteristic add failed,
     * -6 advertising data encode failed, -7 advertising configure failed,
     * -8 advertising start failed.
     * @param boardID the board ID from deploii.no
     */
    //% blockId=deploii_connect
    //% block="connect to Deploii with board ID $boardID"
    //% boardID.defl="board-id"
    //% shim=deploii::connect
    //% weight=100
    export function connect(boardID: string): number {
        // Simulator only: the simulator has no Bluetooth, so pretend it worked.
        // On hardware the C++ implementation in deploii.cpp runs instead.
        return 0;
    }

    /**
     * Send a value to a Deploii datastream.
     * The value can be a number, text, true/false, or a list of these (lists can be nested).
     * Returns 0 on success, or a negative error code:
     * -9 Deploii is not connected (or not subscribed yet),
     * -10 message too large, -11 sending failed.
     * @param id the datastream ID from deploii.no
     * @param value the value to send
     */
    //% blockId=deploii_send
    //% block="send to Deploii datastream $id value $value"
    //% id.defl="datastream-id"
    //% value.shadow=math_number
    //% weight=90
    export function send(id: string, value: any): number {
        return sendRaw(encodeMessage(id, packValue(value)));
    }

    /**
     * Run code when Deploii sends data to this board.
     * Like the Arduino library's receive(), only messages of type "module_data" are passed on.
     * Calling this again replaces the previous handler.
     * @param handler code to run, with the module ID and the data
     */
    //% blockId=deploii_on_receive
    //% block="on Deploii data received $moduleId $data"
    //% draggableParameters="reporter"
    //% weight=80
    export function onReceive(handler: (moduleId: string, data: any) => void): void {
        receiveHandler = handler;
        if (receiveRegistered) return;
        receiveRegistered = true;

        // One event per received message; each run takes one message from the queue
        onDataReceived(function () {
            const msg = receiveMessage();
            if (!msg) return;
            if (msg[MessageField.Type] != "module_data") return;
            if (receiveHandler) receiveHandler(msg[MessageField.ModuleId], msg[MessageField.Data]);
        });
    }

    /**
     * Indexes into the array returned by receiveMessage(), see deploii_config.h
     */
    export const enum MessageField {
        ModuleId = 0,
        Data = 1,
        CardId = 2,
        Type = 3,
        Protocol = 4
    }

    /**
     * Send an already encoded message (MessagePack bytes) to Deploii.
     * Advanced: prefer send.
     */
    //% shim=deploii::sendRaw
    //% blockHidden=1
    export function sendRaw(data: Buffer): number {
        // Simulator only
        return 0;
    }

    /**
     * Run a handler for every message Deploii writes to the board, of any type.
     * Advanced: call receiveMessage() in the handler to get the message. Prefer onReceive.
     */
    //% shim=deploii::onDataReceived
    //% blockHidden=1
    export function onDataReceived(handler: () => void): void {
        // Simulator only: nothing is ever received
    }

    /**
     * Take the oldest received message, decoded as [moduleId, data, cardId, type, protocol]
     * (see MessageField). Returns undefined if no message is waiting or it could not be decoded.
     * Advanced: prefer onReceive.
     */
    //% shim=deploii::receiveMessage
    //% blockHidden=1
    export function receiveMessage(): any[] {
        // Simulator only
        return undefined;
    }

    /*
     * Message encoding, identical to Deploii::send() in the Arduino library:
     * MSGPACK_DEFINE_MAP(id, data) takes key/value pairs, so the message is a
     * MessagePack map with one entry, { "<datastream id>": value }.
     */
    function encodeMessage(id: string, packedValue: Buffer): Buffer {
        return Buffer.fromArray([0x81]) // fixmap, 1 entry
            .concat(packString(id))
            .concat(packedValue);
    }

    function packValue(v: any): Buffer {
        if (v === null || v === undefined) return Buffer.fromArray([0xc0]);    // nil
        if (typeof v == "boolean") return Buffer.fromArray([v ? 0xc3 : 0xc2]); // true / false
        if (typeof v == "number") return packNumber(v);
        if (typeof v == "string") return packString(v);
        if (Array.isArray(v)) {
            const items = v as any[];
            const n = items.length;
            let out: Buffer;
            if (n < 16) out = Buffer.fromArray([0x90 | n]);                  // fixarray
            else out = Buffer.fromArray([0xdc, n >> 8, n & 0xff]);           // array 16
            for (const item of items) out = out.concat(packValue(item));
            return out;
        }
        // Anything else (images, sprites, ...) is sent as its text form
        return packString("" + v);
    }

    function packString(s: string): Buffer {
        const bytes = Buffer.fromUTF8(s);
        const n = bytes.length;
        let header: Buffer;
        if (n < 32) header = Buffer.fromArray([0xa0 | n]);          // fixstr
        else if (n < 256) header = Buffer.fromArray([0xd9, n]);     // str 8
        else header = Buffer.fromArray([0xda, n >> 8, n & 0xff]);   // str 16
        return header.concat(bytes);
    }

    function packNumber(v: number): Buffer {
        if (v == Math.floor(v) && v >= -2147483648 && v <= 2147483647) {
            if (v >= 0 && v < 128) return Buffer.fromArray([v]);        // positive fixint
            if (v < 0 && v >= -32) return Buffer.fromArray([v & 0xff]); // negative fixint
            const b = Buffer.create(5);
            b[0] = 0xd2;                                                // int 32
            b.setNumber(NumberFormat.Int32BE, 1, v);
            return b;
        }
        // Same as the Arduino library, which sends floats as float 32
        const f = Buffer.create(5);
        f[0] = 0xca;
        f.setNumber(NumberFormat.Float32BE, 1, v);
        return f;
    }
}
