/*
OneWireSlave v1.1 by Joshua Fuller - Modified based on versions noted below for Digispark
OneWireSlave v1.0 by Alexander Gordeyev
It is based on Jim's Studt OneWire library v2.0
Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:
The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
Much of the code was inspired by Derek Yerger's code, though I don't
think much of that remains.  In any event that was..
	(copyleft) 2006 by Derek Yerger - Free to distribute freely.
The CRC code was excerpted and inspired by the Dallas Semiconductor
sample code bearing this copyright.
//---------------------------------------------------------------------------
// Copyright (C) 2000 Dallas Semiconductor Corporation, All Rights Reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL DALLAS SEMICONDUCTOR BE LIABLE FOR ANY CLAIM, DAMAGES
// OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of Dallas Semiconductor
// shall not be used except as stated in the Dallas Semiconductor
// Branding Policy.
//---------------------------------------------------------------------------
*/
#include "OneWireSlave.h"
#include "util/OneWire_direct_gpio.h"
#if defined INTERRUPT_MODE
void OneWireSlave::ISRPIN() {
	(*static_OWS_instance).MasterResetPulseDetection();
}

uint8_t _pin;

OneWireSlave::OneWireSlave(uint8_t pin) {
	_pin = pin;
	pinMode(_pin, INPUT);
	bitmask = PIN_TO_BITMASK(_pin);
	baseReg = PIN_TO_BASEREG(_pin);

	//#define dsslavepin _pin
	attachInterrupt(dsslaveassignedint, &ISRPIN, CHANGE);
}

volatile long previous = 0;
volatile long old_previous = 0;
volatile long diff = 0;

void OneWireSlave::MasterResetPulseDetection() {
	old_previous = previous;
	previous = micros();
	diff = previous - old_previous;
	if (diff >= lowmark && diff <= highmark) {
		waitForRequestInterrupt(false);
	}
}

bool OneWireSlave::waitForRequestInterrupt(bool ignore_errors) {
	errno = ONEWIRE_NO_ERROR;
	//owsprint();
	//Reset is detected from the Interrupt by counting time between the Level-Changes
	//Once reset is done, it waits another 30 micros
	//Master wait is 65, so we have 35 more to send our presence now that reset is done
	//delayMicroseconds(30);		good working!!!
	delayMicroseconds(25);
	//Reset is complete, tell the master we are prsent
	// This will pull the line low for 125 micros (155 micros since the reset) and 
	//  then wait another 275 plus whatever wait for the line to go high to a max of 480
	// This has been modified from original to wait for the line to go high to a max of 480.
	while (!presence(50)) {};	//50	//45 arbeitet schon sehr gut
	//Now that the master should know we are here, we will get a command from the line
	//Because of our changes to the presence code, the line should be guranteed to be high
	while (recvAndProcessCmd()) {};
	if ((errno == ONEWIRE_NO_ERROR) || ignore_errors) {
		//continue;
	}
	else {
		return false;
	}
}
#endif // INTERRUPT_MODE

void OneWireSlave::begin(byte pin) {
	pMode(pin, INPUT/*INPUT_PULLUP*/);
	bitmask = PIN_TO_BITMASK(pin);
	baseReg = PIN_TO_BASEREG(pin);
}

bool OneWireSniffer::waitForRequest(byte buf[], byte& cmd, uint16_t timeout_ms, bool ignore_errors) {
	for (;;) {
		if (!waitReset(timeout_ms)) continue;
		if (!presenceDetection()) continue;
		if (recvAndProcessCmd(buf, cmd)) return true;
		if (ignore_errors) continue;
		return false;
	}
}

bool CRIT_TIMING OneWireSniffer::presenceDetection() {
	IO_REG_TYPE mask IO_REG_MASK_ATTR = bitmask;
	volatile IO_REG_TYPE* reg IO_REG_BASE_ATTR = baseReg;
	delayMicroseconds(70);
	if (!DIRECT_READ(reg, mask)) {
		uint32_t timestamp = uS + 280; //350
		while (!DIRECT_READ(reg, mask)) {
			if (uS > timestamp) {
				error = ONEWIRE_PRESENCE_LOW_ON_LINE;
				return false;          // start timeslot < 350 < presence //
			}
		}
		return true; // RESET < RISING < FALLING < 70 < PRESENCE < 350
	}
	return false; // its not presence
}

bool OneWireSniffer::recvAndProcessCmd(byte buf[], byte& cmd) {
again:
	switch (cmd = recvByte()) {
	case 0x55: // MATCH ROM //duty()
	case 0xCC: // SKIP ROM  //duty()
	case 0x33: // READ ROM
		(void)recvData(buf);
		break;
	case 0xEC: // ALARM SEARCH
	case 0xF0: // SEARCH ROM
		(void)searchAndReceive(buf);
		break;
	case CMD: // CUSTOM COMMAND
		(void)sendData(buf);
		break;
	default: // Unknow command
		if (!error) goto again;// skip if no error
		return false;
	}
	if (error) return false;
	return true;
}

bool OneWireSniffer::searchAndReceive(byte buf[]) {
	error = ONEWIRE_NO_ERROR;
	noInterrupts();
	for (byte i = 0, bitmask; i < 8; i++) {
		for (bitmask = 0x01; bitmask; bitmask <<= 1) {
			waitTimeSlot(); if (error) goto exit;
			waitTimeSlot(); if (error) goto exit;
			if (recvBit())
				buf[i] |= bitmask;
			if (error) goto exit;
		}
	}
	return true;
exit:
	interrupts();
	return false;
}

bool OneWireSlave::waitForRequest(uint16_t timeout_ms, bool ignore_errors) {
	error = ONEWIRE_NO_ERROR;
	for (;;) {
		//delayMicroseconds(40);
		//Once reset is done, it waits another 30 micros
		//Master wait is 65, so we have 35 more to send our presence now that reset is done
		if (!waitReset(timeout_ms)) {
			continue;
		}
		//Reset is complete, tell the master we are prsent
		// This will pull the line low for 125 micros (155 micros since the reset) and 
		//  then wait another 275 plus whatever wait for the line to go high to a max of 480
		// This has been modified from original to wait for the line to go high to a max of 480.
		if (!presence()) {
			continue;
		}
		//Now that the master should know we are here, we will get a command from the line
		//Because of our changes to the presence code, the line should be guranteed to be high
		if (recvAndProcessCmd())
			return true;
		else if (ignore_errors)
			continue;
		return false;
	}
}

bool OneWireSlave::recvAndProcessCmd() {
	//uint8_t oldSREG = 0;
	//uint16_t raw = 0;
	for (;;) {
		switch (recvByte()) {
		case 0xF0: // SEARCH ROM
			//raw = ((scratchpad[1] << 8) | scratchpad[0]) >> 4;
			//if (raw <= scratchpad[3] || raw >= scratchpad[2])
			search();
			//delayMicroseconds(6900);
			return false;
		case 0xEC: // ALARM SEARCH
			search();
			return false;
		case 0x33: // READ ROM
			(void)sendData(rom);
			if (error != ONEWIRE_NO_ERROR)
				return false;
			break;
		case 0x55:// MATCH ROM - Choose/Select ROM
			if (error != ONEWIRE_NO_ERROR)
				return false;
			for (byte i = 0; i < 8; i++)
				if (rom[i] != recvByte())
					return false;
			break;
			//duty();
		case 0xCC: // SKIP ROM
			//duty();
			if (error != ONEWIRE_NO_ERROR)
				return false;
			return true;
		default: // Unknow command
			if (error == ONEWIRE_NO_ERROR)
				break; // skip if no error
			else
				return false;
		}
	}
}

bool CRIT_TIMING OneWireSlave::waitReset(uint16_t timeout_ms) {
	IO_REG_TYPE mask IO_REG_MASK_ATTR = bitmask;
	volatile IO_REG_TYPE* reg IO_REG_BASE_ATTR = baseReg;
	uint32_t timestamp;
	error = ONEWIRE_NO_ERROR;
	if (timeout_ms) {      //Wait for the line to fall
		timestamp = mS + timeout_ms;
		while (DIRECT_READ(reg, mask)) {
			if (mS > timestamp) {
				error = ONEWIRE_WAIT_RESET_TIMEOUT;
				return false;
			}
		}
	}
	else while (DIRECT_READ(reg, mask)) {}; //Will wait forever for the line to fall
	timestamp = uS + 960;
	while (!DIRECT_READ(reg, mask)) {
		if (uS > timestamp) {
			error = ONEWIRE_VERY_LONG_RESET;
			return false;
		}
	}
	if (uS < (timestamp - 490)) {  //470
		error = ONEWIRE_VERY_SHORT_RESET;
		return false;
	}
	return true;
}

bool CRIT_TIMING OneWireSlave::presence() {
	IO_REG_TYPE mask IO_REG_MASK_ATTR = bitmask;
	volatile IO_REG_TYPE* reg IO_REG_BASE_ATTR = baseReg;
	delayMicroseconds(30);
	// Master will not read until 70 recommended, but could read as early as 60
	// so we should be well enough ahead of that. Arduino waits 65
	error = ONEWIRE_NO_ERROR;
	DIRECT_MODE_OUTPUT(reg, mask);    // drive output low
	//Delaying for another 125 (orignal was 120) with the line set low is a total of at least 155 micros
	// total since reset high depends on commands done prior, is technically a little longer
	delayMicroseconds(120);
	DIRECT_MODE_INPUT(reg, mask);     // allow it to float
	//Default "delta" is 25, so this is 275 in that condition, totaling to 155+275=430 since the reset rise
	// docs call for a total of 480 possible from start of rise before reset timing is completed
	//This gives us 50 micros to play with, but being early is probably best for timing on read later
	//delayMicroseconds(300 - delta);
	delayMicroseconds(280);
	//Modified to wait a while (roughly 50 micros) for the line to go high
	// since the above wait is about 430 micros, this makes this 480 closer
	// to the 480 standard spec and the 490 used on the Arduino master code
	// anything longer then is most likely something going wrong.
	uint32_t timestamp = uS + 50;
	while (!DIRECT_READ(reg, mask)) {
		if (uS > timestamp) {
			error = ONEWIRE_PRESENCE_LOW_ON_LINE;
			return false;
		}
	}
	return true;
}

byte OneWireSlave::recvData(byte buf[], byte len) {
	byte i = 0;
	do {
		buf[i] = recvByte();
		if (error) break;
	} while (++i < len);
	return i;
}

byte OneWireSlave::recvByte() {
	uint8_t BYTE = 0;
	error = ONEWIRE_NO_ERROR;
	for (uint8_t bitmask = 0x01; bitmask && !error; bitmask <<= 1)
		if (recvBit())
			BYTE |= bitmask;
	return BYTE;
}

bool CRIT_TIMING OneWireSlave::recvBit() {
	IO_REG_TYPE mask IO_REG_MASK_ATTR = bitmask;
	volatile IO_REG_TYPE* reg IO_REG_BASE_ATTR = baseReg;
	bool bit;
	noInterrupts();
	waitTimeSlot();
	if (error) goto exit;
	delayMicroseconds(20);
	bit = DIRECT_READ(reg, mask);
exit:
	interrupts();
	return bit;
}

void CRIT_TIMING OneWireSlave::waitTimeSlot() {
	IO_REG_TYPE mask IO_REG_MASK_ATTR = bitmask;
	volatile IO_REG_TYPE* reg IO_REG_BASE_ATTR = baseReg;
	volatile uint16_t retries = TIMESLOT_WAIT_RETRY_COUNT;
	//Wait for a 0 to rise to 1 on the line for timeout duration
	while (!DIRECT_READ(reg, mask)) { //While line is low, retry
		if (retries-- == 0) {
			error = ONEWIRE_READ_TIMESLOT_TIMEOUT_LOW;
			return;
		}
	}//Wait for a fall form 1 to 0 on the line for timeout duration
	retries = TIMESLOT_WAIT_RECOVERY_RETRY_COUNT;
	while (DIRECT_READ(reg, mask)) {
		if (retries-- == 0) {
			error = ONEWIRE_READ_TIMESLOT_TIMEOUT_HIGH;
			return;
		}
	}
}

bool OneWireSlave::search() {
	bool bit_send, bit_recv; byte i, bitmask;
	error = ONEWIRE_NO_ERROR;
	for (i = 0; i < 8; i++) {
		for (bitmask = 0x01; bitmask; bitmask <<= 1) {
			bit_send = (bitmask & rom[i]) ? 1 : 0;
			sendBit(bit_send); if (error) return false;
			sendBit(!bit_send); if (error) return false;
			bit_recv = recvBit(); if (error) return false;
			if (bit_recv != bit_send) return false;
		}
	}
	return true;
}

byte OneWireSlave::sendData(const byte buf[], byte lenght) {
	byte i = 0;
	do {
		sendByte(buf[i]);
		if (error) break;
	} while (++i < lenght);
	return i;
}

void OneWireSlave::sendByte(const byte v) {
	error = ONEWIRE_NO_ERROR;
	for (uint8_t bitmask = 0x01; bitmask && !error; bitmask <<= 1)
		sendBit((bitmask & v) ? 1 : 0);
}

void CRIT_TIMING OneWireSlave::sendBit(const bool bit) {
	IO_REG_TYPE mask IO_REG_MASK_ATTR = bitmask;
	volatile IO_REG_TYPE* reg IO_REG_BASE_ATTR = baseReg;
	noInterrupts();
	//waitTimeSlot waits for a low to high transition followed by a high to low within the time-out
	waitTimeSlot();
	if (error) goto exit;
	if (bit == 0) {
		DIRECT_MODE_OUTPUT(reg, mask);
		delayMicroseconds(50);
		DIRECT_MODE_INPUT(reg, mask);
	}
exit:
	interrupts();
}

bool OneWireSlave::init(byte(&buf)[8], bool crc_set) {
	rom = buf;
#if ONEWIRESLAVE_CRC
	if (crc_set) {
		rom[7] = crc8(buf, 7);
		return true;
	}
	else return (crc8(buf, 7) == buf[7]);
#endif // #if ONEWIRESLAVE_CRC
	return false;
}

#if ONEWIRESLAVE_CRC

// The 1-Wire CRC scheme is described in Maxim Application Note 27:
// "Understanding and Using Cyclic Redundancy Checks with Maxim iButton Products"

#if ONEWIRESLAVE_CRC8_TABLE
// This table comes from Dallas sample code where it is freely reusable,
// though Copyright (C) 2000 Dallas Semiconductor Corporation
static const uint8_t PROGMEM dscrc_table[] = {
	  0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
	157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
	 35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
	190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
	 70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
	219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
	101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
	248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
	140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
	 17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
	175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
	 50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
	202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
	 87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
	233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
	116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53 };

//
// Compute a Dallas Semiconductor 8 bit CRC. These show up in the ROM
// and the registers.  (note: this might better be done without to
// table, it would probably be smaller and certainly fast enough
// compared to all those delayMicrosecond() calls.  But I got
// confused, so I use this table from the examples.)
//
uint8_t OneWireSlave::crc8(byte addr[], byte len) {
	uint8_t crc = 0;

	while (len--) {
		crc = pgm_read_byte(dscrc_table + (crc ^ *addr++));
	}
	return crc;
}
#else
//
// Compute a Dallas Semiconductor 8 bit CRC directly.
//
uint8_t OneWireSlave::crc8(byte addr[], byte len) {
	uint8_t crc = 0, inbyte, mix;

	while (len--) {
		inbyte = *addr++;
		for (uint8_t i = 8; i; i--) {
			mix = (crc ^ inbyte) & 0x01;
			crc >>= 1;
			if (mix) crc ^= 0x8C;
			inbyte >>= 1;
		}
	}
	return crc;
}
#endif

#endif


