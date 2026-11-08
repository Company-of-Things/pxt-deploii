// Test program, only compiled when building this extension on its own
// (`make build`). It is not included when someone adds the extension to a project.
//
// Connects, then sends a random number (0-100) to a datastream once a second, forever.
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

basic.forever(function () {
    if (result == 0) {
        const status = deploii.sendNumber(DATASTREAM_ID, randint(0, 100))

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
