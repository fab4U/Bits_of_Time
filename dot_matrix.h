/*
 * dot_matrix.h
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

/*
 * Each pixel has two leds, red and green.
 * Each led has four brightness levels (off, 25% on, 50% on, 100% on)
 *
 * screen:
 * Array that contains the pixel columns which are of type pixcol_t.
 *
 * pixel column (pixcol_t):
 * A pixel column is a column of 8 bi-colored pixels.
 * The pixel column contains two 16-bit values - one for the MSBs and one
 * for the LSBs of the brightnesses of the corresponding leds.
 * The bits of each 16-bit value are assigned to the leds as follows:
 * bit 15 = bottom pixel red led
 * bit 14 = bottom pixel green led
 * ...
 * bit 1 = top pixel red led
 * bit 0 = top pixel green led
 *
 * The origin of the pixel coordinate system is in the upper left corner.
 */

#ifndef DOT_MATRIX_H_
#define DOT_MATRIX_H_


#include <inttypes.h>


/*************
 * constants *
 *************/

#define VISIBLE			0		// identifies the visible screen
#define HIDDEN			1		// identifies the hidden screen

// colors
// Each LED has 4 intensity levels (0 = 0 %, 1 = 25 %, 2 = 50 %, 3 = 100 %).
// A color is defined by 4 bits - 2 for the red led and 2 for the green.
// bit0 = green led, least significant intensity bit
// bit1 = green led, most  significant intensity bit
// bit2 = red   led, least significant intensity bit
// bit3 = red   led, most  significant intensity bit
#define RED				0b1100
#define LIGHTRED		0b1101
#define REDORANGE		0b1110
#define ORANGE			0b1111
#define LIGHTORANGE		0b1011
#define YELLOW			0b0111
#define GREEN			0b0011
#define MEDIUMRED		0b1000
#define MEDIUMORANGE	0b1010
#define MEDIUMGREEN		0b0010
#define DARKRED			0b0100
#define DARKORANGE		0b0101
#define DARKGREEN		0b0001
#define BLACK			0b0000

#define RED_MSB			0b1000
#define RED_LSB			0b0100
#define GREEN_MSB		0b0010
#define GREEN_LSB		0b0001

// display modes
#define OPAQUE			0	// black pixels are opaque
#define TRANSPARENT		1	// each black pixel is considered to be transparent
#define XOR				2	// xor pixels with background

// dot matrix display
#define NUM_BLOCKS_X		2		// number of PixBlocks in horizontal direction
#define NUM_BLOCKS_Y		1		// number of PixBlocks in vertical direction
//#define ENABLE_HIDDEN_SCREEN		// if defined two screens (visible & hidden) are implemented

#define COLS_PER_BLOCK		8		// number of columns per PixBlock (must be a power of 2)
#define ROWS_PER_BLOCK		8		// number of rows per PixBlock
#define NUM_BLOCKS			(NUM_BLOCKS_X * NUM_BLOCKS_Y)	// total number of PixBlocks
#define NUM_PIXCOLS			(NUM_BLOCKS   * COLS_PER_BLOCK)	// total number of pixel columns
#define DIM_X				(NUM_BLOCKS_X * COLS_PER_BLOCK)	// number of pixels in x direction
#define DIM_Y				(NUM_BLOCKS_Y * ROWS_PER_BLOCK)	// number of pixels in y direction
#define MAX_BRIGHTNESS		3		// maximum brightness level of a pixel

// display orientation
//#define DM_LSB_FIRST				// shift out led bits with LSB first
//#define DM_REVERSE_COLS			// reverse column order (from right to left)

// some character font defaults
#define DEFAULT_FONT		font_diagonal_ccw
#define DEFAULT_CHAR_BASE	CHAR_BASE_DIAGONAL_CCW	// character base of default font
#define DEFAULT_FONT_SIZE	( sizeof (DEFAULT_FONT) / sizeof (DEFAULT_FONT[0]) )
#define DEFAULT_COLOR		ORANGE

// memory types
#define	RAM			0
#define FLASH		1
#define EEPROM		2
//#define EXTERNAL		3	// not implemented

// hardware pins for accessing the dot matrix
#define DM_DATA_DDR			DDRB
#define DM_DATA_PORT		PORTB
#define DM_DATA_BIT			0
#define DM_CLK_DDR			DDRB
#define DM_CLK_PORT			PORTB
#define DM_CLK_BIT			1
#define DM_LATCH_DDR		DDRB
#define DM_LATCH_PORT		PORTB
#define DM_LATCH_BIT		2


/**************
 * data types *
 **************/

typedef struct {
	uint16_t lsb;
	uint16_t msb;
} pixcol_t;


/********
 * data *
 ********/

const uint16_t PROGMEM rainbow[16] = {
		0xEE20, 0xFA80, 0x7B88, 0xFEA0, 0xDEE2, 0x7FA8, 0x77B8, 0x5FEA,
		0x1DEE, 0x57FA, 0x477B, 0x15FE, 0x11DE, 0x057F, 0x0477, 0x015F
};


/********************
 * class definition *
 ********************/

class DotMatrix
{
public:
	static void init();
	static void clearScreen();
	static void selectScreen(uint8_t vis_hid);
	static void swapScreen();
	static void setOffset(const uint8_t col);
	static uint8_t displayText(const uint8_t x, const uint8_t y, const uint8_t mode, const char* st, const uint8_t src_mem_type, const uint16_t text_column, const uint8_t len);
	static void displayGraphics(const uint8_t x, const uint8_t y, const uint8_t mode, const uint16_t* graphics, const uint8_t src_mem_type,  const uint8_t len);
	static void pattern2PixCol(const uint8_t pix_data, const uint8_t color, pixcol_t* pc);
	static void setPixCol(const uint8_t x, const uint8_t y, const pixcol_t* pc, const uint8_t mode);
	static void setPixel(uint8_t x, uint8_t y, const uint8_t color);
	static uint8_t getPixel(uint8_t x, uint8_t y, const uint8_t vis_hid);
	static void displayLogo();
	static void update();

private:
	static uint8_t offset;			// screen offset
#ifdef ENABLE_HIDDEN_SCREEN
	static pixcol_t screen[NUM_BLOCKS * COLS_PER_BLOCK * 2];
#else
	static pixcol_t screen[NUM_BLOCKS * COLS_PER_BLOCK];
#endif
	static pixcol_t* scr_vis;		// visible screen
	static pixcol_t* scr_hid;		// hidden screen
	static pixcol_t* scr_wrk;		// working screen
	static uint8_t column;			// current column number (0..7)
	static uint8_t bright_cnt;		// brightness counter
	static uint8_t color;			// current text color

	static void shift_out(uint16_t data);
	static uint8_t readChar(const char* ptr, const uint8_t src_mem_type);
};



#endif /* DOT_MATRIX_H_ */
