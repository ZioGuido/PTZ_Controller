////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GSiSRIO - by Guido Scognamiglio - www.GenuineSoundware.com
// 
// Functions for simultaneous use of 74HC565 (out) and 74HC165 (in) Shift Registers with shared clock lines.
// This method requires only 4 wires, and reads/writes to serialized SRs.
// Set pins according to the board you're using. This was originally made for the Raspberry Pi Pico.
// Change the definition of MAX_SRs	if you need to connect more than 4 SRs in series.
//
// Usage:
//  - Instantiate a GSiSRIO object and pass the desired pin numbers
//  - Call Init() in setup() and pass a callback function of type void(uint8_t pin, uint8_t value) that will be called
//    whenever an input pin changes its state.
//  - Call SRIO_NumberSet(number) to specify the number of SR in series in your circuit, from 1 to MAX_SRs.
//  - Call DebounceSet(number) to set the debounce delay in number of scan cycles.
//  - Call DOUT_PinSet(pin, value) to set an output pin high or low.
//  - Call DOUT_SRSet(uint8_t value) to set the an entire 595 at once.
//  - Call DIN_PinGet(pin) to get the state of an input pin at any time.
//  - Call DOUT_PinGet(pin) to get the state of an output pin at any time.
//  - Call Run() in your loop() at the desired speed. For LED and buttons 1KHz is more than enough.
//
// Last update: 15 June 2022
// (TO-DO: Could be optimized! Could use a data order option.)
//

#pragma once

#if 1
#define MAX_SRs	4

#define PIN_SCK 6   // To 595 SCK(11) / To 165 CLK(2)
#define PIN_SH  7   // To 595 RCK(12) / To 165 SH(1)
#define PIN_SO  8   // To 595 SER(14)
#define PIN_SI  9   // To 165 QH(9)

// Redefine these macros to use these routines on a platform other than Arduino, for instance wiringPi.
#define _SRIO_PINMODE(pin, mode) 			pinMode(pin, mode)
#define _SRIO_DIGITALWRITE(pin, status) 	digitalWrite(pin, status)
#define _SRIO_DIGITALREAD(pin) 				digitalRead(pin)

class GSiSRIO
{
public:
	using DIN_CallbackType = void(*)(uint8_t pin, uint8_t pin_value);
	DIN_CallbackType DIN_Callback = nullptr;

	// Specify PIN GPIO numbers or use the default assignments
	GSiSRIO(uint8_t _pin_sck = PIN_SCK, uint8_t _pin_sh = PIN_SH, uint8_t _pin_so = PIN_SO, uint8_t _pin_si = PIN_SI)
	{
		pin_sck = _pin_sck;
		pin_sh = _pin_sh;
		pin_so = _pin_so;
		pin_si = _pin_si;
	}

	//////////////////////////////////////////////////////////////////////////

	// Set the number of Shift Registers in series
	void SRIO_NumberSet(uint8_t num)
	{
		if (num > MAX_SRs) num = MAX_SRs;
		NumberOfShiftRegisters = num;
		SRNumberOfPins = num * 8;
	}

	// Set a 595 SR at once
	void DOUT_SRSet(uint8_t SR, uint8_t byteSet)
	{
		SR595ByteSet[SR] = byteSet;
	}

	// Set an output pin high or low
	void DOUT_PinSet(uint8_t pin, uint8_t pin_value)
	{
		uint8_t SR = pin >> 3; // / 8;
		uint8_t bit = pin & 7; // % 8;

		if (pin_value > 0)
			SR595ByteSet[SR] |= (1 << bit);
		else
			SR595ByteSet[SR] &= ~(1 << bit);
	}

	// Get the state of an input pin
	uint8_t DIN_PinGet(uint8_t pin)
	{
		uint8_t SR = pin >> 3; // / 8;
		uint8_t bit = pin & 7; // % 8;
		return (SR165ByteSet[SR] >> bit) & 1;
	}

	// Get the state of an output pin
	uint8_t DOUT_PinGet(uint8_t pin)
	{
		uint8_t SR = pin >> 3; // / 8;
		uint8_t bit = pin & 7; //  % 8;
		return (SR595ByteSet[SR] >> bit) & 1;
	}

	// Call once in setup() to set the debounce time in number of scan cycles
	void DebounceSet(uint8_t db)
	{
		//if (db < 2) db = 2;
		SRIODebounceAmount = db;
	}

	//////////////////////////////////////////////////////////////////////////

	// Call this in loop() at a regular interval
	void Run()
	{
		// Latch the 595 outputs and load the 165 inputs
		_SRIO_DIGITALWRITE(pin_sh, LOW);
		_SRIO_DIGITALWRITE(pin_sh, HIGH);
		_SRIO_DIGITALWRITE(pin_sck, LOW);

		for (uint8_t SR = 0; SR < NumberOfShiftRegisters; ++SR)
		{
			uint8_t datain = 0;

			for (uint8_t bit = 0; bit < 8; ++bit)
			{
				// Shift the outputs
				_SRIO_DIGITALWRITE(pin_so, (SR595ByteSet[NumberOfShiftRegisters - 1 - SR] >> bit) & 1);

				// Read the inputs
				datain <<= 1;
				if (_SRIO_DIGITALREAD(pin_si) == HIGH) datain = datain | 0x01;

				// Pulse clock
				_SRIO_DIGITALWRITE(pin_sck, HIGH);
				_SRIO_DIGITALWRITE(pin_sck, LOW);
			}

			SR165ByteSet[SR] = datain;
		}

		// Check DIN changes if a callback has been defined
		if (DIN_Callback != nullptr)
			CheckDINs();
	}

	// Call once in setup() to initialize the SRIO routinse
	// _callback = pass a callback of type void(uint8_t pin, uint8_t value) that will be called
	// whenever an input pin changes its state
	void Init(DIN_CallbackType _callback = nullptr)
	{
		DIN_Callback = _callback;

		_SRIO_PINMODE(pin_sck, OUTPUT);
		_SRIO_PINMODE(pin_sh, OUTPUT);
		_SRIO_PINMODE(pin_so, OUTPUT);
		_SRIO_PINMODE(pin_si, INPUT);

		_SRIO_DIGITALWRITE(pin_sh, HIGH);
		_SRIO_DIGITALWRITE(pin_sck, HIGH);
		_SRIO_DIGITALWRITE(pin_so, HIGH);
	}

private:
	uint8_t pin_sck, pin_sh, pin_so, pin_si;

	uint8_t SR595ByteSet[MAX_SRs] = { 0 };
	uint8_t SR165ByteSet[MAX_SRs] = { 0 };
	uint8_t NumberOfShiftRegisters = 1, SRNumberOfPins = 8;
	uint8_t SRDINStateOld[MAX_SRs * 8] = { 1 };
	uint8_t SRIODebounceCounter[MAX_SRs * 8] = { 0 }, SRIODebounceAmount = 32;

	// This is called in RunSRIO() if a callback has been set
	void CheckDINs()
	{
		for (auto pin = 0; pin < SRNumberOfPins; ++pin)
		{
			auto value = DIN_PinGet(pin);
			if (SRDINStateOld[pin] != value)
			{
				SRDINStateOld[pin] = value;

				if (SRIODebounceAmount < 2)
				{
					DIN_Callback(pin, value);
					continue;
				}

				SRIODebounceCounter[pin] = SRIODebounceAmount + 1;
			}

			if (SRIODebounceCounter[pin] > 0)
				SRIODebounceCounter[pin]--;

			if (SRIODebounceCounter[pin] == 1)
			{
				auto value = DIN_PinGet(pin);
				if (SRDINStateOld[pin] == value)
					DIN_Callback(pin, value);
			}
		}
	}
};

#endif
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
