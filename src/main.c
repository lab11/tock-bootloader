/*
 * This file is part of StormLoader, the Storm Bootloader
 *
 * StormLoader is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * StormLoader is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with StormLoader.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2014, Michael Andersen <m.andersen@eecs.berkeley.edu>
 */

#include <string.h>

#include <ioport.h>
#include <asf.h>
#include <board.h>
#include <conf_board.h>
#include <wdt_sam4l.h>
#include <sysclk.h>
#include "bootloader.h"


extern void jump_into_user_code(void)  __attribute__((noreturn));



typedef struct {
    uint32_t signpost_tock_firmware_update_enable;
    uint32_t signpost_tock_firmware_update_source;
    uint32_t signpost_tock_firmware_update_destination;
    uint32_t signpost_tock_firmware_update_length;
    uint32_t signpost_tock_firmware_update_crc;
} signpost_tock_firmware_update_t;


void board_init(void) {
    // // Setup GPIO
    // ioport_init();

    // // Pin which is pulled low to enter bootloader mode.
    // ioport_set_pin_dir(BOOTLOADER_SELECT_PIN, IOPORT_DIR_INPUT);
    // ioport_set_pin_mode(BOOTLOADER_SELECT_PIN, IOPORT_MODE_PULLUP | IOPORT_MODE_GLITCH_FILTER);

    // Setup Clock
    bpm_set_clk32_source(BPM, BPM_CLK32_SOURCE_RC32K);
    sysclk_init();
}

uint32_t crc_internal_flash (uint32_t base, uint32_t len) {
    if (base >= 0x80000 || (base+len) > 0x80000+1) {
        return 0;
    }
    flashcalw_picocache_invalid_all();
    uint8_t* p = (uint8_t*) base;
    return  crc32(0, p, len);
}

void write_page (uint32_t* buffer, uint32_t pagenum) {
    flashcalw_default_wait_until_ready();
    flashcalw_erase_page(pagenum, true);
    flashcalw_picocache_invalid_all();
    flashcalw_default_wait_until_ready();
    flashcalw_clear_page_buffer();
    flashcalw_default_wait_until_ready();

    uint32_t *fp = (uint32_t*) (pagenum*512);
    uint16_t i;
    for (i=0; i < 128; i+=2) {
        *fp = 0xFFFFFFFF;
        *(fp+1) = 0xFFFFFFFF;
        *fp = buffer[i];
        *(fp+1) = buffer[i+1];
        fp+=2;
    }

    flashcalw_default_wait_until_ready();

    flashcalw_write_page(pagenum);
    flashcalw_picocache_invalid_all();
    flashcalw_default_wait_until_ready();
}

void copy_flash (uint32_t source, uint32_t dest, uint32_t len) {
    uint32_t buffer[128];

    // Make sure len is a multiple of 512;
    len = ((len + 511) / 512) * 512;

    for (uint32_t j=0; j<len; j+=512) {
        uint32_t pagenum = (dest + j) >> 9;
        memcpy((uint8_t*) buffer, source+j, 512);

        write_page(buffer, pagenum);
    }
}

int main (void) {
    board_init();

    // Check if we should be doing a firmware update.

    // Header is in a known place in flash.
    signpost_tock_firmware_update_t* stfu_header;
    stfu_header = (signpost_tock_firmware_update_t*) 0x5EC;

    do {
        if (!stfu_header->signpost_tock_firmware_update_enable) {
            // No firmware update ready.
            break;
        }

        // Looks like we need to do a copy.

        // First check the CRC.
        uint32_t crc = crc_internal_flash(
            stfu_header->signpost_tock_firmware_update_source,
            stfu_header->signpost_tock_firmware_update_length);

        if (crc != stfu_header->signpost_tock_firmware_update_crc) {
            break;
        }

        // Now do some sanity checking.
        if (stfu_header->signpost_tock_firmware_update_source < 0x10000 ||
            stfu_header->signpost_tock_firmware_update_destination < 0x10000 ||
            (stfu_header->signpost_tock_firmware_update_destination +
             stfu_header->signpost_tock_firmware_update_length) >=
            stfu_header->signpost_tock_firmware_update_source) {
            break;
        }

        // Now do the copy.
        copy_flash(stfu_header->signpost_tock_firmware_update_source,
                   stfu_header->signpost_tock_firmware_update_destination,
                   stfu_header->signpost_tock_firmware_update_length);

        // Now mark this as done
        uint8_t buffer[512];
        memcpy(buffer, (uint8_t*) 0x400, 512);
        // Zero out the enable word.
        buffer[492] = 0;
        buffer[493] = 0;
        buffer[494] = 0;
        buffer[495] = 0;
        write_page(buffer, 2);

        // Done!
    } while (0);

    jump_into_user_code();

}
