import LoopyCsvText from "./data/cartDatabase";
import {sprintf} from "sprintf-js";

export const OFFSET_ROM = 0x0E000000;
export const OFFSET_SRAM = 0x02000000;

export const ADDR_MAGIC = 0x0;
export const ADDR_ROM_END = 0x000004;
export const ADDR_CHECKSUM = 0x000008;
export const ADDR_SRAM_START = 0x000010;
export const ADDR_SRAM_END = 0x000014;
export const ADDR_HEADER_END = 0x000018;

// Index by internal CRC *as a string*
const cartDatabase = {};
for (let [name, product, mbit, fullCrc, internalCrc] of LoopyCsvText.split("\n").filter(x => !x.startsWith('#') && x.trim().length > 0).map(line => line.split(',').map(s => s.trim()))) {
    cartDatabase[internalCrc.toLowerCase()] = {name, product, mbit, fullCrc, internalCrc};
}

function getHeaderUint32(buffer, addr, littleEndian = false) {
    return new DataView(buffer, addr, 4).getUint32(0, littleEndian);
}

export function getSramSize(buffer) {
    return (getHeaderUint32(buffer, ADDR_SRAM_END) - getHeaderUint32(buffer, ADDR_SRAM_START));
}

export function getRomSize(buffer) {
    return (getHeaderUint32(buffer, ADDR_ROM_END) + 2 - OFFSET_ROM);
}

export function getChecksum(buffer) {
    return getHeaderUint32(buffer, ADDR_CHECKSUM);
}


export function lookupCartDatabase(checksum) {
    if (typeof(checksum) === 'number') {
        checksum = checksum.toString(16);
    }
    return cartDatabase[checksum];
}

export function getCartDataFromDatabase(buffer) {
    if (ArrayBuffer.isView(buffer)) buffer = buffer.buffer;
    const internalCrcString = sprintf("%08x", getChecksum(buffer));
    console.log("Cart ID", internalCrcString);
    return cartDatabase[internalCrcString];
}

export function getCartDataFromHeader(buffer) {
    if (ArrayBuffer.isView(buffer)) buffer = buffer.buffer;
    return {
        checksum: getChecksum(buffer),
        sramSize: getSramSize(buffer),
        romSize: getRomSize(buffer),
    };
}

export const HEADER_UNRECOGNIZED = 0;
export const HEADER_OK = 1;
export const HEADER_LITTLE_ENDIAN = 2;
export const HEADER_BLANK = 3;

export function getCartHeaderMagic(buffer) {
    if (ArrayBuffer.isView(buffer)) buffer = buffer.buffer;
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
    if (ArrayBuffer.isView(buffer)) buffer = buffer.buffer;
    const view = new Uint8Array(buffer)
    for (let i = 0; i < view.byteLength; i += 2) {
        const a = view[i];
        const b = view[i + 1];
        view[i] = b;
        view[i + 1] = a;
    }
}
