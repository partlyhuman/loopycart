import LoopyCsvText from "./data/cartDatabase";
import {sprintf} from "sprintf-js";

// Index by internal CRC *as a string*
const cartDatabase = {};
for (let [name, product, mbit, fullCrc, internalCrc] of LoopyCsvText.split("\n").filter(x => !x.startsWith('#') && x.trim().length > 0).map(line => line.split(','))) {
    cartDatabase[internalCrc.toLowerCase()] = {name, product, mbit, fullCrc, internalCrc};
}

export function getInternalCrc(buffer, littleEndian = false) {
    if (buffer instanceof Uint16Array) buffer = buffer.buffer;
    const view = new DataView(buffer, 0x8, 4);
    return view.getUint32(0, littleEndian);
}

export function getCartData(buffer) {
    if (buffer instanceof Uint16Array) buffer = buffer.buffer;
    const internalCrcString = sprintf("%08x", getInternalCrc(buffer));
    console.log("Cart ID", internalCrcString);
    return cartDatabase[internalCrcString];
}

export const HEADER_UNRECOGNIZED = 0;
export const HEADER_OK = 1;
export const HEADER_LITTLE_ENDIAN = 2;
export const HEADER_BLANK = 3;

export function getCartHeaderMagic(buffer) {
    if (buffer instanceof Uint16Array) buffer = buffer.buffer;
    const MAGIC = [0x0e00, 0x0080];
    const view = new DataView(buffer, 0, 4);

    if (view.getUint16(0, false) === MAGIC[0] && view.getUint16(2, false) === MAGIC[1]) {
        return HEADER_OK;
    } else if (view.getUint16(0, true) === MAGIC[0] && view.getUint16(2, true) === MAGIC[1]) {
        return HEADER_LITTLE_ENDIAN;
    } else if (view.getUint32(0) === 0xffffffff) {
        return HEADER_BLANK;
    } else {
        return HEADER_UNRECOGNIZED;
    }
}

export function swapBytes(buffer) {
    if (buffer instanceof Uint16Array) buffer = buffer.buffer;
    const view = new Uint8Array(buffer)
    for (let i = 0; i < view.byteLength; i += 2) {
        const a = view[i];
        const b = view[i + 1];
        view[i] = b;
        view[i + 1] = a;
    }
}
