import {Serial} from './serial';
import {
    ADDR_HEADER_END,
    getCartDataFromHeader,
    getCartHeaderMagic,
    HEADER_BLANK,
    HEADER_LITTLE_ENDIAN,
    HEADER_OK,
    HEADER_UNRECOGNIZED,
    lookupCartDatabase,
    swapBytes
} from "./cart";

const PROTOCOL_VERSION = 2;
const PAD = 0xffff;
const SERIAL_BUFFER_SIZE = 64;
const SRAM_SIZE = 1024 * 32;

const textDecoder = new TextDecoder();
let port = null;

const $ = document.querySelector.bind(document);
const $connectButton = $('#connect');
const $statusDisplay = $('#status');
const $lines = $('#receiver_lines');
const $progress = $('#progress');
const $busyIndicator = $('#busy-indicator');

function sleep(ms) {
    return new Promise((resolve) => {
        setTimeout(resolve, ms);
    });
}

// Pads a command to the serial buffer size (64 bytes) with extra \rs
// Do this to send a command that expects data to follow, so the command is predictable size
function padCommand(str) {
    return `${str}${'\r'.repeat(SERIAL_BUFFER_SIZE)}`.substring(0, SERIAL_BUFFER_SIZE);
}

function serialEcho(data) {
    const text = textDecoder.decode(data);
    if (text.match(/^!OK\b/m)) {
        setProgress(false);
    }
    appendLines(text);
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
    //TODO just innerText +=, enough of this
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

function setProgress(n) {
    $progress.innerText = n;
    if (typeof (n) === 'number') {
        $progress.classList.remove('indeterminate');
        $progress.indeterminate = false;
        $progress.value = n;
    } else if (typeof (n) === 'boolean' && n === true) {
        $progress.indeterminate = true;
        $progress.removeAttribute('value');
        $progress.classList.add('indeterminate');
    } else {
        $progress.classList.remove('indeterminate');
        $progress.indeterminate = false;
        $progress.value = 0;
    }
}

function setBusy(b) {
    if (b) {
        $busyIndicator.classList.add('busy');
    } else {
        $busyIndicator.classList.remove('busy');
    }
}

async function connect() {
    try {
        await port.connect();
        $statusDisplay.textContent = '';
        $connectButton.textContent = 'Disconnect';
        firstData = '';
        // NOTE Temporarily turned off version check because it prevents connection on reload (version only echoed when pico first connects?)
        // port.onReceive = versionCheck;
        // port.onReceiveError = error => console.error(error);
        port.onReceive = serialEcho;
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

function parseRom(buffer) {
    switch (getCartHeaderMagic(buffer)) {
        case HEADER_UNRECOGNIZED:
        case HEADER_BLANK:
            console.error("Does not appear to be a loopy ROM");
            return;
        case HEADER_LITTLE_ENDIAN:
            console.warn("Using a little endian dump, please update your ROMs to big endian");
            swapBytes(buffer);
            console.assert(getCartHeaderMagic(buffer) === HEADER_OK, "Something went wrong with the swap");
            break;
        case HEADER_OK:
            console.log("Header ok!");
            break;
        default:
            console.error("Unhandled case");
            break;
    }

    return lookupCartDatabase(getCartDataFromHeader(buffer).checksum);
}


async function downloadAndParseCartHeader() {
    return getCartDataFromHeader(await download(ADDR_HEADER_END, `D${ADDR_HEADER_END.toString(10)}\r`));
}

function download(bytesToDownload, serialCommand) {
    return new Promise((resolve) => {
        const dumpBuffer = new ArrayBuffer(bytesToDownload);
        console.log(`Dumping ${bytesToDownload} bytes...`);
        let addrBytes = 0;

        port.onReceive = (/** @type DataView */ data) => {
            const dumpView = new Uint8Array(dumpBuffer, addrBytes, data.byteLength);
            const packetView = new Uint8Array(data.buffer, 0, data.buffer.byteLength);
            console.assert(ArrayBuffer.isView(dumpView), 'Expected dumpView to be a view into the shared dumpBuffer');
            console.assert(ArrayBuffer.isView(packetView), 'Expected packetView to be a view into the shared data.buffer');

            // Should this be necessary?
            swapBytes(packetView);

            dumpView.set(packetView);
            addrBytes += packetView.byteLength;

            setProgress(addrBytes / bytesToDownload);

            if (addrBytes >= dumpBuffer.byteLength) {
                console.log('Done download!');
                port.onReceive = serialEcho;
                setProgress(0);
                resolve(dumpBuffer);
            }
        }

        port.send(serialCommand);
    });
}

function saveBufferToFile(dumpBuffer, filename = 'loopy.bin') {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([dumpBuffer], {type: 'application/octet-stream'}));
    a.download = filename;
    a.click();
}

//------------------- BUTTONS ----------------

$('.flash-upload').addEventListener('change', async ({target: {files}}) => {
    if (!files || files.length === 0) {
        console.log('No file selected');
        return;
    }
    let buffer = new Uint16Array(await files[0].arrayBuffer());
    const info = parseRom(buffer);
    if (info) console.log(`Identified ${info.name}`);

    console.log('UPLOADING!');

    // Detect padding and un-pad
    const lastWord = buffer.findLastIndex(w => w !== PAD);
    if (lastWord >= 0) {
        buffer = buffer.subarray(0, lastWord + 1);
    }

    console.log(`Sending ${buffer.byteLength} bytes / ${buffer.length} words`);

    port.send(padCommand(`P${buffer.length}`));

    // Is this important to break up?
    // port.send(buffer);
    try {
        const blockSize = 1024;
        for (let addr = 0; addr < buffer.byteLength;) {
            setProgress(addr / buffer.byteLength);
            const block = new Uint8Array(buffer.buffer, addr, Math.min(blockSize, buffer.byteLength - addr));
            addr += block.byteLength;

            setBusy(true); // really all this does is prove that when we block the pico really isn't responding any more - no progress while this stays on
            const {bytesWritten, status} = await port.send(block);
            if (status !== "ok") throw new Error(`${status}`);
            setBusy(false);
            // console.log(`${status} writing ${block.byteLength} byte block starting at ${block.byteOffset}, ${bytesWritten} actually written`);
        }
        console.log("DONE!");
        setProgress(0);
    } catch (e) {
        console.error(e);
        setProgress(0);
        // set error message in UI
    }

});

$('.sram-upload').addEventListener('change', async ({target: {files}}) => {
    if (!files || files.length === 0) {
        console.log('No file selected');
        return;
    }
    console.log('UPLOADING!');
    let buffer = new Uint8Array(await files[0].arrayBuffer());

    if (buffer.byteLength > SRAM_SIZE) {
        console.error(`Save expected to be ${SRAM_SIZE} bytes or less, was ${buffer.byteLength}`);
        return;
    }
    if (buffer.byteLength % 1024 !== 0) {
        console.warn(`Save expected to be divisible by 1kb`);
    }

    port.send(padCommand(`Ps`));
    port.send(buffer);
});


$('.flash-inspect').addEventListener('click', async () => {
    const header = await downloadAndParseCartHeader();
    console.log(header);
    appendLines(`Parsed header: ${JSON.stringify(header)}\r\n`);
    const data = lookupCartDatabase(header.checksum);
    if (data) {
        console.log(data);
        appendLines(`Identified in database as: ${JSON.stringify(data)}\r\n\r\n`);
    }

    port.send('I\r');
});

$('.sram-backup').addEventListener('click', () => {
    port.send('Sr\r');
});

$('.sram-restore').addEventListener('click', () => {
    port.send('Sw\r');
});

$('.flash-download').addEventListener('click', async () => {
    // const header = await downloadAndParseCartHeader();
    // console.log(header);
    // saveBufferToFile(await download(header.romSize, `D${header.romSize.toString(10)}\r`), 'loopy-rom.bin');
    const bytes = 1 << 20;
    saveBufferToFile(await download(bytes, `D${bytes.toString(10)}\r`), 'loopy-rom.bin');
});

$('.sram-download').addEventListener('click', () => {
    // TODO SRAM_SIZE does vary, use header (but needs to change on firmware too)
    download(SRAM_SIZE, `Ds\r`).then(buffer => saveBufferToFile(buffer, 'loopy.sav'));
});

$('.flash-erase').addEventListener('click', () => {
    setProgress(true);
    port.send('E\r');
});
$('.flash-erase-one').addEventListener('click', () => {
    setProgress(true);
    port.send('E0\r');
});
$('.sram-erase').addEventListener('click', () => {
    setProgress(true);
    port.send('Es\r');
});

function makeTestFile() {
    const arrayBuffer = new ArrayBuffer(1024 * 1024 * 2);
    const view = new DataView(arrayBuffer, 0, arrayBuffer.byteLength);
    console.log(`making a file size=${view.byteLength}`);
    let addr = 0;
    for (let len = 0; ; len++) {
        console.log(`span length ${len}`);
        for (let byte = 0; byte <= 0xff; byte++) {
            for (let i = 0; i < len; i++) {
                if (addr >= view.byteLength) return arrayBuffer;
                view.setUint8(addr++, byte);
            }
        }
    }
}

$('.cls').addEventListener('click', () => {
    console.log('done');
    saveBufferToFile(makeTestFile(), 'test.bin');

    $('#receiver_lines').innerHTML = '';
});
