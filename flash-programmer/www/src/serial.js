// Based on https://example.tinyusb.org/webusb-serial/serial.js
// Some modernizations by @partlyhuman

const textEncoder = new TextEncoder();

export class Serial {
    static async getPorts() {
        const devices = await navigator.usb.getDevices();
        return devices.map(device => new Port(device));
    }

    static async requestPort() {
        const filters = [
            {'vendorId': 0xcafe}, // TinyUSB
            {'vendorId': 0x239a}, // Adafruit
            {'vendorId': 0x2e8a}, // Raspberry Pi
            {'vendorId': 0x303a}, // Espressif
            {'vendorId': 0x2341}, // Arduino
        ];
        const device = await navigator.usb.requestDevice({'filters': filters});
        return new Port(device);
    }
}


export class Port {
    constructor(device) {
        this.device_ = device;
        this.interfaceNumber = 0;
        this.endpointIn = 0;
        this.endpointOut = 0;
    }

    async connect() {
        const readLoop = () => {
            this.device_.transferIn(this.endpointIn, 64).then(result => {
                this.onReceive(result.data);
                readLoop();
            }, error => {
                this.onReceiveError(error);
            });
        };

        await this.device_.open()

        if (this.device_.configuration === null) {
            return this.device_.selectConfiguration(1);
        }

        const interfaces = this.device_.configuration.interfaces;
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

        await this.device_.claimInterface(this.interfaceNumber);

        await this.device_.selectAlternateInterface(this.interfaceNumber, 0);
        await this.device_.controlTransferOut({
            'requestType': 'class',
            'recipient': 'interface',
            'request': 0x22,
            'value': 0x01,
            'index': this.interfaceNumber
        });

        readLoop();
    }

    async disconnect() {
        await this.device_.controlTransferOut({
            'requestType': 'class',
            'recipient': 'interface',
            'request': 0x22,
            'value': 0x00,
            'index': this.interfaceNumber
        });
        return await this.device_.close();
    }

    async send(data) {
        if (typeof data === 'string') {
            data = textEncoder.encode(data);
        }
        return await this.device_.transferOut(this.endpointOut, data);
    }
}

