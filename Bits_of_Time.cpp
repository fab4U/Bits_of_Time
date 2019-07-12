/*
 * Bits_of_Time.cpp
 *
 */ 

/**********************************************************************************

Description:		An electronic sandglass
					ATtiny84A processor running at 8 MHz

					Connect inclination switch to pin INCL_PIN with the outer
					metal can connected to GND. Optional: Connect a capacitor
					of 1 µF in parallel to the inclination switch to reduce
					sensitivity to vibrations.
					
					When the first PixBlock is in the upper position, the
					inclination sensor input should be high.

					Pixel(0, 0) of the first PixBlock should be the uppermost
					pixel.

					Push button 1 adjusts minutes.
					Push button 2 adjusts quarter minutes.
					Push button 3 restarts the hourglass.

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
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
//#include <avr/sleep.h>
#include <util/atomic.h>

#include "dot_matrix.h"


/*************
 * constants *
 *************/

// total number of grains
// If you change this parameter adapt function fill_bulb().
#define GRAINS_TOTAL		54

// cycle times for dot-matrix display
#define DM_PRESCALER		2			// prescaler = 1:8 (do not change)
#define DM_REFRESH_FREQ		2500		// dot matrix column refresh rate (Hz)
										// If you change this value adapt function wait().
#define DM_REFRESH			(uint16_t)(0.5 + F_CPU / (8.0 * DM_REFRESH_FREQ))

// simulation parameters
#define SIM_SPEED			10			// default simulation speed
#define CALIBRATION			1.0			// time calibration factor
#define DROP_CYCLE(time)	(uint16_t)(0.5 + (float)DM_REFRESH_FREQ * (float)(time) * CALIBRATION / (float)GRAINS_TOTAL)
// 'time' specifies how long it takes for the sand to trickle to the lower bulb.

// maximum number of minutes (range 0..9)
#define MAX_MINUTES			5

//#define QUARTER_OVERFLOWS_INTO_MINUTES

// inclination sensor
#define INCL_PIN			PA3

// PWM output (OC0B = PA7)
#define PWM_PIN				PA7			// do not change
#define NO_PWM				0			// do not change
#define NON_INVERTING		2			// do not change
#define INVERTING			3			// do not change
#define PWM_MODE			NON_INVERTING
#define PWM_PRESCALER		4			// range 1..5
#if PWM_PRESCALER == 1
#define PWM_PRESC_FACTOR	1.0			// PWM freqency = 15.7 kHz @ F_CPU = 8 MHz
#elif PWM_PRESCALER == 2
#define PWM_PRESC_FACTOR	8.0			// PWM freqency = 1.96 kHz
#elif PWM_PRESCALER == 3
#define PWM_PRESC_FACTOR	64.0		// PWM freqency = 245.1 Hz
#elif PWM_PRESCALER == 4
#define PWM_PRESC_FACTOR	256.0		// PWM freqency = 61.3 Hz
#elif PWM_PRESCALER == 5
#define PWM_PRESC_FACTOR	1024.0		// PWM freqency = 15.3 Hz
#else
#error "bad PWM_PRESCALER setting"
#endif
#define	PWM_FREQ		(F_CPU / (PWM_PRESC_FACTOR * 510.0))
#define PWM_OFF			0				// duty cycle =  0 %
#define PWM_25			64				// duty cycle = 25 %
#define PWM_50			128				// duty cycle = 50 %
#define PWM_75			192				// duty cycle = 75 %
#define PWM_ON			255				// duty cycle =100 %
#define SERVO_LEFT		8				// on-time = 0.5 ms @ PWM_PRESCALER = 4
#define SERVO_MIDDLE	23				// on-time = 1.5 ms @ PWM_PRESCALER = 4
#define SERVO_RIGHT		39				// on-time = 2.5 ms @ PWM_PRESCALER = 4

// gravity states
#define DOWN			0
#define UP				1

// hourglass bulbs
#define UPPER			0
#define LOWER			1

// modes of operation
#define SET				0
#define RUN				1
#define ALARM			2

// pseudo random number generator
#define POLYNOMIAL		0b00001110		// feedback polynomial
#define RANDOM_SEED		120

// processor clock
#ifndef F_CPU
#define F_CPU	8000000		// 8 MHz
#endif


FUSES =
{
	0xE2, // .low
	0xDF, // .high
	0xFF // .extended
};


/********************
 * global variables *
 ********************/

volatile uint16_t	timer;					// software timer
DotMatrix			dm;
char				screen[10] = "         ";
uint8_t				gravity ;				// direction of gravity
uint8_t				sensor;					// state of inclination sensor
uint16_t			sim_speed = SIM_SPEED;	// simulation speed

// time presets (in seconds)
// = time it takes for the sand to trickle to the lower bulb
const uint16_t PROGMEM times[] = {
		DROP_CYCLE( 10.0), DROP_CYCLE( 15.0), DROP_CYCLE( 30.0), DROP_CYCLE( 45.0),
		DROP_CYCLE( 60.0), DROP_CYCLE( 75.0), DROP_CYCLE( 90.0), DROP_CYCLE(105.0),
		DROP_CYCLE(120.0), DROP_CYCLE(135.0), DROP_CYCLE(150.0), DROP_CYCLE(165.0),
		DROP_CYCLE(180.0), DROP_CYCLE(195.0), DROP_CYCLE(210.0), DROP_CYCLE(225.0),
		DROP_CYCLE(240.0), DROP_CYCLE(255.0), DROP_CYCLE(270.0), DROP_CYCLE(285.0),
		DROP_CYCLE(300.0), DROP_CYCLE(315.0), DROP_CYCLE(330.0), DROP_CYCLE(345.0),
		DROP_CYCLE(360.0), DROP_CYCLE(375.0), DROP_CYCLE(390.0), DROP_CYCLE(405.0),
		DROP_CYCLE(420.0), DROP_CYCLE(435.0), DROP_CYCLE(450.0), DROP_CYCLE(465.0),
		DROP_CYCLE(480.0), DROP_CYCLE(495.0), DROP_CYCLE(510.0), DROP_CYCLE(525.0),
		DROP_CYCLE(540.0), DROP_CYCLE(555.0), DROP_CYCLE(570.0), DROP_CYCLE(585.0)
};

uint8_t ee_time_setting[2] EEMEM = {0, 2};


/*************
 * functions *
 *************/

void init_hardware()
{
	// i/o ports
	PORTA = 0x0F;					// use pull-ups on PA0..3
	DDRA  = (1 << PWM_PIN);			// make PWM_PIN an output

	// use timer1 as system time base running at 1 MHz
	OCR1A  = DM_REFRESH;			// set dot matrix refresh time
	OCR1B  = 0;
	TCCR1A = (PWM_MODE << COM1B0);	// normal mode
	TCCR1B = (DM_PRESCALER << CS10);
	TIMSK1 = (1 << OCIE1A);

#if PWM_MODE > 0
	// use timer0 for PWM output on OC0B
	OCR0B  = 0;
	TCCR0A = (PWM_MODE << COM0B0)|(1 << WGM00);	// phase correct PWM mode 1
	TCCR0B = (0 << WGM02)|(PWM_PRESCALER << CS00);
#endif
}


void set_pwm_output(uint8_t pwm)
{
#if PWM_MODE > 0		// PWM enabled ?
	OCR0B = pwm;
#else					// normal port operation
	if (pwm)	{ PORTA |=  (1 << PA5); }
	else		{ PORTA &= ~(1 << PA5); }
#endif
}


void wait(uint16_t ms)
// Wait for the specified numer of milliseconds.
// Global variable 'timer' is incremented at a rate defined by DM_REFRESH_FREQ.
{
	uint16_t alarm;

// This is the general case:
//	alarm = timer + (uint16_t)(0.5 + ms * (float)DM_REFRESH_FREQ / 1000.0);

// Instead of all the floating point arithmetic we choose a faster implementation
// which is only valid for DM_REFRESH_FREQ = 2500 Hz.
	alarm = timer + (ms << 1) + (ms >> 1);	// = timer + 2.5 * ms
	while (timer != alarm);					// wait on alarm
}


uint8_t get_pixel(uint8_t x, uint8_t y, uint8_t bulb)
// Return pixel color.
// 'bulb' denotes upper or lower part of the hourglass.
{
	if ((x > 7) || (y > 7)) { return(255); }

	if (gravity == UP) {
		x = 7 - x;
		y = 7 - y;
	}
	if (bulb ^ gravity) { x += 8; }
	return( dm.getPixel(x, y, VISIBLE) );
}


void set_pixel(uint8_t x, uint8_t y, uint8_t bulb, const uint8_t color)
// Set pixel to desired color.
// 'bulb' denotes upper or lower part of the hourglass.
{
	if ((x > 7) || (y > 7)) { return; }

	if (gravity == UP) {
		x = 7 - x;
		y = 7 - y;
	}
	if (bulb ^ gravity) { x += 8; }
	dm.setPixel(x, y, color);
}


uint8_t random(uint8_t seed)
// Generate a pseudo-random number.
// If 'seed' > 0 this number is taken as a new seed.
// If 'seed' = 0 simply return the next number in the sequence.
// By principle the returned number can never be zero.
{
	static uint8_t	rnd = RANDOM_SEED;

	if (seed) { rnd = seed; }		// if seed > 0 set new seed

	if (rnd & 0x80) {				// MSB = 1 ?
		rnd <<= 1;
		rnd ^= ((POLYNOMIAL << 1) | 1);
	}
	else {
		rnd <<= 1;
	}
	return(rnd);
}


void fill_bulb(uint8_t bulb)
// Fill specified bulb with 54 grains of sand.
{
	uint8_t x, y;

	for (y = 0; y < 8; y++) {
		for (x = 0; x < 8; x++) {
			if ((x + y) > 3) {
				set_pixel(x, y, bulb, YELLOW);
				wait(8);
			}
		}
	}
}


uint8_t move_grain(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t bulb)
// If position2 is empty move grain from position1 to position2.
// Grains that have moved become orange.
{
	if (get_pixel(x2, y2, bulb) == BLACK) {		// check if target position is empty
		set_pixel(x1, y1, bulb, BLACK);			// remove grain from old position
		wait(sim_speed);
		set_pixel(x2, y2, bulb, ORANGE);		// set grain to new position
		wait(sim_speed);
		return(1);
	}
	return(0);
}


uint8_t simulate_grain(uint8_t x, uint8_t y, uint8_t bulb)
// If possible move specified grain of sand according to gravity.
// Return 255 if hourglass is at rest.
// 'bulb' denotes upper or lower part of the hourglass.
{
	static uint8_t	side = 0;	// decides which side, left or right, is checked first
	static uint8_t	static_counter = 0;

	x &= 0x07;  y &= 0x07;		// limit coordinate range to one bulb (0..7, 0..7)

	if (get_pixel(x, y, bulb) == BLACK) {			// no grain -> nothing to do
		static_counter++;
		return(static_counter);
	}

	if ( move_grain(x, y, x + 1, y + 1, bulb) ) {	// fall straight down
		static_counter = 0;		// reset counter
		return(static_counter);
	}

	if (side) {
		side ^= 1;		// change side
		if ( move_grain(x, y, x + 1, y, bulb) ) {
			static_counter = 0;		// reset counter
			return(static_counter);
		}
		if ( move_grain(x, y, x, y + 1, bulb) ) {
			static_counter = 0;		// reset counter
			return(static_counter);
		}
	}
	else {
		side ^= 1;		// change side
		if ( move_grain(x, y, x, y + 1, bulb) ) {
			static_counter = 0;		// reset counter
			return(static_counter);
		}
		if ( move_grain(x, y, x + 1, y, bulb) ) {
			static_counter = 0;		// reset counter
			return(static_counter);
		}
	}
	if (bulb == LOWER) { static_counter++; }
	else { static_counter = 0; }	// hourglass is not at rest if there is a grain in the upper bulb
	set_pixel(x, y, bulb, YELLOW);	// turn grain yellow if it is at rest
	return(static_counter);
}


void drop()
// Move grain of sand from upper to lower bulb.
{
	if (get_pixel(7, 7, UPPER) == BLACK) { return; }	// no grain -> no drop
	if (get_pixel(0, 0, LOWER) == BLACK) {	// grain may only drop if target position
											// is empty
		set_pixel(7, 7, UPPER, BLACK);
		set_pixel(0, 0, LOWER, ORANGE);
	}
}


void reset_hour_glass()
// Clear hour glass and fill upper bulb.
{
	dm.clearScreen();
	fill_bulb(UPPER);
}


uint8_t sense_gravity()
// If sensor input is high gravity is pointing DOWNwards.
// Return 1 if gravity has changed, otherwise 0.
{
	uint8_t			sensor_new;

	sensor_new = PINA & (1 << INCL_PIN);	// read inclination switch
	if (sensor_new == sensor) { return(0); }

	// hourglass was turned
	if (sensor_new) {						// sensor input = high
		gravity = DOWN;
	} else {								// sensor input = low
		gravity = UP;
	}
	sensor = sensor_new;
	return(1);
}


uint16_t get_drop_cycle(uint8_t m, uint8_t q)
// Read drop cycle time from table 'times'.
{
	uint16_t time;
	time = pgm_read_word(&times[(m << 2) + q]);
	sim_speed = SIM_SPEED;
	if (time < DROP_CYCLE( 30.0)) {
		sim_speed = SIM_SPEED >> 1;		// double simulation speed
	}
	if (time < DROP_CYCLE( 15.0)) {
		sim_speed = SIM_SPEED >> 1;		// double simulation speed
	}
	return(time);
}


void display_time_setting(uint8_t m, uint8_t q, uint8_t gravity)
// m is displayed as a number and q as a quarter mark
{
	if (m > 9) { m = 9; }		// limit range to 0..9
	q &= 0x03;					// limit range to 0..3

	screen[0] = 0x01;			// select font1 (diagonal_ccw)
//	screen[0] = 0x02;			// select font2 (diagonal_cw)
	screen[1] = 0x10 | GREEN;

	if (gravity == DOWN) {
		m += 48;
		q += 58;
		screen[1] = 0x10 | GREEN;
		screen[2] = m;
		screen[3] = 0x10 | MEDIUMRED;
		screen[4] = q;
	}
	else {
		m += 34;
		q += 44;
		screen[1] = 0x10 | MEDIUMRED;
		screen[2] = q;
		screen[3] = 0x10 | GREEN;
		screen[4] = m;
	}
	screen[5] = 0;				// terminating zero
}


uint8_t upper_bulb_empty()
// Check if all grains of sand have left the upper bulb.
{
	uint8_t x, y;

	for (y = 0; y < 8; y++) {
		for (x = 0; x < 8; x++) {
			if (get_pixel(x, y, UPPER) != BLACK) {
				return(0);
			}
		}
	}
	return(1);
}


void alarm_signal()
// This routine defines what happens if time has elapsed.
{
	wait(500);
	dm.clearScreen();
	screen[0] = 1;
	screen[1] = 0x10 | ORANGE;
	screen[2] = ' ';
	screen[3] = ' ';
	screen[4] = 0;
}


/********
 * main *
 ********/

int main(void)
{
	uint8_t		minute;
	uint8_t		quarter;
	uint8_t		mode;
	uint16_t	drop_cycle;
	uint8_t 	r;
	uint16_t	last_drop;		// time of last drop
	uint8_t		animation = 0;

	dm.init();					// initialize dot-matrix
	init_hardware();
	sei();						// enable interrupts

	dm.displayGraphics(0, 0, OPAQUE, rainbow, FLASH, 8);
	dm.displayGraphics(8, 0, OPAQUE, rainbow, FLASH, 8);
	wait(1000);

	// get former time setting from EEPROM
	minute = eeprom_read_byte(&ee_time_setting[0]);
	if (minute > MAX_MINUTES) { minute = 0; }
	quarter = eeprom_read_byte(&ee_time_setting[1]);
	quarter &= 0x03;
	mode = RUN;

	sensor = ~PINA & (1 << INCL_PIN);	// force update of gravity by inverting sensor reading
	sense_gravity();
	reset_hour_glass();
	drop_cycle = get_drop_cycle(minute, quarter);
	last_drop = timer;

	while(1)
	{
		if (sense_gravity()) {				// hourglass was turned ?
			sense_gravity();
			if (mode != RUN) {
				mode = RUN;
				reset_hour_glass();
				drop_cycle = get_drop_cycle(minute, quarter);
			}
			random(timer);		// use timer as new seed for random number generator
			wait(400);
		}

		if (~PINA & (1 << PA0)) {			// push button S1
			wait(50);
			if (mode == SET) {
				minute++;
				if (minute > MAX_MINUTES) { minute = 0; }
				quarter = 0;
			}
			else {
				mode = SET;
			}
			while (~PINA & (1 << PA0));		// wait on release
			wait(50);
		}

		if (~PINA & (1 << PA1)) {			// push button S2
			wait(50);
			if (mode == SET) {
				quarter++;
				if (quarter > 3) {
#ifdef QUARTER_OVERFLOWS_INTO_MINUTES
					minute++;
					if (minute > MAX_MINUTES) { minute = 0; }
#endif
					quarter = 0;
				}
			}
			else {
				mode = SET;
			}
			while (~PINA & (1 << PA1));		// wait on release
			wait(50);
		}

		if (~PINA & (1 << PA2)) {			// push button S3
			wait(50);
			reset_hour_glass();
			drop_cycle = get_drop_cycle(minute, quarter);
			mode = RUN;
			while (~PINA & (1 << PA2));		// wait on release
			wait(50);
		}

		if (mode == SET) {		// show preset time
			display_time_setting(minute, quarter, gravity);
			dm.displayText(0, 0, OPAQUE, screen, RAM, 0, 16);
			wait(100);
		}

		if (mode == RUN) {		// run the simulation
			if ((timer - last_drop) >= drop_cycle) {	// is it time to drop another grain ?
				last_drop = timer;
				drop();
			}

			r = random(0);		// randomly select the pixel which is to be updated
								// bit0 = bulb, bit1..3 = x coordinate, bit4..6 = y coordinate
			if (255 == simulate_grain(r >> 1, r >> 4, r & 1)) {		// time elapsed ?
				eeprom_update_byte(&ee_time_setting[0], minute);
				eeprom_update_byte(&ee_time_setting[1], quarter);
				alarm_signal();
				mode = ALARM;
			}
		}

		if (mode == ALARM) {		// display alarm animation
			dm.displayText(0, 0, OPAQUE, screen, RAM, 0, 16);
			animation++;
			animation &= 0x03;		// limit range to 0..3
			screen[2] = 65 + animation;
			screen[3] = 68 - animation;
			wait(70);
		}

	}	// of while(1)
}


/**********************
 * interrupt routines *
 **********************/

ISR(TIM1_COMPA_vect)
// dot matrix refresh interrupt
// Called periodically at a rate defined by DM_REFRESH_FREQ.
{
	OCR1A += DM_REFRESH;				// setup next interrupt cycle
	timer++;
	sei();
	DotMatrix::update();
}



