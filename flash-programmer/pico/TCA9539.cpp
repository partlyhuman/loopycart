//Arduino Library for TCA9539
//16 bit port expander
#include "TCA9539.h"

TCA9539::TCA9539() {
  _addr = 0;
  _reset_pin = 0;
  _int_pin = 0;
}

TCA9539::TCA9539(uint8_t address, TwoWire* i2c) {
  _addr = address;
  _reset_pin = 0;
  _int_pin = 0;
  _i2c = i2c;
}

TCA9539::TCA9539(uint8_t reset_pin, uint8_t int_pin, uint8_t address, TwoWire* i2c) {
  //setup pins and addresses
  _reset_pin = reset_pin;
  _int_pin = int_pin;
  _addr = address;
  _i2c = i2c;
}

void TCA9539::_set_reg_defaults() {
  _input.word = 0;
  _output.word = 0xffff;
  _pol_inv.word = 0;
  _config.word = 0xffff;
}

void TCA9539::TCA9539_init(uint8_t address, TwoWire* i2c) {
  _addr = address;
  _i2c = i2c;
  TCA9539_init();
}

void TCA9539::TCA9539_init() {
  //reset the device to initial register defaults
  //requires minimum 6ns pulse on reset which the digitalWrite
  //timing would satisfy but an extra delay to be safe
  _i2c->begin();

  if (_reset_pin != 0) {
    pinMode(_reset_pin, OUTPUT);
    digitalWrite(_reset_pin, LOW);
    delayMicroseconds(1);
    digitalWrite(_reset_pin, HIGH);
  } else {
    //TODO - reset via I2C command
  }

  //TODO - attach interrupt for inputs
  if (_int_pin != 0) {
    pinMode(_int_pin, INPUT);
  }

  //setup registers to default values
  _set_reg_defaults();
}

void TCA9539::_TCA9539_set_bit(TCA9539_register* reg, uint8_t bit, bool value) {
  if (!_valid_pin(bit)) return;
  if (value) reg->word |= 1 << bit;  //set bit
  else reg->word &= ~(1 << bit);     //clear bit
}

bool TCA9539::_TCA9539_get_bit(TCA9539_register* reg, uint8_t bit) {
  if (!_valid_pin(bit)) return false;
  return (reg->word >> bit) & 1;
}

void TCA9539::TCA9539_set_dir(uint8_t pin, TCA9539_pin_dir_t dir) {
  if (!_valid_pin(pin)) return;

  _TCA9539_set_bit(&_config, pin, dir);

  _i2c->beginTransmission(_addr);
  _i2c->write(TCA9539_CONFIG_ADDR);
  _i2c->write(_config.low);
  _i2c->write(_config.high);
  _i2c->endTransmission();
}

void TCA9539::TCA9539_set_pol_inv(uint8_t pin, TCA9539_pol_inv_t inv) {
  if (!_valid_pin(pin)) return;

  _TCA9539_set_bit(&_pol_inv, pin, inv);

  _i2c->beginTransmission(_addr);
  _i2c->write(TCA9539_POL_INV_ADDR);
  _i2c->write(_pol_inv.low);
  _i2c->write(_pol_inv.high);
  _i2c->endTransmission();
}

void TCA9539::TCA9539_set_pin_val(uint8_t pin, TCA9539_pin_val_t val) {
  if (!_valid_pin(pin)) return;

  _TCA9539_set_bit(&_output, pin, val);

  _i2c->beginTransmission(_addr);
  _i2c->write(TCA9539_OUTPUT_ADDR);
  _i2c->write(_output.low);
  _i2c->write(_output.high);
  _i2c->endTransmission();
}

bool TCA9539::TCA9539_read_pin_val(uint8_t pin) {
  if (!_valid_pin(pin)) return false;

  _TCA9539_read_reg(TCA9539_INPUT_ADDR, &_input);
  return _TCA9539_get_bit(&_input, pin);
}

bool TCA9539::TCA9539_check_pin_dir(uint8_t pin, TCA9539_pin_dir_t dir) {
  if (!_valid_pin(pin)) return false;
  return (dir == (_config.word >> pin & 1));
}

void TCA9539::_TCA9539_read_reg(uint8_t reg_addr, _TCA9539_register* reg_memory) {
  if (reg_addr > TCA9539_CONFIG_ADDR) return;

  _i2c->beginTransmission(_addr);
  _i2c->write(reg_addr);
  _i2c->endTransmission();
  _i2c->requestFrom(_addr, 2);
  reg_memory->low = _i2c->read();
  reg_memory->high = _i2c->read();
}

// @partlyhuman added

uint16_t TCA9539::TCA9539_read_word() {
  _TCA9539_read_reg(TCA9539_INPUT_ADDR, &_input);
  return _input.word;
}

uint16_t TCA9539::TCA9539_read_word_bigend() {
  _i2c->beginTransmission(_addr);
  _i2c->write(TCA9539_INPUT_ADDR);
  _i2c->endTransmission();
  _i2c->requestFrom(_addr, 2);
  _input.high = _i2c->read();
  _input.low = _i2c->read();
  return _input.word;
}

void TCA9539::TCA9539_set_word(uint16_t word) {
  _output.word = word;
  _i2c->beginTransmission(_addr);
  _i2c->write(TCA9539_OUTPUT_ADDR);
  _i2c->write(_output.low);
  _i2c->write(_output.high);
  _i2c->endTransmission();
}

void TCA9539::TCA9539_set_word_bigend(uint16_t word) {
  _output.word = word;
  _i2c->beginTransmission(_addr);
  _i2c->write(TCA9539_OUTPUT_ADDR);
  _i2c->write(_output.high);
  _i2c->write(_output.low);
  _i2c->endTransmission();
}


bool TCA9539::_valid_pin(uint8_t pin) {
  return (pin < TCA9539_REG_SIZE);
}
