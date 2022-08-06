/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
// Controller for PTZ camera using PelcoD protocol
// Based on Raspberry Pi Pico + RS485 transceiver
// All buttons and LEDs are scanned/driven using 74HC165 & 74HC595 shift registers
//
// Code by Guido Scognamiglio - www.GenuineSoundware.com
// Last update: 21 July 2022
//


//#include "../../GSiSRIO/SRIO.h"
#include "c:/Users/ziogu/Documents/Arduino/Arduino/GSiSRIO/SRIO.h"

GSiSRIO SRIO;
auto SRIOTimer = millis();
auto ButtonTimer = 0U;

byte PINS_CameraSelectBtn[] = { 0, 1, 2, 3 };
byte PINS_PresetBtn[] = { 4, 5, 6, 7, 8, 9 };
#define PIN_FOCUS_AUTO	10
#define PIN_FOCUS_F		11
#define PIN_FOCUS_N		12
#define PIN_ZOOM_W		13
#define PIN_ZOOM_T		14
#define PIN_UP_LEFT		15 
#define PIN_UP			16 
#define PIN_UP_RIGHT	17
#define PIN_LEFT		18
#define PIN_RIGHT		19 
#define PIN_DOWN_LEFT	20
#define PIN_DOWN		21
#define PIN_DOWN_RIGHT	22
byte PINS_PanTiltBtn[] = { 15, 16, 17, 18, 19, 20, 21, 22 };

// Remember: LED pins are reversed for each 595
byte LEDS_CameraSelect[] = { 7, 6, 5, 4 };
byte LEDS_Preset[] = { 3, 2, 1, 0, 15, 14 };
#define LED_FOCUS_AUTO	 13

byte Camera = 0;
byte Preset = 0;

byte CameraAddr = 1;
byte PT_Speed = 1;
byte ZOOM_Speed = 1;

byte PanningOrTilting = 0;

int ADC_Value[2] = { -1, -1 };
int ADC_Ports[2] = { A0, A1 };
int ADC_Select = 0;

int PresetPerCam[4] = { -1, -1, -1, -1 };

int BlinkLed = -1, BlinkT = 0, BlinkS = 0, BlinkC = 0;

void ReadADC(int port)
{
	// ADC Resolution is 12 bits
	auto value = 0; 
	for (int i = 0; i < 10; ++i) value += analogRead(ADC_Ports[port]);
	value /= 10;

	auto diff = abs(value - ADC_Value[port]);
	if (diff <= 8) return;
	ADC_Value[port] = value;

	//Serial.printf("ADC n. %d - Value = %d\n", port, value);
	
	// Pot 0 = Pan/Tilt speed
	if (port == 0)
	{
		int newSpeed = value >> 6; // Pan/Tilt speed is 6 bits 0x00 ~ 0x3F
		if (PT_Speed != newSpeed)
		{
			PT_Speed = newSpeed;

			if (PanningOrTilting > 0)
				SendPelcoD(CameraAddr, 0x00, PanningOrTilting, PT_Speed, PT_Speed);
		}
	}
	
	// Pot 1 = Zoom speed	
	if (port == 1)
	{
		 auto newSpeed = value >> 10; // only 2 bits!
		 if (ZOOM_Speed != newSpeed)
		 {
			 ZOOM_Speed = newSpeed;
			 SendPelcoD(CameraAddr, 0x00, 0x25, 0x00, ZOOM_Speed);
		 }
	}
	//Serial.printf("PT_Speed = %d - ZOOM_Speed = %d\n", PT_Speed, ZOOM_Speed);
}

void SendPelcoD(uint8_t addr, uint8_t cmd1, uint8_t cmd2, uint8_t data1, uint8_t data2)
{
	int sum = (addr + cmd1 + cmd2 + data1 + data2) % 0x100;
	uint8_t cmd[7] = { 0xFF, addr, cmd1, cmd2, data1, data2, sum };
	Serial1.write(cmd, 7);

	Serial.printf("Pelco-D: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6]);
}

void SelectCamera(byte cam)
{
	Serial.printf("Set Camera n. %d\n", cam + 1);

	Camera = cam;
	CameraAddr = Camera + 1;
	
	for (byte c = 0; c < 4; c++)
		SRIO.DOUT_PinSet(LEDS_CameraSelect[c], Camera == c);

	for (auto p = 0; p < 6; ++p)
		SRIO.DOUT_PinSet(LEDS_Preset[p], PresetPerCam[Camera] == p);
}

void NoPreset()
{
	PresetPerCam[Camera] = -1;

	for (auto p = 0; p < 6; ++p)
		SRIO.DOUT_PinSet(LEDS_Preset[p], 0);
}

void SavePreset(byte pNum)
{
	Serial.printf("Save Presetn n. %d\n", pNum + 1);
	
	SendPelcoD(CameraAddr, 0x00, 0x03, 0x00, pNum);
	PresetPerCam[Camera] = pNum;

	for (auto p = 0; p < 6; ++p)
		SRIO.DOUT_PinSet(LEDS_Preset[p], 0);

	BlinkLed = LEDS_Preset[pNum];
}

void RecallPreset(byte pNum)
{
	Serial.printf("Recall Presetn n. %d\n", pNum + 1);
	
	ButtonTimer = 0;
	PresetPerCam[Camera] = pNum;
	SendPelcoD(CameraAddr, 0x00, 0x07, 0x00, pNum);

	for (auto p = 0; p < 6; ++p)
		SRIO.DOUT_PinSet(LEDS_Preset[p], p == pNum);
}

void DIN_Notify(uint8_t pin, uint8_t value)
{
	// Button down
	if (value == 0)
	{
		switch (pin)
		{
		case PIN_UP_LEFT:		SendPelcoD(CameraAddr, 0x00, 0x0C, PT_Speed, PT_Speed); NoPreset(); PanningOrTilting = 0x0C; break;
		case PIN_UP:			SendPelcoD(CameraAddr, 0x00, 0x08, PT_Speed, PT_Speed); NoPreset(); PanningOrTilting = 0x08; break;
		case PIN_UP_RIGHT:		SendPelcoD(CameraAddr, 0x00, 0x0A, PT_Speed, PT_Speed); NoPreset(); PanningOrTilting = 0x0A; break;
		case PIN_LEFT:			SendPelcoD(CameraAddr, 0x00, 0x04, PT_Speed, PT_Speed); NoPreset(); PanningOrTilting = 0x04; break;
		case PIN_RIGHT:			SendPelcoD(CameraAddr, 0x00, 0x02, PT_Speed, PT_Speed); NoPreset(); PanningOrTilting = 0x02; break;
		case PIN_DOWN_LEFT:		SendPelcoD(CameraAddr, 0x00, 0x14, PT_Speed, PT_Speed); NoPreset(); PanningOrTilting = 0x14; break;
		case PIN_DOWN:			SendPelcoD(CameraAddr, 0x00, 0x10, PT_Speed, PT_Speed); NoPreset(); PanningOrTilting = 0x10; break;
		case PIN_DOWN_RIGHT:	SendPelcoD(CameraAddr, 0x00, 0x12, PT_Speed, PT_Speed); NoPreset(); PanningOrTilting = 0x12; break;

		case PIN_FOCUS_AUTO:	
			SendPelcoD(CameraAddr, 0x00, 0x2B, 0, 0);
			SRIO.DOUT_PinSet(LED_FOCUS_AUTO, 1);
			break;
		case PIN_FOCUS_N:		
			SRIO.DOUT_PinSet(LED_FOCUS_AUTO, 0);
			SendPelcoD(CameraAddr, 0x00, 0x2B, 0, 1);
			SendPelcoD(CameraAddr, 0x01, 0x00, 0, 0); 
			break;
		case PIN_FOCUS_F:		
			SRIO.DOUT_PinSet(LED_FOCUS_AUTO, 0);
			SendPelcoD(CameraAddr, 0x00, 0x2B, 0, 1);
			SendPelcoD(CameraAddr, 0x00, 0x80, 0, 0);
			break;

		case PIN_ZOOM_W:		
			SendPelcoD(CameraAddr, 0x00, 0x40, 0x00, 0x00);
			NoPreset();
			break;
		case PIN_ZOOM_T:		
			SendPelcoD(CameraAddr, 0x00, 0x20, 0x00, 0x00);
			NoPreset();
			break;

		default:
			for (auto c : PINS_CameraSelectBtn)
				if (pin == c)
					return SelectCamera(c);
			
			for (auto p : PINS_PresetBtn)
				if (pin == p)
				{
					ButtonTimer = 500; 
					return;
				}
		}
	}

	// Button up
	else
	{
		// Recall Preset
		for (auto p = 0; p < 6; ++p)
			if (pin == PINS_PresetBtn[p] && ButtonTimer > 1)
				return RecallPreset(p);

		// If one of the Pan/Tilt buttons is still depressed when another button has been released, take action
		for (auto p : PINS_PanTiltBtn)
			if (SRIO.DIN_PinGet(p) == 0)
				return DIN_Notify(p, 0);
		
		// Stop any movement when a button is released
		SendPelcoD(CameraAddr, 0, 0, 0, 0);
		PanningOrTilting = 0;
	}
}

void setup()
{
	Serial.begin(115200);
	Serial1.begin(9600);

	SRIO.Init(DIN_Notify);
	SRIO.SRIO_NumberSet(3);
	SRIO.DebounceSet(16);

	analogReadResolution(10); delay(100); analogReadResolution(12);

	SelectCamera(0);
	DIN_Notify(PIN_FOCUS_AUTO, 0);
}

void loop()
{
	// Run every millisecond
	if (millis() - SRIOTimer > 1)
	{
		SRIOTimer = millis();

		ReadADC(ADC_Select);
		ADC_Select = ADC_Select == 0 ? 1 : 0;

		SRIO.Run();

		// Button long press for saving presets?
		if (ButtonTimer > 0) ButtonTimer--;
		if (ButtonTimer == 1)
		{
			ButtonTimer = 0;
			for (auto p = 0; p < 6; ++p)
				if (SRIO.DIN_PinGet(PINS_PresetBtn[p]) == 0)
					return SavePreset(p);
		}

		if (BlinkLed >= 0)
		{
			if (++BlinkT >= 25)
			{
				BlinkT = 0;
				SRIO.DOUT_PinSet(BlinkLed, BlinkS);
				BlinkS = ~BlinkS;

				if (++BlinkC > 5)
				{
					BlinkC = 0;
					BlinkLed = -1;
					SRIO.DOUT_PinSet(BlinkLed, 1);
				}
			}
		}
	}
}
