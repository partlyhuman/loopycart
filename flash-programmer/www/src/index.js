import {Serial} from './serial';

const PROTOCOL_VERSION = 2;
const PAD = 0xffff;
const SERIAL_BUFFER_SIZE = 64;

const SRAM_SIZE = 8192;
const ROM_SIZE_SMALL = 1 << 21;

const textDecoder = new TextDecoder();

const $ = document.querySelector.bind(document);
const $connectButton = $('#connect');
const $statusDisplay = $('#status');
const $lines = $('#receiver_lines');

let port = null;

// Pads a command to the serial buffer size (64 bytes) with extra \rs
// Do this to send a command that expects data to follow, so the command is predictable size
function padCommand(str) {
    return `${str}${'\r'.repeat(SERIAL_BUFFER_SIZE)}`.substring(0, SERIAL_BUFFER_SIZE);
}

function serialEcho(data) {
    appendLines(textDecoder.decode(data));
}

let firstData;
let versionTimeout;

function versionCheck(data) {
    clearTimeout(versionTimeout);
    firstData += textDecoder.decode(data);
    let end = firstData.indexOf('\0');
    if (end >= 0) {
        let receivedVersion = parseInt(firstData.slice(0, end));
        if (receivedVersion === PROTOCOL_VERSION) { // Match, echo any more text
            let moreText = firstData.slice(end + 1);
            if (moreText) appendLines(moreText);
            port.onReceive = serialEcho;
        } else { // Mismatch, disconnect
            disconnect();
            $statusDisplay.textContent = `Protocol version mismatch! App: ${PROTOCOL_VERSION}, Device: ${receivedVersion}`;
        }
    } else {
        versionTimeout = setTimeout(() => {
            disconnect();
            $statusDisplay.textContent = "Protocol version check timed out.";
        }, 200);
    }
}

function addLine(text) {
    const senderLine = document.createElement('div');
    senderLine.className = 'line';
    const textnode = document.createTextNode(text);
    senderLine.appendChild(textnode);
    $lines.appendChild(senderLine);
    return senderLine;
}

let currentReceiverLine;

function appendLines(text) {
    const lines = text.split('\r');
    if (currentReceiverLine) {
        currentReceiverLine.innerHTML = currentReceiverLine.innerHTML + lines[0];
        for (let i = 1; i < lines.length; i++) {
            currentReceiverLine = addLine(lines[i]);
        }
    } else {
        for (let i = 0; i < lines.length; i++) {
            currentReceiverLine = addLine(lines[i]);
        }
    }
    $lines.scrollTo(0, $lines.scrollHeight, {smooth: true});
}

async function connect() {
    try {
        await port.connect();
        $statusDisplay.textContent = '';
        $connectButton.textContent = 'Disconnect';
        firstData = '';
        port.onReceive = versionCheck;
        port.onReceiveError = error => console.error(error);
    } catch (error) {
        $statusDisplay.textContent = error;
    }
}

async function disconnect() {
    port.disconnect();
    $connectButton.textContent = 'Connect';
    $statusDisplay.textContent = '';
    port = null;
}

$connectButton.addEventListener('click', async () => {
    if (port) {
        await disconnect();
    } else {
        Serial.requestPort().then(selectedPort => {
            port = selectedPort;
            connect().then();
        }).catch(error => {
            $statusDisplay.textContent = error;
        });
    }
});

Serial.getPorts().then(ports => {
    if (ports.length === 0) {
        $statusDisplay.textContent = 'No device found.';
    } else {
        $statusDisplay.textContent = 'Connecting...';
        port = ports[0];
        connect();
    }
});


$('.flash-upload').addEventListener('change', async ({target: {files}}) => {
    if (!files || files.length === 0) {
        console.log('No file selected');
        return;
    }
    console.log('UPLOADING!');
    let buffer = new Uint16Array(await files[0].arrayBuffer());

    // Detect padding and un-pad
    const lastWord = buffer.findLastIndex(w => w !== PAD);
    if (lastWord >= 0) {
        buffer = buffer.subarray(0, lastWord + 1);
    }

    console.log(`Sending ${buffer.byteLength} bytes / ${buffer.length} words`);

    port.send(padCommand(`P${buffer.length}`));
    port.send(buffer);
});

$('.sram-upload').addEventListener('change', async ({target: {files}}) => {
    if (!files || files.length === 0) {
        console.log('No file selected');
        return;
    }
    console.log('UPLOADING!');
    let buffer = new Uint8Array(await files[0].arrayBuffer());

    if (buffer.byteLength !== SRAM_SIZE) {
        console.error(`Save expected to be ${SRAM_SIZE} bytes, was ${buffer.byteLength}`);
        return;
    }

    port.send(padCommand(`Ps`));
    port.send(buffer);
});


$('.flash-inspect').addEventListener('click', () => {
    port.send('I\r');
});

$('.sram-backup').addEventListener('click', () => {
    port.send('Sr\r');
});

$('.sram-restore').addEventListener('click', () => {
    port.send('Sw\r');
});

function download(filename = 'loopy.bin', bytesToDownload, serialCommand) {
    return new Promise((resolve) => {
        const dumpBuffer = new ArrayBuffer(bytesToDownload);
        let addrBytes = 0;

        port.onReceive = (data) => {
            const dumpView = new Uint8Array(dumpBuffer, addrBytes, data.byteLength);
            const packetView = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
            dumpView.set(packetView);
            addrBytes += data.byteLength;
            console.log(addrBytes);
            if (addrBytes >= dumpBuffer.byteLength) {
                const a = document.createElement('a');
                a.href = URL.createObjectURL(new Blob([dumpBuffer], {type: 'application/octet-stream'}));
                a.download = filename;
                a.innerText = 'Download dump';
                a.click();

                port.onReceive = serialEcho;

                resolve();
            }
        }

        port.send(serialCommand);
    });
}

$('.flash-download').addEventListener('click', () => {
    // 2MB dump
    const expectedWords = 1 << 20;
    download('loopy-rom.bin', expectedWords * 2, `D${expectedWords}\r`).then();
});
$('.sram-download').addEventListener('click', () => {
    download('loopy.sav', SRAM_SIZE, `Ds\r`).then();
});

$('.flash-erase').addEventListener('click', () => {
    // if connected
    port.send('E\r');
});
$('.flash-erase-one').addEventListener('click', () => {
    // if connected
    port.send('E0\r');
});
$('.sram-erase').addEventListener('click', () => {
    // if connected
    port.send('Es\r');
});

$('.cls').addEventListener('click', () => {
    $('#receiver_lines').innerHTML = '';
});
