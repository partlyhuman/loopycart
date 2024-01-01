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
        this.onReceive = () => {
        };
        this.onReceiveError = () => {
        };
    }

    get isOpen() {
        return this._device.opened;
    }

    async connect() {
        const readLoop = () => {
            this._device.transferIn(this.endpointIn, BUFSIZE).then(result => {
                this.onReceive(result.data);
                readLoop();
            }, error => {
                this.onReceiveError(error);
            });
        };

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
            'requestType': 'class',
            'recipient': 'interface',
            'request': 0x22,
            'value': 0x01,
            'index': this.interfaceNumber
        });

        readLoop();
    }

    async disconnect() {
        await this._device.controlTransferOut({
            'requestType': 'class',
            'recipient': 'interface',
            'request': 0x22,
            'value': 0x00,
            'index': this.interfaceNumber
        });
        return await this._device.close();
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

