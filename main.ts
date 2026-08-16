namespace deploii {

    //% blockId=deploii-loop
    //% block="deploii loop"
    export function loop() : void {
        return;
    }

    //% blockId=deploii-connect
    //% block="connect with cardID: $cardID"
    export function connect(cardID: string) : void {
        let UUID = generateServiceUUID(cardID)
        basic.showNumber(UUID);
        return
    }

    //% blockId=deploii-send
    //% block="send $data to $datastream" 
    export function send(datastream: string, data: any) : void {
        return
    }

    //% blockId=deploii-interval
    //% block="every $t ms, do $f"
    //% t.min=1 t.defl=1000
    //% f.shadow=functions
    export function interval(t: number, f: () => void) : void {
        return
    }

    function generateServiceUUID(cardID: string) : number {

        let hash = 0x811c // FNV offset basis (16-bit)

        for (let i = 0; i < cardID.length; i++) {
            hash ^= cardID.charCodeAt(i)
            hash = (hash * 0x0101) & 0xffff // FNV prime, keep 16-bit
        }

        return hash
    }
}