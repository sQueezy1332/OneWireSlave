/*
 Name:		OneWireSlave.h
 Created:	14-Jun-24 07:46:16
 Author:	sQueezy
 Editor:	http://www.visualmicro.com
*/

#pragma once
#ifdef __cplusplus

#include <stdint.h>

#if defined(__AVR__)
#include <util/crc16.h>
#endif

#include <Arduino.h>       // for delayMicroseconds, digitalPinToBitMask, etc
#include <GyverIO.h>
#define pInit(pin, mode)    gio::init(pin, mode)
#define DIRECT_READ(pin)	gio::read(pin)
#if defined(__AVR__)
#define DIRECT_WRITE_HIGH(pin)	gio::mode(pin, INPUT)
#define DIRECT_WRITE_LOW(pin) gio::mode(pin, OUTPUT)
#else
#define DIRECT_WRITE_HIGH(pin)	gio::high(pin)	//OPEN_DRAIN
#define DIRECT_WRITE_LOW(pin) gio::low(pin)
#endif
#define systime micros()
#define mS millis()

#ifdef ARDUINO_ARCH_ESP32
// for info on this, search "IRAM_ATTR" at https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/general-notes.html 
#define CRIT_TIMING IRAM_ATTR
#else
#define CRIT_TIMING 
#endif

// You can exclude certain features from OneWire.  In theory, this
// might save some space.  In practice, the compiler automatically
// removes unused code (technically, the linker, using -fdata-sections
// and -ffunction-sections when compiling, and Wl,--gc-sections
// when linking), so most of these will not result in any code size
// reduction.  Well, unless you try to use the missing features
// and redesign your program to not need them!  ONEWIRE_CRC8_TABLE
// is the exception, because it selects a fast but large algorithm
// or a small but slow algorithm.

// Board-specific macros for direct GPIO
//#include "util/OneWire_direct_regtype.h"
//#define microsecondsToClockCycles(a) ( (a) * clockCyclesPerMicrosecond() )
//#define clockCyclesPerMicrosecond() ( F_CPU / 1000000L )
#if defined(__AVR__)
#define divider 18uL //25 w/o read 100000 is 10088-10092 //30 with READ 10048-10056 (~10056)   
#endif 
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP8266)
#define divider 27uL //11 with out read 10000 is 10075 // 27 with read 10000 is 10077 
#endif 
//These are the major change from original, we now wait quite a bit longer for some things
#define TIMESLOT_WAIT_RETRY_COUNT microsecondsToClockCycles(120) / divider //295000 Maximum with interrupts
//This TIMESLOT_WAIT_READ_RETRY_COUNT is new, and only used when waiting for the line to go low on a read
//It was derived from knowing that the Arduino based master may go up to 130 micros more than our wait after reset
#define TIMESLOT_WAIT_RECOVERY_RETRY_COUNT microsecondsToClockCycles(2000) / divider

#define ONEWIRESLAVE_CRC 1
// Select the table-lookup method of computing the 8-bit CRC
// by setting this to 1.  The lookup table no longer consumes
// limited RAM, but enlarges total code size by about 250 bytes
#define ONEWIRESLAVE_CRC8_TABLE 1
#define CMD 0xAA
typedef uint8_t byte;

__attribute__((weak)) extern const byte pin_onewire;

class OneWireSlave {
private:
	byte* rom = nullptr;
protected:
	void waitTimeSlot();
public:
	OneWireSlave() { begin(pin_onewire); }
	void begin(byte pin);
	void init(byte buf[8], bool crc_set = false);
	bool waitForRequest(uint16_t timeout_ms = 1000, bool ignore_errors = false);
	bool recvAndProcessCmd();
	bool waitReset(uint16_t timeout_ms = 1000);
	bool presence();
	byte recvData(byte buf[], byte lenght = 8);
	byte recvByte();
	bool recvBit();
	byte sendData(const byte* const& buf, byte lenght = 8);
	void sendByte(const byte v);
	void sendBit(const bool bit);
	bool search();
	typedef enum {
		ONEWIRE_NO_ERROR = 0,
		ONEWIRE_READ_TIMESLOT_TIMEOUT_LOW,
		ONEWIRE_READ_TIMESLOT_TIMEOUT_HIGH,
		ONEWIRE_WAIT_RESET_TIMEOUT,
		ONEWIRE_VERY_LONG_RESET,
		ONEWIRE_VERY_SHORT_RESET,
		ONEWIRE_PRESENCE_LOW_ON_LINE
	} err_t;
#if ONEWIRESLAVE_CRC
	byte crc8(const byte addr[], byte len);
#endif
	err_t error = ONEWIRE_NO_ERROR;
};

class OneWireSniffer : public OneWireSlave {
public:
	OneWireSniffer() {}; //OneWireSlave() {};
	bool waitForRequest(byte buf[8], byte& cmd, uint16_t timeout_ms = 1000, bool ignore_errors = true);
	bool presenceDetection();
	bool recvAndProcessCmd(byte* const& buf, byte& cmd);
	bool searchAndReceive(byte* const& buf);
};

//static OneWireSlave* static_OWS_instance;
#endif
