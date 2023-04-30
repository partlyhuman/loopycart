/*
 * Arduino Library for TCA9539PWR
 * IC Datasheet: https://www.ti.com/lit/ds/symlink/tca9539.pdf
 * Written By: Micah Black
 * Date: 2021-01-18
 * Revision History:
	 Micah Black - 2021-01-21 Original Writing
   @partlyhuman: added word mode
*/


#ifndef TCA9539_h
#define TCA9539_h

#include "Wire.h"

/* I2C Addresses:
 * A1=L & A0=L : 0x74
 * A1=L & A0=H : 0x75
 * A1=H & A0=L : 0x76
 * A1=H & A0=H : 0x77
 */

/* Command Bytes: Command 				: Register Default
 * 0x00 		: Read Input Port 0 	: 	xxxx xxxx (external input level)
 * 0x01 		: Read Input Port 1 	: 	xxxx xxxx (external input level)
 * 0x02 		: R/W Output Port 0 	: 	1111 1111 //write - write to output
 * 0x03 		: R/W Output Port 1 	: 	1111 1111 //read - read the value in the output flipflop
 * 0x04 		: R/W Pol Inv Port 0 	: 	0000 0000 //invert polarity of pins defined as inputs
 * 0x05 		: R/W Pol Inv Port 1 	: 	0000 0000
 * 0x06 		: R/W Config Port 0 	: 	1111 1111 //set pin I/O direction
 * 0x07 		: R/W Config Port 1 	: 	1111 1111
 */

/***********DEVICE DEFINES**************
***************************************/

typedef enum e_TCA9539_pin_dir_t{
	TCA9539_PIN_DIR_OUTPUT = false,
	TCA9539_PIN_DIR_INPUT = true
}TCA9539_pin_dir_t;

typedef enum e_TCA9539_pol_inv_t{
	TCA9539_POL_INV_FALSE = false,
	TCA9539_POL_INV_TRUE = true
}TCA9539_pol_inv_t;

typedef enum e_TCA9539_pin_val_t{
	TCA9539_PIN_OUT_LOW = false,
	TCA9539_PIN_OUT_HIGH = true
}TCA9539_pin_val_t;

#define TCA9539_REG_SIZE			16 //16 bits including 0

/***********DEVICE ADDRESSES************
***************************************/

#define TCA9539_INPUT_ADDR 			0x00
#define TCA9539_INPUT_PORT_0_ADDR 	0x00
#define TCA9539_INPUT_PORT_1_ADDR 	0x01

#define TCA9539_OUTPUT_ADDR			0x02
#define TCA9539_OUTPUT_PORT_0_ADDR	0x02
#define TCA9539_OUTPUT_PORT_1_ADDR	0x03

#define TCA9539_POL_INV_ADDR		0x04
#define TCA9539_POL_INV_PORT_0_ADDR	0x04
#define TCA9539_POL_INV_PORT_1_ADDR 0x05

#define TCA9539_CONFIG_ADDR			0x06
#define TCA9539_CONFIR_PORT_0_ADDR	0x06
#define TCA9539_CONFIG_PORT_1_ADDR	0x07


typedef union _TCA9539_register{
	uint16_t word;
	struct{
		uint8_t low;
		uint8_t high;
	};
}TCA9539_register;


class TCA9539
{
	public:
		TCA9539();
		TCA9539(uint8_t address, TwoWire *i2c = &Wire);
		TCA9539(uint8_t reset_pin, uint8_t int_pin, uint8_t address, TwoWire *i2c = &Wire); //constructor
		
		void TCA9539_init(); //reset with the reset pin
		void TCA9539_init(uint8_t address, TwoWire *i2c = &Wire);
		void TCA9539_set_dir(uint8_t pin, TCA9539_pin_dir_t dir);
		void TCA9539_set_pol_inv(uint8_t pin, TCA9539_pol_inv_t inv);
		void TCA9539_set_pin_val(uint8_t pin, TCA9539_pin_val_t val);
		bool TCA9539_read_pin_val(uint8_t pin);
		bool TCA9539_check_pin_dir(uint8_t pin, TCA9539_pin_dir_t dir);

    // @partlyhuman added
		uint16_t TCA9539_read_word();
		uint16_t TCA9539_read_word_bigend();
		void TCA9539_set_word(uint16_t word);
		void TCA9539_set_word_bigend(uint16_t word);
		
	private:
		bool _valid_pin(uint8_t pin);
		void _set_reg_defaults();
		void _TCA9539_set_bit(TCA9539_register* reg, uint8_t bit, bool value);
		bool _TCA9539_get_bit(TCA9539_register* reg, uint8_t bit);
		void _TCA9539_read_reg(uint8_t addr, _TCA9539_register* reg_memory);
		
		//pins and I2C address
		uint8_t _addr;
		uint8_t _reset_pin;
		uint8_t _int_pin;
		TwoWire *_i2c;
		
		//registers
		TCA9539_register _input;
		TCA9539_register _output;
		TCA9539_register _pol_inv;
		TCA9539_register _config;
};

#endif
