import {$, assertConnected, connect, disconnect, sleep} from '../index';
import {qaFullWipe} from "./qa";


navigator.usb.addEventListener('connect', async () => {
    await sleep(100);
    assertConnected();
    await qaFullWipe();
    $('audio#ding')?.play();
    await disconnect();
});
