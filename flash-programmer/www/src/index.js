import {Serial} from './serial';

const textDecoder = new TextDecoder();

const $ = document.querySelector.bind(document);
const $connectButton = $('#connect');
const $statusDisplay = $('#status');
let port = null;

function serialEcho(data) {
    let str = textDecoder.decode(data);
    appendLines('receiver_lines', str);
}

function addLine(linesId, text) {
    var senderLine = document.createElement('div');
    senderLine.className = 'line';
    var textnode = document.createTextNode(text);
    senderLine.appendChild(textnode);
    document.getElementById(linesId).appendChild(senderLine);
    return senderLine;
}

let currentReceiverLine;

function appendLines(linesId, text) {
    const lines = text.split('\r');
    if (currentReceiverLine) {
        currentReceiverLine.innerHTML = currentReceiverLine.innerHTML + lines[0];
        for (let i = 1; i < lines.length; i++) {
            currentReceiverLine = addLine(linesId, lines[i]);
        }
    } else {
        for (let i = 0; i < lines.length; i++) {
            currentReceiverLine = addLine(linesId, lines[i]);
        }
    }
}

async function connect() {
    try {
        await port.connect();
        $statusDisplay.textContent = '';
        $connectButton.textContent = 'Disconnect';
        port.onReceive = serialEcho;
        port.onReceiveError = error => console.error(error);
    } catch (error) {
        $statusDisplay.textContent = error;
    }
}

$connectButton.addEventListener('click', () => {
    if (port) {
        port.disconnect();
        $connectButton.textContent = 'Connect';
        $statusDisplay.textContent = '';
        port = null;
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
    const buffer = new Uint16Array(await files[0].arrayBuffer());

    // Cheap way to block this off
    const BLOCKSIZE = 64;
    port.send(`P${buffer.length}${'\r'.repeat(BLOCKSIZE)}`.substring(0, BLOCKSIZE));
    port.send(buffer);
});


$('.flash-inspect').addEventListener('click', () => {
    port.send('I\r');
});

$('.flash-download').addEventListener('click', () => {
    $('.dump-file-link').innerHTML = '';

    // const expectedWords = 1 << 20; // full 2mb
    const expectedWords = 1 << 10;
    // For sanity, we write the bytes in the order we get them; endianness is set on the platform / data
    const dumpBuffer = new ArrayBuffer(expectedWords * 2);
    let addrBytes = 0;
    port.onReceive = (data) => {
        const dumpView = new Uint8Array(dumpBuffer, addrBytes, data.byteLength);
        const packetView = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
        dumpView.set(packetView);
        addrBytes += data.byteLength;
        console.log(addrBytes);
        if (addrBytes >= dumpBuffer.byteLength) {
            console.log('DONE reading', addrBytes);
            const a = document.createElement('a');
            a.href = URL.createObjectURL(new Blob([dumpBuffer], {type: 'application/octet-stream'}));
            a.download = 'loopy.bin';
            a.innerText = 'Download dump';
            // $('.dump-file-link').appendChild(a);
            a.click();

            port.onReceive = serialEcho;
        }
    };
    port.send(`D${expectedWords}\r`);
});

$('.cls').addEventListener('click', () => {
    $('#receiver_lines').innerHTML = '';
});

$('.flash-erase').addEventListener('click', () => {
    // if connected
    port.send('E\r');
});
