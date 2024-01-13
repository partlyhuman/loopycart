import {Serial} from './serial';
import {ADDR_HEADER_END, getCartDataFromHeader, lookupCartDatabase, parseRom, trimEnd} from './cart';

// Warn if this doesn't match. Inserting this could be automated but that would require lockstep commits
// Something better could be done with build automation that builds Arduino and web
const FW_CURRENT = '84c7776';

const SERIAL_BUFFER_SIZE = 64;
export const SRAM_SIZE = 1 << 17;
const UPLOAD_CHUNK_SIZE = 1024;
const TRUNCATE_DUMPED_SRAM = true;

const textDecoder = new TextDecoder();
/** @type Port */
let port;

export const $ = document.querySelector.bind(document);
const $body = $('body');
const $connectButton = $('button#connect');
const $statusDisplay = $('#progress-text');
const $lines = $('#receiver_lines');

export function sleep(ms) {
    return new Promise((resolve) => {
        setTimeout(resolve, ms);
    });
}

let currentOperationResolver;

function waitForStatus() {
    return new Promise(resolve => currentOperationResolver = resolve);
}

/**
 * Rounds to next integral serial buffer size
 * @return {number}
 */
function roundSize(s) {
    return Math.ceil(s / SERIAL_BUFFER_SIZE) * SERIAL_BUFFER_SIZE;
}

// The default port.onReceive handler. Parses text and puts it in the console.
function serialEcho(buffer) {
    if (ArrayBuffer.isView(buffer)) buffer = buffer.buffer;
    const data = (buffer instanceof Uint8Array) ? buffer : new Uint8Array(buffer);
    // trim null-terminated strings
    const lastByte = data.indexOf(0x00);
    const text = textDecoder.decode(lastByte < 0 ? data : data.subarray(0, lastByte));
    if (text.match(/^!OK\b/m)) {
        setProgress(false);
        if (typeof currentOperationResolver === 'function') {
            currentOperationResolver();
            currentOperationResolver = null;
        }
    }
    log(text);
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

export function log(text, copyToConsole = false) {
    const lines = text.split('\r');
    if (currentReceiverLine) {
        currentReceiverLine.innerHTML = currentReceiverLine.innerHTML + lines.shift();
    }
    for (const line of lines) {
        currentReceiverLine = addLine(line);
    }
    if (copyToConsole) {
        console.log(text.replace('\r', '').trim());
    }
    $lines.scrollTo(0, $lines.scrollHeight, {smooth: true});
}

function setProgress(n) {
    const $inner = $('#progress-inner');
    const $outer = $('#progress-outer');
    if (typeof (n) === 'number') {
        $outer.classList.remove('marquee');
        $inner.style.width = `${n * 100}%`;
    } else if (typeof (n) === 'boolean' && n === true) {
        $outer.classList.add('marquee');
    } else {
        $outer.classList.remove('marquee');
        $inner.style.width = '0';
    }
}

window.setProgress = setProgress;

function setBusy(b) {
    if (b) {
        $body.classList.add('busy');
    } else {
        $body.classList.remove('busy');
    }
    document.querySelectorAll('button').forEach(el => {
        if (!el.classList.contains('ignore-busy')) {
            el.disabled = b;
        }
    });
}

window.setBusy = setBusy;

function setStatus(str) {
    $statusDisplay.innerText = str ?? '';
    console.log(str);
}

function showError(str) {
    const $dialog = $('#dialog-error');

    console.error(str);
    log("ERROR: " + str + "\r\n");

    return new Promise((resolve) => {
        $dialog.querySelector('.body').innerText = str;
        $dialog.querySelectorAll('button').forEach(btn => {
            const result = btn.dataset['result'];
            btn.addEventListener('click', (event) => {
                $dialog.classList.remove('open');
                resolve(result);
            }, {once: true});
        });
        $dialog.classList.add('open');
    });
}

window.showError = showError;

export async function commandWithProgress(command, stepInfo = '', indefinite = true) {
    setProgress(indefinite || 0);
    setStatus(stepInfo);
    await port.send(command);
    await waitForStatus();
}

async function connect() {
    try {
        await port.connect();
        $statusDisplay.innerText = 'Connected';
        $connectButton.classList.remove('default');

        // Parse initial connection message
        port.onReceive = (data) => {
            const text = textDecoder.decode(data);
            const match = text.match(/^!FW ([A-Za-z0-9]+)/);
            const fw = match?.[1];
            if (fw !== FW_CURRENT) {
                $('.download-firmware').classList.remove('hidden');
            }
            port.onReceive = serialEcho;
            serialEcho(data);
        }
    } catch (error) {
        $statusDisplay.innerText = error;
    }
}

async function disconnect() {
    await port.disconnect();
    $connectButton.classList.add('default');
    port = null;
}

$connectButton?.addEventListener('click', async () => {
    if (port) {
        await disconnect();
    } else {
        Serial.requestPort().then(selectedPort => {
            port = selectedPort;
            connect().then();
        }).catch(error => {
            $statusDisplay.innerText = error;
        });
    }
});

Serial.getPorts().then(ports => {
    if (ports.length === 0) {
        $statusDisplay.innerText = 'No device found';
    } else {
        $statusDisplay.innerText = 'Connecting...';
        port = ports[0];
        connect().then();
    }
});

async function downloadAndParseCartHeader() {
    const size = roundSize(ADDR_HEADER_END);
    return getCartDataFromHeader(await download(size, `D${size}\r`));
}

export function download(bytesToDownload, serialCommand) {
    if (bytesToDownload % SERIAL_BUFFER_SIZE !== 0) {
        console.warn('Expected download size is not modulo serial buffer size');
    }
    return new Promise((resolve) => {
        const dumpBuffer = new ArrayBuffer(bytesToDownload);
        console.log(`Dumping ${bytesToDownload} bytes...`);
        let addrBytes = 0;

        port.onReceive = (/** @type DataView */ data) => {
            const dumpView = new Uint8Array(dumpBuffer, addrBytes, data.byteLength);
            const packetView = new Uint8Array(data.buffer, 0, data.buffer.byteLength);
            console.assert(ArrayBuffer.isView(dumpView), 'Expected dumpView to be a view into the shared dumpBuffer');
            console.assert(ArrayBuffer.isView(packetView), 'Expected packetView to be a view into the shared data.buffer');

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

        port.send(serialCommand).then();
    });
}

export async function programFlash(buffer) {
    await port.send(`P${buffer.byteLength}\r`);
    await uploadChunked(buffer);
    // Important to allow confirmation text to come from Floopy over serial and be parsed
    await sleep(250);

}

export async function programSram(buffer) {
    if (buffer.byteLength !== SRAM_SIZE) {
        throw new Error(`Expected buffer of SRAM size ${SRAM_SIZE}`);
    }
    await port.send(`Ps${buffer.byteLength}\r`);
    await uploadChunked(buffer);
    // Important to allow confirmation text to come from Floopy over serial and be parsed
    await sleep(250);
}

async function uploadChunked(buffer) {
    try {
        for (let addr = 0; addr < buffer.byteLength;) {
            setProgress(addr / buffer.byteLength);
            const chunk = new Uint8Array(buffer.buffer, addr, Math.min(UPLOAD_CHUNK_SIZE, buffer.byteLength - addr));
            addr += chunk.byteLength;

            const {bytesWritten, status} = await port.send(chunk);
            if (status !== "ok") throw new Error(`${status}`);
            // console.log(`${status} writing ${block.byteLength} byte block starting at ${block.byteOffset}, ${bytesWritten} actually written`);
        }
        console.log("DONE!");
    } finally {
        setProgress(0);
    }
}

export function saveBufferToFile(dumpBuffer, filename = 'loopy.bin') {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([dumpBuffer], {type: 'application/octet-stream'}));
    a.download = filename;
    a.click();
}

//------------------- BASIC UI -------------------

async function simpleFlash(/** @type File */ file) {
    setBusy(true);
    try {
        if (!port?.isOpen) {
            throw new Error('Not connected!');
        }

        // check to see if dropped is a loopy bin
        // TODO eliminate word arrays
        let buffer = new Uint16Array(await file.arrayBuffer());
        let {header: newGameHeader, name: newGameName} = parseRom(buffer);
        newGameName ??= "Unrecognized ROM";
        const originalSize = buffer.byteLength;

        // Sanity checks
        if (originalSize > 0x400000) {
            throw new Error('Bigger than Floopy Drive flash!');
        }
        buffer = trimEnd(buffer);

        // What's on there right now?
        const currentRomHeader = await downloadAndParseCartHeader();
        const isDifferentGame = currentRomHeader.checksum !== newGameHeader.checksum;

        // If different game, backup SRAM
        if (isDifferentGame && currentRomHeader.checksum !== 0xffffffff) {
            await commandWithProgress('Sr\r', 'Backing up current save');
        }

        // Erase only what's needed - great for small homebrew
        await commandWithProgress(`E${buffer.byteLength}\r`, `Erasing flash`);

        // Erase either half or full
        // if (originalSize <= 0x200000) {
        //     await commandWithProgress('E0\r', 'Erasing flash (half)');
        // } else {
        //     await commandWithProgress('E\r', 'Erasing flash (all)');
        // }

        // Flash
        setStatus(`Flashing ${newGameName}`)
        await programFlash(buffer);

        // Restore old save or format SRAM if no existing backup
        if (isDifferentGame) {
            await commandWithProgress('Sw\r', 'Restoring previous save');
        }

        setStatus('DONE!');
    } catch (err) {
        await showError(err.message);
    } finally {
        setBusy(false);
    }
    console.log('done', new Date())

}

const $drop = $('.drop');
window.addEventListener('dragover', e => e.preventDefault());
window.addEventListener('drop', e => e.preventDefault());
$drop?.addEventListener('drop', event => {
    event.preventDefault();
    event.target.classList.remove('over');
    simpleFlash(event.dataTransfer.files?.[0]).then();
});
$drop?.addEventListener('click', () => $('.flash-upload-simple').click());
$drop?.addEventListener('dragover', (event) => event.target.classList.add('over'));
$drop?.addEventListener('dragleave', (event) => event.target.classList.remove('over'));
$drop?.addEventListener('dragend', (event) => event.target.classList.remove('over'));

const $advancedMode = $('#advanced-mode');
// restore initial state
if (localStorage.getItem('advanced') === 'true') {
    $advancedMode.setAttribute('open', 'open');
}
$advancedMode?.addEventListener('click', async event => {
    await sleep(0);
    const open = $advancedMode.hasAttribute('open');
    console.log(open.toString());
    localStorage.setItem('advanced', open.toString());
});

//------------------- ADVANCED UI ----------------

$('.flash-upload-button')?.addEventListener('click', () => $('input.flash-upload').click());

$('input.flash-upload')?.addEventListener('change', async ({target, target: {files}}) => {
    try {
        if (!files || files.length === 0) {
            throw new Error('No file selected');
        }
        // TODO eliminate word arrays
        let buffer = new Uint16Array(await files[0].arrayBuffer());
        const info = parseRom(buffer);
        if (info) {
            console.log(`Identified ${info.name}`);
        }

        console.log('UPLOADING!');

        buffer = trimEnd(buffer);
        console.log(`Sending ${buffer.byteLength} bytes / ${buffer.length} words`);

        await programFlash(buffer);
    } catch (error) {
        showError(error).then();
    } finally {
        console.dir(target);
        // Allow set same file again
        target.value = null;
    }
});

$('input.flash-upload-simple')?.addEventListener('change', ({target: {files}}) => {
    if (!files || files.length === 0) {
        console.log('No file selected');
        return;
    }
    simpleFlash(files[0]).then();
});

$('.sram-upload-button')?.addEventListener('click', () => $('input.sram-upload').click());
$('input.sram-upload')?.addEventListener('change', async ({target, target: {files}}) => {
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

    await port.send(`Ps${buffer.byteLength}\r`);
    await uploadChunked(buffer);

    target.value = null;
});


$('button.flash-inspect')?.addEventListener('click', async () => {
    const header = await downloadAndParseCartHeader();
    console.log(header);
    log(`Parsed header: ${JSON.stringify(header)}\r\n`);
    const cart = lookupCartDatabase(header.checksum);
    if (cart) {
        console.log(cart);
        log(`Identified in database as: ${JSON.stringify(cart)}\r\n\r\n`);
    }

    await port.send('I\r');
});

$('button.sram-backup')?.addEventListener('click', async () => {
    await port.send('Sr\r');
});

$('button.sram-restore')?.addEventListener('click', async () => {
    await port.send('Sw\r');
});

$('button.sram-format-fs')?.addEventListener('click', async () => {
    await port.send('Sf\r');
});

$('button.flash-download')?.addEventListener('click', async () => {
    const header = await downloadAndParseCartHeader();
    console.log(`downloading ${header.romSize} bytes`);
    saveBufferToFile(await download(header.romSize, `D${header.romSize}\r`), 'loopy-rom.bin');
});

$('button.sram-download')?.addEventListener('click', async () => {
    const header = await downloadAndParseCartHeader();
    const cart = lookupCartDatabase(header.checksum);

    // SRAM always downloads the whole thing, this is a good thing, we can truncate if desired
    let buffer = await download(SRAM_SIZE, `Ds\r`);
    if (cart && TRUNCATE_DUMPED_SRAM) {
        if (header.sramSize > 0 && header.sramSize < buffer.byteLength) {
            console.log(`truncating ${buffer.byteLength / 1024}kb downloaded to declared save size of ${header.sramSize / 1024}kb`)
            buffer = new Uint8Array(buffer, 0, header.sramSize);
        }
        saveBufferToFile(buffer, `${cart.name}.sav`);
    } else {
        saveBufferToFile(buffer, 'loopy.sav')
    }
});

$('button.flash-erase')?.addEventListener('click', async () => {
    setProgress(true);
    await port.send('E\r');
});
$('button.flash-erase-one')?.addEventListener('click', async () => {
    setProgress(true);
    await port.send('E0\r');
});
$('button.sram-erase')?.addEventListener('click', async () => {
    setProgress(true);
    await port.send('Es\r');
});

$('.device-nickname button')?.addEventListener('click', async () => {
    const nick = $('.device-nickname input').value;
    await port.send(`N${nick}\r`);
});

$('form.manual-command')?.addEventListener('submit', async (event) => {
    event.preventDefault();
    const $input = $('.manual-command input');
    const cmd = `${$input.value.toUpperCase().trim()}\r`;
    $input.value = '';
    await port.send(cmd);
    return false;
});

$('button.cls')?.addEventListener('click', () => {
    $('#receiver_lines').innerHTML = '';
});
