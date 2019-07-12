/*
 * fonts.h
 *
 */

/**********************************************************************************

Description:		List of PixBlock fonts

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
 *  How to add another font
 *
 *  1. Generate a new header file named 'font_<your font name>.h'.
 *     The file must contain
 *     - the pixel data for each character,
 *     - an array with pointers to all characters (use UNDEF for a
 *       character that is not defined),
 *     - the character code of the first character (CHAR_BASE_<your font name>)
 *     It is best to use an existing font file as an example.
 *
 *  2. Include the font file in this header file (see below "font files").
 *
 *  3. Add the font to fonttable.
 *
 *  4. Under font parameters enter the code of the first character and total
 *     number of characters of your new font (including undefined characters).
 *
 */

#ifndef FONTS_H_
#define FONTS_H_

#include <avr/pgmspace.h>

#define FLASHDATA	const unsigned char PROGMEM

#define NUMBER_OF_FONTS		( sizeof (fonttable) / sizeof (fonttable[0]) )
							// Note: Total number of fonts must be <= 7.


// undefined character (pattern for an undefined character)
FLASHDATA undefined_char[] = {0x01, 0x00};	//
#define UNDEF	undefined_char


// font files
#include "font_diagonal_ccw.h"
#include "font_diagonal_cw.h"


// table of available fonts
// Note: Total number of fonts must be <= 7.

FLASHDATA* const* const fonttable[] = {
		font_diagonal_ccw,
		font_diagonal_cw
};


// font parameters
// code of first character and total number of characters in each font
const unsigned char PROGMEM fontparams[NUMBER_OF_FONTS * 2] = {
		CHAR_BASE_DIAGONAL_CCW, sizeof (font_diagonal_ccw) / sizeof (font_diagonal_ccw[0]),
		CHAR_BASE_DIAGONAL_CW,  sizeof (font_diagonal_cw)  / sizeof (font_diagonal_cw[0])
};


#endif /* FONTS_H_ */
