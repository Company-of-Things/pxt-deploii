// Test program, only compiled when building this extension on its own
// (`make build`). It is not included when someone adds the extension to a project.
//
// Connects, then once a second sends a value to a datastream, cycling through the
// supported types (number, decimal, text, true/false, list). Data received from
// Deploii is shown on the display.
// Use the board ID and datastream ID from deploii.no.

const BOARD_ID = "6ab9110fbc68e21f35abd3e3"
const DATASTREAM_ID = "test123"

const result = deploii.connect(BOARD_ID)

if (result == 0) {
    basic.showIcon(IconNames.Yes)
} else {
    // Connect failed: show the error code, nothing is sent
    basic.showNumber(result)
}

bluetooth.onBluetoothConnected(function () {
    basic.showIcon(IconNames.Heart)
})

bluetooth.onBluetoothDisconnected(function () {
    basic.showIcon(IconNames.Yes)
})

deploii.onReceive(function (moduleId, data) {
    // Also print to the USB serial port, view with `make monitor`
    serial.writeLine("received from " + moduleId + ": " + data)
    basic.showString("" + data)
})

let counter = 0

basic.forever(function () {
    if (result == 0) {
        let value: any
        switch (counter % 5) {
            case 0: value = randint(0, 100); break           // number
            case 1: value = randint(0, 1000) / 10; break     // decimal
            case 2: value = "hello " + counter; break        // text
            case 3: value = counter % 2 == 0; break          // true/false
            default: value = [randint(0, 9), randint(0, 9), randint(0, 9)] // list
        }
        counter++

        const status = deploii.send(DATASTREAM_ID, value)

        // Toggle the top-right LED on every successful send.
        // -9 only means Deploii hasn't connected/subscribed yet, so it is ignored.
        if (status == 0) {
            led.toggle(4, 0)
        } else if (status != -9) {
            basic.showNumber(status)
        }
    }
    basic.pause(1000)
})
