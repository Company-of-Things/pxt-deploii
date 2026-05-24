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

    //% blockid=deploii-interval
    //% block="every $t ms, do $f"
    export function interval(f:()=>void, t:number) : void {
        f();
        return;
    }

}