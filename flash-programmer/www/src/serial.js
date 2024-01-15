// Based on https://example.tinyusb.org/webusb-serial/serial.js
// Some modernizations by @partlyhuman

const USB_VENDOR_ID = 0x239a; // Adafruit USB library, or make a vanity one for Floopy perhaps
const BUFSIZE = 64;
const textEncoder = new TextEncoder();
const outBuffer = new Uint8Array(BUFSIZE);

export class Serial {
    static async getPorts() {
        const devices = await navigator.usb.getDevices();
        return devices.map(device => new Port(device));
    }

    static async requestPort() {
        const device = await navigator.usb.requestDevice({
            'filters': [
                {vendorId: USB_VENDOR_ID},
            ]
        });
        return new Port(device);
    }
}


export class Port {

    constructor(device) {
        this._device = device;
        this.interfaceNumber = 0;
        this.endpointIn = 0;
        this.endpointOut = 0;
        const noop = () => {
        };
        this.onReceive = noop;
        this.onReceiveError = noop;
        this.onDisconnect = noop;
    }

    get isOpen() {
        return this._device.opened;
    }

    async connect() {
        await this._device.open();

        if (this._device.configuration === null) {
            return this._device.selectConfiguration(1);
        }

        const interfaces = this._device.configuration.interfaces;
        interfaces.forEach(element => {
            element.alternates.forEach(elementalt => {
                if (elementalt.interfaceClass === 0xFF) {
                    this.interfaceNumber = element.interfaceNumber;
                    elementalt.endpoints.forEach(elementendpoint => {
                        if (elementendpoint.direction === "out") {
                            this.endpointOut = elementendpoint.endpointNumber;
                        }
                        if (elementendpoint.direction === "in") {
                            this.endpointIn = elementendpoint.endpointNumber;
                        }
                    })
                }
            })
        })

        await this._device.claimInterface(this.interfaceNumber);

        await this._device.selectAlternateInterface(this.interfaceNumber, 0);
        await this._device.controlTransferOut({
            requestType: 'class',
            recipient: 'interface',
            request: 0x22,
            value: 0x01,
            index: this.interfaceNumber,
        });

        navigator.usb.addEventListener('disconnect', ({device}) => {
            if (device === this._device) {
                this.onDisconnect();
            }
        });

        const readLoop = async () => {
            try {
                const {data} = await this._device.transferIn(this.endpointIn, BUFSIZE);
                this.onReceive(data);
                // Clear the stack
                setTimeout(readLoop, 0);
            } catch (error) {
                this.onReceiveError(error);
            }
        }
        // This could help avoid deadlock between initial read and write
        // TODO add a NOOP function to firmware to explicitly do nothing without triggering "unknown command"
        await this.send("D");
        // don't await, we're ready to go as soon as this returns
        readLoop().then();
    }

    async disconnect() {
        await this._device.controlTransferOut({
            requestType: 'class',
            recipient: 'interface',
            request: 0x22,
            value: 0x00,
            index: this.interfaceNumber,
        });
        await this._device.releaseInterface(this.interfaceNumber);
        await this._device.close();
    }

    async send(data) {
        if (typeof data === 'string') {
            console.log(`>${data}`);
            // Ensure padded to buffer size
            textEncoder.encodeInto(data, outBuffer);
            data = outBuffer;
        }
        return await this._device.transferOut(this.endpointOut, data);
    }
}

