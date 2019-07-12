/*
 * dot_matrix.cpp
 *
 *  Created on: 28.02.2015
 *      Author: Frank Andre
 */

/**********************************************************************************

Description:		Class for operating the PixBlock dot matrix display.
					(see www.fab4U.de for details)

Author:				Frank Andre
Copyright 2015:		Frank Andre
License:			see "license.md"
Disclaimer:			This software is provided by the copyright holder "as is" and any
					express or implied warranties, including, but not limited to, the
					implied warranties of merchantability and fitness for a particular
					purpose are disclaimed. In no event shall the copyright owner or
					contributors be liable for any direct, indirect, incidental,
					special, exemplary, or consequential damages (including, but not
					limited to, procurement of substitute goods or services; loss of
					use, data, or profits; or business interruption) however caused
					and on any theory of liability, whether in contract, strict
					liability, or tort (including negligence or otherwise) arising
					in any way out of the use of this software, even if advised of
					the possibility of such damage.

**********************************************************************************/

#include <inttypes.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <util/delay.h>

#include "dot_matrix.h"
#include "fonts.h"

#ifdef EEPROM
#include <avr/eeprom.h>
#endif


/**************************
 * static class variables *
 **************************/

#ifdef ENABLE_HIDDEN_SCREEN
pixcol_t	DotMatrix::screen[NUM_BLOCKS * COLS_PER_BLOCK * 2];
#else
pixcol_t	DotMatrix::screen[NUM_BLOCKS * COLS_PER_BLOCK];
#endif
pixcol_t*	DotMatrix::scr_vis;			// visible screen
pixcol_t*	DotMatrix::scr_hid;			// hidden screen
pixcol_t*	DotMatrix::scr_wrk;			// working screen
uint8_t		DotMatrix::offset;			// screen offset
uint8_t		DotMatrix::column;			// current column number
uint8_t		DotMatrix::bright_cnt;		// current brightness level
uint8_t		DotMatrix::color;			// current color


/********
 * data *
 ********/

const uint16_t PROGMEM pixcol_mask[8] = {
	0x0001, 0x0004, 0x0010, 0x0040, 0x0100, 0x0400, 0x1000, 0x4000
};

const char PROGMEM logo_string[] = ("\n\x01\x1C" "Pix" "\x17" "Block" "\x13" "fab" "\x1F" "4" "\x13" "U ");


/***********
 * methods *
 ***********/

void DotMatrix::displayLogo()
{
	if (NUM_BLOCKS_Y == 1) {
		displayGraphics(0, 0, OPAQUE, rainbow, FLASH, 8);
		displayText(10,  1, OPAQUE, logo_string, FLASH,  0, 36);
		displayText(49,  1, OPAQUE, logo_string, FLASH, 37,  4);
		displayText(52,  1, OPAQUE, logo_string, FLASH, 42, 19);
		setPixel(52, 1, GREEN);
		displayGraphics(72, 0, OPAQUE, rainbow, FLASH, 8);
	}
	else if (NUM_BLOCKS_Y == 2) {
		displayGraphics(0, 0, OPAQUE, rainbow, FLASH, 8);
		displayText( 2,  1, OPAQUE, logo_string, FLASH,  0, 36);
		displayText(13,  9, OPAQUE, logo_string, FLASH, 37,  4);
		displayText(16,  9, OPAQUE, logo_string, FLASH, 42, 19);
		setPixel(16, 9, GREEN);
	}
	else {
		displayGraphics(0, 0, OPAQUE, rainbow, FLASH, 8);
		displayText(11,  1, OPAQUE, logo_string, FLASH,  0, 12);
		displayText( 0,  9, OPAQUE, logo_string, FLASH, 13, 24);
		displayText( 1, 17, OPAQUE, logo_string, FLASH, 37,  4);
		displayText( 4, 17, OPAQUE, logo_string, FLASH, 42, 19);
		setPixel(4, 17, GREEN);
	}
}


void DotMatrix::init()
// The parameters define the arrangement of PixBlocks.
{
	DM_DATA_PORT  &= ~(1 << DM_DATA_BIT);
	DM_CLK_PORT   &= ~(1 << DM_CLK_BIT);
	DM_LATCH_PORT &= ~(1 << DM_LATCH_BIT);
	DM_DATA_DDR   |=  (1 << DM_DATA_BIT);
	DM_CLK_DDR    |=  (1 << DM_CLK_BIT);
	DM_LATCH_DDR  |=  (1 << DM_LATCH_BIT);

#ifdef ENABLE_HIDDEN_SCREEN
	// with hidden screen
	scr_hid = &screen[NUM_BLOCKS * COLS_PER_BLOCK];
	selectScreen(HIDDEN);
	clearScreen();
#else
	// no hidden screen
	scr_hid = &screen[0];	// hidden screen is identical to visible screen
#endif

	scr_vis = &screen[0];
	selectScreen(VISIBLE);
	clearScreen();
	column = 0;
	bright_cnt = MAX_BRIGHTNESS;
	color = DEFAULT_COLOR;
}


void DotMatrix::setOffset(const uint8_t col)
// set the screen offset (range 0..NUM_PIXCOLS-1)
// The offset value determines which column of the screen is displayed
// in the leftmost column of the (leftmost) PixBlock.
{
	if (col < NUM_PIXCOLS) {
		ATOMIC_BLOCK(ATOMIC_FORCEON) {
			offset = col;
		}
	}
}


void DotMatrix::shift_out(uint16_t data)
// shift out the data
{
	for (uint8_t i = 0; i < 16; i++) {
#ifdef DM_LSB_FIRST
		if (data & 0x0001) { DM_DATA_PORT |= (1 << DM_DATA_BIT); }
		else { DM_DATA_PORT &= ~(1 << DM_DATA_BIT); }
		data >>= 1;
#else
		if (data & 0x8000) { DM_DATA_PORT |= (1 << DM_DATA_BIT); }
		else { DM_DATA_PORT &= ~(1 << DM_DATA_BIT); }
		data <<= 1;
#endif
		// clock, rising egde
		DM_CLK_PORT &= ~(1 << DM_CLK_BIT);
		DM_CLK_PORT |=  (1 << DM_CLK_BIT);
	}
}

void DotMatrix::update()
// Update one column on all PixBlock displays.
// Should be called periodically.
{
	pixcol_t*	scr;
	uint16_t	br_msb, br_lsb;
	uint8_t		b;
	uint8_t		c;			// index of pixel column which is to be shifted out

	if (column == 0) {
		bright_cnt--;
		bright_cnt &= MAX_BRIGHTNESS;	// limit range
	}

	// start with last (rightmost) PixBlock which has to be shifted out first
	c = NUM_PIXCOLS - COLS_PER_BLOCK;
	c +=  offset;								// add offset
	if (c >= NUM_PIXCOLS) { c -= NUM_PIXCOLS; }	// on overflow -> wrap around
#ifdef DM_REVERSE_COLS
	c += (COLS_PER_BLOCK-1) - column;
#else
	c += column;
#endif
	for (b = 0; b < NUM_BLOCKS; b++) {
		scr = &(scr_vis[c]);
		br_msb = scr->msb;
		br_lsb = scr->lsb;
		c -= COLS_PER_BLOCK;
		if (c >= NUM_PIXCOLS) { c += NUM_PIXCOLS; }	// on underflow -> wrap around
		if (bright_cnt & 1) { br_msb = br_msb & br_lsb; }	// bright_cnt == 1 or 3
		//else if (bright_cnt == 2) { do nothing }
		else if (bright_cnt == 0) { br_msb = br_msb | br_lsb; }

		shift_out(br_msb);
	}

	// set final state of clock pin
	// used for column sync (low -> display column 0)
	if (column == 0) {
		DM_CLK_PORT &= ~(1 << DM_CLK_BIT);
	}

	// pulse latch signal
	DM_LATCH_PORT |=  (1 << DM_LATCH_BIT);
	_delay_us(1);
	DM_LATCH_PORT &= ~(1 << DM_LATCH_BIT);

	// next column
	column++;
	column &= COLS_PER_BLOCK - 1;		// limit column range
}


void DotMatrix::clearScreen()
{
	uint8_t i;

	for (i = 0; i < NUM_PIXCOLS; i++) {
		scr_wrk[i].msb = 0;
		scr_wrk[i].lsb = 0;
	}
}


void DotMatrix::selectScreen(uint8_t vis_hid)
// Select working screen for pixel operations (e. g. setPixel, displayText, ...).
{
	if (vis_hid == VISIBLE) { scr_wrk = scr_vis;  return; }
	if (vis_hid == HIDDEN)  { scr_wrk = scr_hid; }
}


void DotMatrix::swapScreen()
// Exchange visible and hidden screen (if enabled).
// Working screen is unaffected by this method, i. e. if you have been working
// on the hidden screen future operations will still be working on the hidden screen.
{
	pixcol_t *temp_screen;

	temp_screen = scr_vis;
	scr_vis = scr_hid;
	scr_hid = temp_screen;
	if (scr_wrk == scr_vis)	{ scr_wrk = scr_hid; }
	else					{ scr_wrk = scr_vis; }
}


uint8_t DotMatrix::displayText(const uint8_t x, const uint8_t y, const uint8_t mode, const char* st, const uint8_t src_mem_type, const uint16_t text_column, const uint8_t len)
// Copy the pixel patterns of the ticker text into the working screen,
// starting at the given text column with a length of len columns.
// Return 1 if end of ticker text has been reached.
// Character codes 1..NUMBER_OF_FONTS change the font of the following characters.
// Character code 16 toggles inverse character rendering.
// Character codes 17..32 change the color of the following characters.
// For pixel coordinates origin is the upper left corner.
{
	uint16_t	tc = 0;			// text column
	uint8_t		sc, sc_end;		// screen column
	uint8_t		ch;				// ticker text character
	uint8_t		w = 0;			// character width
	uint8_t		invert = 0;		// inversion flag
	uint8_t		color = DEFAULT_COLOR;					// current text color
	uint8_t		char_base    = DEFAULT_CHAR_BASE;		// character code of first character in font
	uint8_t		num_of_chars = DEFAULT_FONT_SIZE;		// number of characters in current font
	const unsigned char* const*	font = DEFAULT_FONT;	// pointer to current font
	const unsigned char*		p;						// font parameter
	pixcol_t	pixcol;

	sc = x;
	if ((DIM_X - sc) >= len) {
		sc_end = sc + len;								// screen column one after the last column
	}
	else {
		sc_end = DIM_X;
	}

	while (sc < sc_end) {
		if (w == 0) {									// if remaining character width is 0
			ch = readChar(st++, src_mem_type);			// get next character
			if (ch < 32) {
				if (ch == 0) { return(1); }					// end of text string
				if (ch <= NUMBER_OF_FONTS) {			// switch font command?
					ch--;								// ch contains number of new font
					font = (const unsigned char* const*) pgm_read_word(&fonttable[ch]);	// change font
					p = &fontparams[2 * ch];
					char_base    = pgm_read_byte(p++);
					num_of_chars = pgm_read_byte(p);
				}
				else if (ch >= 16) {					// change color command?
					if (ch == 16) { invert = ~invert; }
					else { color = ch & 0xF; }			// change color
				}
				continue;
			}
			if (ch < char_base) { continue; }			// character code out of range
			ch -= char_base;
			if (ch >= num_of_chars) { continue; }		// character code out of range
			p = (const unsigned char*) pgm_read_word(&font[ch]);	// get pointer to pixel data
			w = pgm_read_byte(p++);						// get character width
		}
		if (tc >= text_column) {						// starting column reached ?
			ch = pgm_read_byte(p++);					// yes -> get pixel data
			if (invert) { ch = ~ch; }					// invert pixel data
			pattern2PixCol(ch, color, &pixcol);			// convert pixel data
			setPixCol(sc, y, &pixcol, mode);			// and write to screen
			sc++;
			w--;
		}
		else {
			if (text_column >= (tc + w)) {				// no -> skip
				tc += w;
				w = 0;									// skip whole character
			}
			else {
				tc++;									// skip column
				p++;
				w--;
			}

		}
	}
	return(0);
}


uint8_t DotMatrix::readChar(const char* ptr, const uint8_t src_mem_type)
{
	if (src_mem_type == RAM)		{ return( *ptr ); }					// read byte from RAM
	else if (src_mem_type == FLASH)	{ return( pgm_read_byte(ptr) ); }	// read byte from FLASH
#ifdef EEPROM
	else if (src_mem_type == EEPROM){ return( eeprom_read_byte((const unsigned char*)ptr) ); }	// read byte from EEPROM
#endif
#ifdef EXTERNAL
	else if (src_mem_type == EXTERNAL){ return(0) ); }	// read byte from external memory
#endif
	else { return(0); }
}


void DotMatrix::displayGraphics(const uint8_t x, const uint8_t y, const uint8_t mode, const uint16_t* graphics, const uint8_t src_mem_type,  const uint8_t len)
// Display a graphics block on screen (origin = upper left corner).
// The graphics block consists of <len> pixel columns.
// Each pixel column is stored as two consecutive 16-bit values representing lsb and msb.
// ptr points to the data and src_mem_type specifies in which kind of memory (RAM, FLASH, EEPROM)
// the data is stored.

{
	uint8_t		i;
	pixcol_t	pc;

	for (i = 0; i < len; i++) {
		if (src_mem_type == RAM) {			// read from RAM
			pc.lsb = *graphics++;
			pc.msb = *graphics++;
		}
		else if (src_mem_type == FLASH) {	// read from FLASH
			pc.lsb = pgm_read_word((uint16_t*)graphics++);
			pc.msb = pgm_read_word((uint16_t*)graphics++);
		}
	#ifdef EEPROM
		else if (src_mem_type == EEPROM) {	// read from EEPROM
			pc.lsb = eeprom_read_word((uint16_t*)graphics++);
			pc.msb = eeprom_read_word((uint16_t*)graphics++);
			pc.lsb = 0;  pc.msb = 0;		// replace this line with implementation
		}
	#endif
	#ifdef EXTERNAL
		else if (src_mem_type == EXTERNAL) {	// read from external memory
			pc.lsb = 0;  pc.msb = 0;		// replace this line with implementation
		}
	#endif
		setPixCol(x + i, y, &pc, mode);
	}
}


void DotMatrix::pattern2PixCol(const uint8_t pix_data, const uint8_t color, pixcol_t* pc)
// Transform the 8 pixels in pix_data to a pixel column with given color.
{
	typedef union {
		uint32_t u32;
		struct {
			uint8_t  u8lo;
			uint16_t u16;
			uint8_t  u8hi;
		};
	} shifter_t;

	shifter_t	sh;
	uint16_t	temp;
	uint8_t		i;

	// spread pix_data, i. e. insert a 0 to the right of every bit
	// e. g. 11011111 -> 1010001010101010
	sh.u32 = pix_data;
	for (i = 0; i < 8; i++) {
		sh.u32 <<= 1;
		sh.u16 <<= 1;
	}

	// set most significant color bits
	if (color & 0b0010) { temp =  sh.u16 >> 1; }
	else { temp = 0; }
	if (color & 0b1000) { temp |= sh.u16; }
	pc->msb = temp;

	// set least significant color bits
	if (color & 0b0001) { temp =  sh.u16 >> 1; }
	else { temp = 0; }
	if (color & 0b0100) { temp |= sh.u16; }
	pc->lsb = temp;
}


void DotMatrix::setPixCol(const uint8_t x, const uint8_t y, const pixcol_t* pc, const uint8_t mode)
// Write a pixel column (8 bicolor-pixels) at the given position
// in the working screen.
// origin (0, 0) = upper left corner
{
	typedef union {
		uint32_t u32;
		struct {
			uint16_t lo;
			uint16_t hi;
		};
	} split32_t;

	uint8_t		idx;			// index to screen
	uint8_t		yr;
	uint8_t		i;
	split32_t	mask, pixel_lsb, pixel_msb;

	if (x >= DIM_X) { return; }
	if (y >= DIM_Y) { return; }
	idx = x + (y / ROWS_PER_BLOCK) * DIM_X;			// calculate index to screen
	yr = y & (ROWS_PER_BLOCK - 1);					// remainder of y coordinate

	if (mode == TRANSPARENT) {						// calculate mask
		mask.lo = (pc->lsb) | (pc->msb);
		mask.lo |= ((mask.lo >> 1) & 0x5555);		// or-ing red and green bits
		mask.lo |= (mask.lo << 1);
	}
	else {
		mask.lo = 0xFFFF;
	}

	pixel_lsb.lo = pc->lsb;
	pixel_msb.lo = pc->msb;
	for (i = 0; i < (yr * 2); i++) {				// shift mask and pixel data
		mask.u32      <<= 1;
		pixel_lsb.u32 <<= 1;
		pixel_msb.u32 <<= 1;
	}
	mask.u32 = ~mask.u32;

	if (mode == XOR) {
		scr_wrk[idx].lsb ^= pixel_lsb.lo;			// set new pixels
		scr_wrk[idx].msb ^= pixel_msb.lo;
	}
	else {
		scr_wrk[idx].lsb &= mask.lo;				// clear pixels that are overwritten
		scr_wrk[idx].msb &= mask.lo;
		scr_wrk[idx].lsb |= pixel_lsb.lo;			// set new pixels
		scr_wrk[idx].msb |= pixel_msb.lo;
	}

	if (yr) {
		idx += DIM_X;
		if (idx >= (NUM_BLOCKS * COLS_PER_BLOCK)) { return; }
		if (mode == XOR) {
			scr_wrk[idx].lsb ^= pixel_lsb.hi;		// set new pixels
			scr_wrk[idx].msb ^= pixel_msb.hi;
		}
		else {
			scr_wrk[idx].lsb &= mask.hi;			// clear pixels that are overwritten
			scr_wrk[idx].msb &= mask.hi;
			scr_wrk[idx].lsb |= pixel_lsb.hi;		// set new pixels
			scr_wrk[idx].msb |= pixel_msb.hi;
		}
	}
}


void DotMatrix::setPixel(uint8_t x, uint8_t y, const uint8_t color)
// set pixel in working screen
{
	uint16_t	mask_red, mask_green, pix;

	mask_green = pgm_read_word(&pixcol_mask[y & (ROWS_PER_BLOCK - 1)]);
	mask_red = mask_green << 1;

	if (x >= DIM_X) { return; }
	if (y >= DIM_Y) { return; }
	y /= ROWS_PER_BLOCK;
	x = x + y * DIM_X;					// calculate index to screen

	// set most significant color bits
	pix = scr_wrk[x].msb & ~(mask_red | mask_green);	// clear pixel
	if (color & 0b0010) { pix |=  mask_green; }
	if (color & 0b1000) { pix |=  mask_red; }
	scr_wrk[x].msb = pix;

	// set least significant color bits
	pix = scr_wrk[x].lsb & ~(mask_red | mask_green);	// clear pixel
	if (color & 0b0001) { pix |=  mask_green; }
	if (color & 0b0100) { pix |=  mask_red; }
	scr_wrk[x].lsb = pix;
}


uint8_t DotMatrix::getPixel(uint8_t x, uint8_t y, const uint8_t vis_hid)
// return color of specified pixel
// return 255 if pixel coordinates are out of range
{
	uint16_t	mask_red, mask_green;
	uint8_t		color;
	pixcol_t*	scr;

	mask_green = pgm_read_word(&pixcol_mask[y & (ROWS_PER_BLOCK - 1)]);
	mask_red = mask_green << 1;

	if (x >= DIM_X) { return(255); }
	if (y >= DIM_Y) { return(255); }
	y /= ROWS_PER_BLOCK;
	x = x + y * DIM_X;					// calculate index to screen

	if (vis_hid == VISIBLE)		{ scr = scr_vis; }
	else if (vis_hid == HIDDEN)	{ scr = scr_hid; }
	else 						{ return(255); }
	color = 0;

	// get most significant color bits
	if (scr[x].msb & mask_red)   { color |=  RED_MSB; }
	if (scr[x].msb & mask_green) { color |=  GREEN_MSB; }

	// get least significant color bits
	if (scr[x].lsb & mask_red)   { color |=  RED_LSB; }
	if (scr[x].lsb & mask_green) { color |=  GREEN_LSB; }

	return(color);
}


