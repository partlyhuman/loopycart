import {Serial} from './serial';

const textDecoder = new TextDecoder();

const $ = document.querySelector.bind(document);
const $connectButton = $("#connect");
const $statusDisplay = $('#status');
let port = null;

function addLine(linesId, text) {
    var senderLine = document.createElement("div");
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

function connect() {
    port.connect().then(() => {
        $statusDisplay.textContent = '';
        $connectButton.textContent = 'Disconnect';

        port.onReceive = data => {
            console.log(textDecoder.decode(data));
                appendLines('receiver_lines', textDecoder.decode(data));
            // }
        };
        port.onReceiveError = error => {
            console.error(error);
        };
    }, error => {
        $statusDisplay.textContent = error;
    });
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
            connect();
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
    // typed array with file contents
    const buffer = new Uint16Array(await files[0].arrayBuffer());
    console.log(buffer);

    // Cheap way to block this off
    const BLOCKSIZE = 64;
    port.send(`P${buffer.length}${'\r'.repeat(BLOCKSIZE)}`.substring(0, BLOCKSIZE));

    console.log('here goes the buffer');
    port.send(buffer);
});

$('.cls').addEventListener('click', () => {
    $('#receiver_lines').innerHTML = '';
})

$('.flash-erase').addEventListener('click', () => {
    // if connected
    port.send('E\r');
});

$('.flash-dump').addEventListener('click', () => {
    port.send('D\r');
})
