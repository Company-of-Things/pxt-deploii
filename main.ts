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
     * Send a number to a Deploii datastream.
     * Returns 0 on success, or a negative error code:
     * -9 Deploii is not connected (or not subscribed yet),
     * -10 message too large, -11 sending failed.
     * @param id the datastream ID from deploii.no
     * @param value the number to send
     */
    //% blockId=deploii_send_number
    //% block="send to Deploii datastream $id value $value"
    //% id.defl="datastream-id"
    //% weight=90
    export function sendNumber(id: string, value: number): number {
        return sendRaw(encodeMessage(id, packNumber(value)));
    }

    /**
     * Send an already encoded message (MessagePack bytes) to Deploii.
     * Advanced: prefer sendNumber.
     */
    //% shim=deploii::sendRaw
    //% blockHidden=1
    export function sendRaw(data: Buffer): number {
        // Simulator only
        return 0;
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
