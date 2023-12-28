import {commandWithProgress, download, programFlash} from './index';

const BLOCK_SIZE = 32 * 1024;


function fillWith(size, byte = 0xff) {
    const buffer = new Uint8Array(size);
    for (let i = 0; i < size; i++) {
        buffer[i] = byte;
    }
    return buffer;
}

function fillRandom(size) {
    const buffer = new Uint8Array(size);
    for (let i = 0; i < size; i++) {
        buffer[i] = Math.floor(Math.random() * 0x100);
    }
    return buffer;
}

function assertEqual(in1, in2) {
    const buffer1 = new Uint8Array(in1);
    const buffer2 = new Uint8Array(in2);
    if (buffer1.length !== buffer2.length) {
        throw new Error('Sizes differ');
    }
    for (let i = 0; i < buffer1.length; i++) {
        if (buffer1[i] !== buffer2[i]) {
            throw new Error(`Mismatch at offset 0x${i.toString(16)}`);
        }
    }
}

/**
 * Test 0 - on building - Quickly verify the flash is blank, all 0xff
 * Soldering errors can result in non-ff. Passing this test doesn't mean everything's ok.
 */
export async function assertFirstBlockBlank() {
    assertEqual(
        await download(BLOCK_SIZE, `D${BLOCK_SIZE}\r`),
        fillWith(BLOCK_SIZE, 0xff)
    );
}

/**
 * Test 1 - Quickly fill a block with random stuff and see if it can be read back
 * Ensures flash is writeable.
 */
export async function assertEraseAndWrite(size = BLOCK_SIZE) {
    await commandWithProgress(`E${size}\r`, 'Erase');
    const expected = fillRandom(size);
    await programFlash(expected);
    const actual = await download(size, `D${size}\r`);
    assertEqual(expected, actual);
}

/**
 * Test 2 - Erase both banks of flash and write the entire 4mb. Write the entire 128kb of SRAM.
 * Don't check the contents yet!
 * Ensures both the upper and lower banks are writeable.
 * Contents: A bootable 3MB ROM appended with random up to 4MB, and a known good save appended with random up to 128kb.
 * Now boot in Loopy. Ensure ROM runs and the save loads.
 */

/**
 * Test 3 - read back flash and SRAM. Verify it's as expected.
 * This ensures SRAM survives removal of power.
 * Loopy booting the game is a pretty good indication that things work. Verifying every single byte like this may or may not be overkill.
 */

/**
 * Test 4 - format everything. Verify it's clear as expected.
 * We already tested erasing, but this final check also prepares the floopy drive for shipping.
 */

// TODO trigger with some sort of UI, consider making this a separate page
