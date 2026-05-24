namespace deploii{

    //% blockId=deploii-loop
    //% block="deploii loop"
    export function loop() : void {
        return;
    }

    //% blockId=deploii-connect
    //% block="connect to $ssid, with password $password"
    export function connect(ssid: string, password: string) : void {
        return;
    }

    //% blockId=deploii-send
    //% block="send $data to $datastream" 
    export function send(datastream: string, data: any) : void {
        return;
    }

    //% blockId=deploii-interval
    //% t.min=1 t.defl=1000
    //% f.shadow=functions
    export function interval(t: number, f: () => void) : void {
        return;
    }

}