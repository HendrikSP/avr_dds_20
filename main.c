//*****************************************************************************
//
// File Name	: 'main.c'
// Title		: AVR DDS2 signal generator
// Author		: Scienceprog.com - Copyright (C) 2008
// Created		: 2008-03-09
// Revised		: 2008-03-09
// Version		: 2.0
// Target MCU	: Atmel AVR series ATmega16
//
// This code is distributed under the GNU Public License
//		which can be found at http://www.gnu.org/licenses/gpl.txt
//
//*****************************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <inttypes.h>
#include "lcd_lib.h"
//define R2R port
#define R2RPORT PORTA
#define R2RDDR DDRA
//define button port and dedicated pins
#define BPORT PORTD
#define BPIN PIND
#define BDDR DDRD
#define DOWN 0//PORTD
#define LEFT 1//PORTD
#define START  2//PORTD
#define RIGHT 3//PORTD
#define UP 4//PORTD
//Define Highs Speed (HS) signal output
#define HSDDR DDRD
#define HSPORT PORTD
#define HS 5//PD5
//define eeprom addresses
#define EEMODE 0
#define EEFREQ1 1
#define EEFREQ2 2
#define EEFREQ3 3
#define EEDUTY 4
#define EEINIT E2END
#define RESOLUTION 0.095367431640625
#define MINFREQ 0//minimum frequency
#define MAXFREQ 65534//maximum DDS frequency
#define MN_No 10// number of menu items

//function prototypes
void delay1s(void);
void Timer2_Init(void);
void Timer2_Start(void);
void Timer2_Stop(void);
void Main_Init(void);
void Menu_Update(uint8_t);
void Freq_Update(void);
void Timer1_Start(uint8_t);
void Timer1_Stop(void);
void checkButtons(void);
void static inline Signal_OUT(const uint8_t *, uint8_t, uint8_t, uint8_t);
//adjust LCDsendChar() function for strema
static int LCDsendstream(char c, FILE *stream);
//----set output stream to LCD-------
static FILE lcd_str = FDEV_SETUP_STREAM(LCDsendstream, NULL, _FDEV_SETUP_WRITE);

//Menu Strings in flash
const uint8_t MN000[] PROGMEM="      Sine      \0";
//menu 1
const uint8_t MN100[] PROGMEM="     Square     \0";
//menu 2
const uint8_t MN200[] PROGMEM="    Triangle    \0";
//menu 3
const uint8_t MN300[] PROGMEM="    SawTooth    \0";
//menu 4
const uint8_t MN400[] PROGMEM="  Rev SawTooth  \0";
//menu 5
const uint8_t MN500[] PROGMEM="      ECG       \0";
//menu 6
const uint8_t MN600[] PROGMEM="    Freq Step   \0";
//menu 7
const uint8_t MN700[] PROGMEM="     Noise      \0";
//menu 8
const uint8_t MN800[] PROGMEM="   High Speed   \0";
//menu 9
const uint8_t MN900[] PROGMEM="    PWM (HS)    \0";

//Array of pointers to menu strings stored in flash
const uint8_t *MENU[]={
		MN000,	//
		MN100,	//menu 1 string
		MN200,	//menu 2 string
		MN300,	//menu 3 string
		MN400,	//menu 4 string
		MN500,	
		MN600,
		MN700,
		MN800,
		MN900
		}; 
//various LCD strings
const uint8_t MNON[] PROGMEM="ON \0";//ON
const uint8_t MNOFF[] PROGMEM="OFF\0";//OFF
const uint8_t NA[] PROGMEM="       NA       \0";//Clear freq value
const uint8_t CLR[] PROGMEM="                \0";//Clear freq value	
const uint8_t MNClrfreq[] PROGMEM="           \0";//Clear freq value
const uint8_t TOEEPROM[] PROGMEM="Saving Settings\0";//saving to eeprom
const uint8_t ONEMHZ[] PROGMEM="      1MHz   \0";//saving to eeprom
const uint8_t welcomeln1[] PROGMEM="AVR SIGNAL\0";
const uint8_t RND[] PROGMEM="    Random\0";

//variables to control TDA7313
struct signal{
volatile uint8_t mode;		//signal
volatile uint8_t fr1;		//Frequency [0..7]
volatile uint8_t fr2;		//Frequency [8..15]
volatile int8_t fr3;		//Frequency [16..31]
volatile uint32_t freq;		//frequency value
volatile uint8_t flag;		//if "0"generator is OFF, "1" - ON
volatile uint32_t acc;		//accumulator
volatile uint8_t ON;
volatile uint8_t HSfreq;		//high speed frequency [1...4Mhz]
volatile uint32_t deltafreq;	//frequency step value
volatile uint16_t pwmFreq;		//PWM freq [62.5Hz...62500Hz]
volatile uint8_t  pwmDuty;
}SG;

//define signals
const uint8_t  sinewave[] __attribute__ ((section (".MySection1")))= //sine 256 values
{
0x80,0x83,0x86,0x89,0x8c,0x8f,0x92,0x95,0x98,0x9c,0x9f,0xa2,0xa5,0xa8,0xab,0xae,
0xb0,0xb3,0xb6,0xb9,0xbc,0xbf,0xc1,0xc4,0xc7,0xc9,0xcc,0xce,0xd1,0xd3,0xd5,0xd8,
0xda,0xdc,0xde,0xe0,0xe2,0xe4,0xe6,0xe8,0xea,0xec,0xed,0xef,0xf0,0xf2,0xf3,0xf5,
0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfc,0xfd,0xfe,0xfe,0xff,0xff,0xff,0xff,0xff,
0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xfe,0xfd,0xfc,0xfc,0xfb,0xfa,0xf9,0xf8,0xf7,
0xf6,0xf5,0xf3,0xf2,0xf0,0xef,0xed,0xec,0xea,0xe8,0xe6,0xe4,0xe2,0xe0,0xde,0xdc,
0xda,0xd8,0xd5,0xd3,0xd1,0xce,0xcc,0xc9,0xc7,0xc4,0xc1,0xbf,0xbc,0xb9,0xb6,0xb3,
0xb0,0xae,0xab,0xa8,0xa5,0xa2,0x9f,0x9c,0x98,0x95,0x92,0x8f,0x8c,0x89,0x86,0x83,
0x80,0x7c,0x79,0x76,0x73,0x70,0x6d,0x6a,0x67,0x63,0x60,0x5d,0x5a,0x57,0x54,0x51,
0x4f,0x4c,0x49,0x46,0x43,0x40,0x3e,0x3b,0x38,0x36,0x33,0x31,0x2e,0x2c,0x2a,0x27,
0x25,0x23,0x21,0x1f,0x1d,0x1b,0x19,0x17,0x15,0x13,0x12,0x10,0x0f,0x0d,0x0c,0x0a,
0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x03,0x02,0x01,0x01,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x02,0x03,0x03,0x04,0x05,0x06,0x07,0x08,
0x09,0x0a,0x0c,0x0d,0x0f,0x10,0x12,0x13,0x15,0x17,0x19,0x1b,0x1d,0x1f,0x21,0x23,
0x25,0x27,0x2a,0x2c,0x2e,0x31,0x33,0x36,0x38,0x3b,0x3e,0x40,0x43,0x46,0x49,0x4c,
0x4f,0x51,0x54,0x57,0x5a,0x5d,0x60,0x63,0x67,0x6a,0x6d,0x70,0x73,0x76,0x79,0x7c
};
const uint8_t squarewave[] __attribute__ ((section (".MySection2")))= //square wave
{
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
};
const uint8_t sawtoothwave[] __attribute__ ((section (".MySection3")))= //sawtooth wave
{
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};
const uint8_t rewsawtoothwave[] __attribute__ ((section (".MySection4")))= //reverse sawtooth wave
{
0xff,0xfe,0xfd,0xfc,0xfb,0xfa,0xf9,0xf8,0xf7,0xf6,0xf5,0xf4,0xf3,0xf2,0xf1,0xf0,
0xef,0xee,0xed,0xec,0xeb,0xea,0xe9,0xe8,0xe7,0xe6,0xe5,0xe4,0xe3,0xe2,0xe1,0xe0,
0xdf,0xde,0xdd,0xdc,0xdb,0xda,0xd9,0xd8,0xd7,0xd6,0xd5,0xd4,0xd3,0xd2,0xd1,0xd0,
0xcf,0xce,0xcd,0xcc,0xcb,0xca,0xc9,0xc8,0xc7,0xc6,0xc5,0xc4,0xc3,0xc2,0xc1,0xc0,
0xbf,0xbe,0xbd,0xbc,0xbb,0xba,0xb9,0xb8,0xb7,0xb6,0xb5,0xb4,0xb3,0xb2,0xb1,0xb0,
0xaf,0xae,0xad,0xac,0xab,0xaa,0xa9,0xa8,0xa7,0xa6,0xa5,0xa4,0xa3,0xa2,0xa1,0xa0,
0x9f,0x9e,0x9d,0x9c,0x9b,0x9a,0x99,0x98,0x97,0x96,0x95,0x94,0x93,0x92,0x91,0x90,
0x8f,0x8e,0x8d,0x8c,0x8b,0x8a,0x89,0x88,0x87,0x86,0x85,0x84,0x83,0x82,0x81,0x80,
0x7f,0x7e,0x7d,0x7c,0x7b,0x7a,0x79,0x78,0x77,0x76,0x75,0x74,0x73,0x72,0x71,0x70,
0x6f,0x6e,0x6d,0x6c,0x6b,0x6a,0x69,0x68,0x67,0x66,0x65,0x64,0x63,0x62,0x61,0x60,
0x5f,0x5e,0x5d,0x5c,0x5b,0x5a,0x59,0x58,0x57,0x56,0x55,0x54,0x53,0x52,0x51,0x50,
0x4f,0x4e,0x4d,0x4c,0x4b,0x4a,0x49,0x48,0x47,0x46,0x45,0x44,0x43,0x42,0x41,0x40,
0x3f,0x3e,0x3d,0x3c,0x3b,0x3a,0x39,0x38,0x37,0x36,0x35,0x34,0x33,0x32,0x31,0x30,
0x2f,0x2e,0x2d,0x2c,0x2b,0x2a,0x29,0x28,0x27,0x26,0x25,0x24,0x23,0x22,0x21,0x20,
0x1f,0x1e,0x1d,0x1c,0x1b,0x1a,0x19,0x18,0x17,0x16,0x15,0x14,0x13,0x12,0x11,0x10,
0x0f,0x0e,0x0d,0x0c,0x0b,0x0a,0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00,
};

const uint8_t trianglewave[] __attribute__ ((section (".MySection5")))= //triangle wave
{
0x00,0x02,0x04,0x06,0x08,0x0a,0x0c,0x0e,0x10,0x12,0x14,0x16,0x18,0x1a,0x1c,0x1e,
0x20,0x22,0x24,0x26,0x28,0x2a,0x2c,0x2e,0x30,0x32,0x34,0x36,0x38,0x3a,0x3c,0x3e,
0x40,0x42,0x44,0x46,0x48,0x4a,0x4c,0x4e,0x50,0x52,0x54,0x56,0x58,0x5a,0x5c,0x5e,
0x60,0x62,0x64,0x66,0x68,0x6a,0x6c,0x6e,0x70,0x72,0x74,0x76,0x78,0x7a,0x7c,0x7e,
0x80,0x82,0x84,0x86,0x88,0x8a,0x8c,0x8e,0x90,0x92,0x94,0x96,0x98,0x9a,0x9c,0x9e,
0xa0,0xa2,0xa4,0xa6,0xa8,0xaa,0xac,0xae,0xb0,0xb2,0xb4,0xb6,0xb8,0xba,0xbc,0xbe,
0xc0,0xc2,0xc4,0xc6,0xc8,0xca,0xcc,0xce,0xd0,0xd2,0xd4,0xd6,0xd8,0xda,0xdc,0xde,
0xe0,0xe2,0xe4,0xe6,0xe8,0xea,0xec,0xee,0xf0,0xf2,0xf4,0xf6,0xf8,0xfa,0xfc,0xfe,
0xff,0xfd,0xfb,0xf9,0xf7,0xf5,0xf3,0xf1,0xef,0xef,0xeb,0xe9,0xe7,0xe5,0xe3,0xe1,
0xdf,0xdd,0xdb,0xd9,0xd7,0xd5,0xd3,0xd1,0xcf,0xcf,0xcb,0xc9,0xc7,0xc5,0xc3,0xc1,
0xbf,0xbd,0xbb,0xb9,0xb7,0xb5,0xb3,0xb1,0xaf,0xaf,0xab,0xa9,0xa7,0xa5,0xa3,0xa1,
0x9f,0x9d,0x9b,0x99,0x97,0x95,0x93,0x91,0x8f,0x8f,0x8b,0x89,0x87,0x85,0x83,0x81,
0x7f,0x7d,0x7b,0x79,0x77,0x75,0x73,0x71,0x6f,0x6f,0x6b,0x69,0x67,0x65,0x63,0x61,
0x5f,0x5d,0x5b,0x59,0x57,0x55,0x53,0x51,0x4f,0x4f,0x4b,0x49,0x47,0x45,0x43,0x41,
0x3f,0x3d,0x3b,0x39,0x37,0x35,0x33,0x31,0x2f,0x2f,0x2b,0x29,0x27,0x25,0x23,0x21,
0x1f,0x1d,0x1b,0x19,0x17,0x15,0x13,0x11,0x0f,0x0f,0x0b,0x09,0x07,0x05,0x03,0x01
};
const uint8_t ECG[] __attribute__ ((section (".MySection6")))= //ECG wave
{
73,74,75,75,74,73,73,73,73,72,71,69,68,67,67,67,
68,68,67,65,62,61,59,57,56,55,55,54,54,54,55,55,
55,55,55,55,54,53,51,50,49,49,52,61,77,101,132,
169,207,238,255,254,234,198,154,109,68,37,17,5,
0,1,6,13,20,28,36,45,52,57,61,64,65,66,67,68,68,
69,70,71,71,71,71,71,71,71,71,72,72,72,73,73,74,
75,75,76,77,78,79,80,81,82,83,84,86,88,91,93,96,
98,100,102,104,107,109,112,115,118,121,123,125,
126,127,127,127,127,127,126,125,124,121,119,116,
113,109,105,102,98,95,92,89,87,84,81,79,77,76,75,
74,73,72,70,69,68,67,67,67,68,68,68,69,69,69,69,
69,69,69,70,71,72,73,73,74,74,75,75,75,75,75,75,
74,74,73,73,73,73,72,72,72,71,71,71,71,71,71,71,
70,70,70,69,69,69,69,69,70,70,70,69,68,68,67,67,
67,67,66,66,66,65,65,65,65,65,65,65,65,64,64,63,
63,64,64,65,65,65,65,65,65,65,64,64,64,64,64,64,
64,64,65,65,65,66,67,68,69,71,72,73
};
//array of pointers to signal tables
const uint8_t *SIGNALS[] ={
	sinewave,
	squarewave,
	trianglewave,
	sawtoothwave,
	rewsawtoothwave,
	ECG
};
//adjust LCD stream fuinction to use with printf()
static int LCDsendstream(char c , FILE *stream)
{
LCDsendChar(c);
return 0;
}
//delay 1s
void delay1s(void)
{
	uint8_t i;
	for(i=0;i<100;i++)
	{
		_delay_ms(10);
	}
}
//initialize Timer2 (used for button reading)
void Timer2_Init(void)
{
	TCNT2=0x00;
	sei();
} 
//start timer2
void Timer2_Start(void)
{
	TCCR2|=(1<<CS22)|(1<<CS21); //prescaller 256 ~122 interrupts/s
	TIMSK|=(1<<TOV2);//Enable Timer0 Overflow interrupts
}
//stop timer 2
void Timer2_Stop(void)
{
	TCCR0&=~((1<<CS22)|(1<<CS21)); //Stop timer0
	TIMSK&=~(1<<TOV2);//Disable Timer0 Overflow interrupts
}

//Initial menu
//show initial signal and frequency
//generator is off
void Menu_Update(uint8_t on)
{
	LCDclr();
	CopyStringtoLCD(MENU[(SG.mode)], 0, 0 );
	LCDGotoXY(0, 1);
	if (SG.mode==6)
		{
			CopyStringtoLCD(CLR, 0, 1 );
			LCDGotoXY(0, 1);
			printf("    %5uHz", (uint16_t)SG.deltafreq);
		}
	if (SG.mode==7)
		{
			CopyStringtoLCD(CLR, 0, 1 );
			CopyStringtoLCD(RND, 0, 1 );
		}
	if (SG.mode==8)
		{
		CopyStringtoLCD(CLR, 0, 1 );
		LCDGotoXY(0, 1);
		printf(" %5uMHz", SG.HSfreq);
		}
	if (SG.mode==9)	{ // PWM
		LCDGotoXY(0, 1);
		printf("%5uHz %3u", SG.pwmFreq, SG.pwmDuty);
	}
	if((SG.mode==0)||(SG.mode==1)||(SG.mode==2)||(SG.mode==3)||(SG.mode==4)||(SG.mode==5))
		{
			CopyStringtoLCD(CLR, 0, 1 );
			LCDGotoXY(0, 1);
			printf(" %5uHz", (uint16_t)SG.freq);
		}
	if (SG.mode!=6)
	{
		if(on==1)
			CopyStringtoLCD(MNON, 13, 1 );
		else
			CopyStringtoLCD(MNOFF, 13, 1 );
	}
	_delay_ms(100);
}

//update frequency value on LCD menu - more smooth display
void Freq_Update(void)
{
	if (SG.mode==6)
	{
		LCDGotoXY(0, 1);
		printf("    %5uHz", (uint16_t)SG.deltafreq);
	}
	if (SG.mode==8)
	{
	//if HS signal
		LCDGotoXY(0, 1);
		printf(" %5uMHz", SG.HSfreq);
	}
	if (SG.mode==9) { // PWM
		LCDGotoXY(0, 1);
		printf("%5uHz", SG.pwmFreq);
	}
	if((SG.mode==0)||(SG.mode==1)||(SG.mode==2)||(SG.mode==3)||(SG.mode==4)||(SG.mode==5))
		{
			LCDGotoXY(0, 1);
			printf(" %5uHz", (uint16_t)SG.freq);
		}
	_delay_ms(100);
}

//update duty value on LCD menu - more smooth display
void Duty_Update(void)
{
	if (SG.mode==9) { // PWM
		LCDGotoXY(8, 1);
		printf("%3u", SG.pwmDuty);
	}
	_delay_ms(100);
}

//External interrupt0 service routine
//used to stop DDS depending on active menu
//any generator is stopped by setting flag value to 0
//DDs generator which is inline ASM is stopped by setting
//CPHA bit in SPCR register
ISR(INT0_vect)
{
	SG.flag = 0;                        // set flag to stop generator
	SPCR   |= (1<<CPHA);                // using CPHA bit as stop mark
	SG.ON   = 0;                        // set off in LCD menu
	loop_until_bit_is_set(BPIN, START); // wait for button release
}

ISR(INT1_vect)
{
	SPCR |= (1<<CPHA);//using CPHA bit as stop mark
	//loop_until_bit_is_set(BPIN, START);//wait for button release
}

ISR(INT2_vect)
{
	SPCR   |= (1<<CPHA);                // using CPHA bit as stop mark
	//loop_until_bit_is_set(BPIN, START); // wait for button release
}

//timer overflow interrupt service tourine
//checks all button status and if button is pressed
//value is updated
void checkButtons(void)
{
if (SG.flag==0 && bit_is_clear(BPIN, UP))
//Button UP increments value which selects previous signal mode
//if first mode is reached - jumps to last
	{
	if (SG.mode==0)
	{ 
		SG.mode=MN_No-1;
	}
	else
	{
		SG.mode--;
	}
	//Display menu item
	Menu_Update(SG.ON);
	loop_until_bit_is_set(BPIN, UP);
	}
if (SG.flag==0 && bit_is_clear(BPIN, DOWN))
//Button Down decrements value which selects next signal mode
//if last mode is reached - jumps to first
	{
	if (SG.mode<(MN_No-1))
			{ 
				SG.mode++;
			}
		else
			{
				SG.mode=0;
			}
	//Display menu item
	Menu_Update(SG.ON);
	loop_until_bit_is_set(BPIN, DOWN);
	}
if (bit_is_clear(BPIN, RIGHT))
//frequency increment
	{
		if(SG.flag==0 && SG.mode==6)
		{
			if(SG.deltafreq==10000)
				SG.deltafreq=1;
			else
				SG.deltafreq=(SG.deltafreq*10);
			Freq_Update();
			loop_until_bit_is_set(BPIN, RIGHT);
		}
		if (SG.mode==8) { // high speed signal
			if(SG.HSfreq==8)
				SG.HSfreq=1;
			else
				SG.HSfreq=(SG.HSfreq<<1);
			Freq_Update();
			loop_until_bit_is_set(BPIN, RIGHT);
			if(SG.flag) {
				Timer1_Stop();
				Timer1_Start(SG.HSfreq);
			}
		}
		if (SG.mode==9) { // PWM
			if(!SG.flag) {
				switch(SG.pwmFreq) {
					case 61:    SG.pwmFreq = 244; break;
					case 244:   SG.pwmFreq = 976; break;
					case 976:   SG.pwmFreq = 7813; break;
					case 7813:  SG.pwmFreq = 62500; break;
					case 62500: SG.pwmFreq = 61; break;
				}
				Freq_Update();
				loop_until_bit_is_set(BPIN, RIGHT);
			}
			else {
				if(SG.pwmDuty < 255) ++SG.pwmDuty;
				OCR1A = SG.pwmDuty;
				Duty_Update();
				uint8_t ii=0;
				//press button and wait for long press (~0.5s)
				do{
					_delay_ms(2);
					ii++;
				}while((bit_is_clear(BPIN, RIGHT))&&(ii<=250));//wait for button release
				if(ii>=250)
				{
					do{
						if(SG.pwmDuty < 255) ++SG.pwmDuty;
						OCR1A = SG.pwmDuty;
						Duty_Update();
					}while(bit_is_clear(BPIN, RIGHT));//wait for button release
				}
			}
		}
		if((SG.mode==0)||(SG.mode==1)||(SG.mode==2)||(SG.mode==3)||(SG.mode==4)||(SG.mode==5))
			{
				if ((0xFFFF-SG.freq)>=SG.deltafreq)
					SG.freq+=SG.deltafreq;
				Freq_Update();
				uint8_t ii=0;
				//press button and wait for long press (~0.5s)
				do{
					_delay_ms(2);
					ii++;
				}while((bit_is_clear(BPIN, RIGHT))&&(ii<=250));//wait for button release
				if(ii>=250)
				{
					do{
						if ((0xFFFF-SG.freq)>=SG.deltafreq)
							SG.freq+=SG.deltafreq;
						Freq_Update();
					}while(bit_is_clear(BPIN, RIGHT));//wait for button release
				}
			}
	}
	
	if (bit_is_clear(BPIN, LEFT)) { //frequency decrement
		if(SG.flag==0 && SG.mode==6)
		{
			if(SG.deltafreq==1)
				SG.deltafreq=10000;
			else
				SG.deltafreq=(SG.deltafreq/10);
			Freq_Update();
			loop_until_bit_is_set(BPIN, LEFT);
		}
		if (SG.mode==8) { // high speed signal
			if(SG.HSfreq==1)
				SG.HSfreq=8;
			else
				SG.HSfreq=(SG.HSfreq>>1);
			Freq_Update();
			loop_until_bit_is_set(BPIN, LEFT);
			if(SG.flag) {
				Timer1_Stop();
				Timer1_Start(SG.HSfreq);
			}
		}
		if (SG.mode==9) { // PWM
			if(!SG.flag) {
				switch(SG.pwmFreq) {
					case 61:    SG.pwmFreq = 62500; break;
					case 244:   SG.pwmFreq = 61; break;
					case 976:   SG.pwmFreq = 244; break;
					case 7813:  SG.pwmFreq = 976; break;
					case 62500: SG.pwmFreq = 7813; break;
				}
				Freq_Update();
				loop_until_bit_is_set(BPIN, LEFT);
			}
			else {
				if(SG.pwmDuty > 0) --SG.pwmDuty;
				OCR1A = SG.pwmDuty;
				Duty_Update();
				uint8_t ii=0;
				//press button and wait for long press (~0.5s)
				do{
					_delay_ms(2);
					ii++;
				}while((bit_is_clear(BPIN, LEFT))&&(ii<=250));//wait for button release
				if(ii>=250)
				{
					do{
						if(SG.pwmDuty > 0) --SG.pwmDuty;
						OCR1A = SG.pwmDuty;
						Duty_Update();
					}while(bit_is_clear(BPIN, LEFT));//wait for button release
				}
			}
		}
		if ((SG.mode==0)||(SG.mode==1)||(SG.mode==2)||(SG.mode==3)||(SG.mode==4)||(SG.mode==5))
			{
				if (SG.freq>=SG.deltafreq)
					SG.freq-=SG.deltafreq;
				Freq_Update();
				uint8_t ii=0;
				//press button and wait for long press (~0.5s)
				do{
					_delay_ms(2);
					ii++;
				}while((bit_is_clear(BPIN, LEFT))&&(ii<=250));//wait for button release
				if(ii>=250)
				{
					do{
						if (SG.freq>=SG.deltafreq)
							SG.freq-=SG.deltafreq;
						Freq_Update();
					}while(bit_is_clear(BPIN, LEFT));//wait for button release
				}
			}
	}
	
	if(!SG.flag && bit_is_clear(BPIN, START)) {
		if(SG.mode != 6) {
			//saving last configuration
			SG.fr1=(uint8_t)(SG.freq);
			SG.fr2=(uint8_t)(SG.freq>>8);
			SG.fr3=(uint8_t)(SG.freq>>16);
			if (eeprom_read_byte((uint8_t*)EEMODE)!=SG.mode) eeprom_write_byte((uint8_t*)EEMODE,SG.mode);
			if (eeprom_read_byte((uint8_t*)EEFREQ1)!=SG.fr1) eeprom_write_byte((uint8_t*)EEFREQ1,SG.fr1);
			if (eeprom_read_byte((uint8_t*)EEFREQ2)!=SG.fr2) eeprom_write_byte((uint8_t*)EEFREQ2,SG.fr2);
			if (eeprom_read_byte((uint8_t*)EEFREQ3)!=SG.fr3) eeprom_write_byte((uint8_t*)EEFREQ3,SG.fr3);
			//Calculate frequency value from restored EEPROM values
			SG.freq=(((uint32_t)(SG.fr3)<<16)|((uint32_t)(SG.fr2)<<8)|((uint32_t)(SG.fr1)));
			SG.flag=1;//set flag to start generator
			SG.ON=1;//set ON on LCD menu
			//Stop timer2 - menu inactive
			Timer2_Stop();
			//display ON on LCD
			Menu_Update(SG.ON);
		}
		loop_until_bit_is_set(BPIN, START);//wait for button release
	}
}

//timer overflow interrupt service tourine
//checks all button status and if button is pressed
//value is updated
ISR(TIMER2_OVF_vect)
{
	checkButtons();
}

/*DDS signal generation function
Original idea is taken from
http://www.myplace.nu/avr/minidds/index.htm
small modification is made - added additional command which
checks if CPHA bit is set in SPCR register if yes - exit function
*/
void static inline Signal_OUT(const uint8_t *signal, uint8_t ad2, uint8_t ad1, uint8_t ad0)
{
asm volatile(	"eor r18, r18 	;r18<-0"	"\n\t"
				"eor r19, r19 	;r19<-0"	"\n\t"
				"1:"						"\n\t"
				"add r18, %0	;1 cycle"			"\n\t"
				"adc r19, %1	;1 cycle"			"\n\t"	
				"adc %A3, %2	;1 cycle"			"\n\t"
				"lpm 			;3 cycles" 	"\n\t"
				"out %4, __tmp_reg__	;1 cycle"	"\n\t"
				"sbis %5, 2		;1 cycle if no skip" "\n\t"
				"rjmp 1b		;2 cycles. Total 10 cycles"	"\n\t"
				:
				:"r" (ad0),"r" (ad1),"r" (ad2),"e" (signal),"I" (_SFR_IO_ADDR(PORTA)), "I" (_SFR_IO_ADDR(SPCR))
				:"r18", "r19" 
	);
}

void Timer1_Start(uint8_t FMHz)
{
switch(FMHz){
	case 1:
		//start high speed (1MHz) signal
		OCR1A=7;
		break;
	case 2:
		OCR1A=3;//2MHz
		break;
	case 4:
		OCR1A=1;//4MHz
		break;
	case 8:
		OCR1A=0;//8MHz
		break;
	default:
		OCR1A=7;//defauls 1MHz
		break;}
	//Output compare toggles OC1A pin
	TCCR1A=0x40;
	//start timer without prescaler
	TCCR1B=0b00001001;
}

void Timer1_StartPwm(uint8_t freqHz)
{
	uint8_t prescaler;
	switch(SG.pwmFreq) {
		case 61:    prescaler = 0b101; break;
		case 244:   prescaler = 0b100; break;
		case 976:   prescaler = 0b011; break;
		case 7813:  prescaler = 0b010; break;
		default:    prescaler = 0b001; break;
	}
	
	// Fast PWM 8 bit; non-inverting
	TCCR1A = (1<<WGM10) | (1<<COM1A1);
	TCCR1B = (1<<WGM12) | prescaler;
}

void Timer1_Stop(void)
{
	TCCR1B=0x00;//timer off
}
//main init function
void Main_Init(void)
{
//stderr = &lcd_str;
stdout = &lcd_str;
//--------init LCD----------
LCDinit();
LCDclr();
LCDcursorOFF();
//-------EEPROM initial values----------
if (eeprom_read_byte((uint8_t*)EEINIT)!='T')
{
eeprom_write_byte((uint8_t*)EEMODE,0x00);//initial mode 0 � OUT~~~~;
eeprom_write_byte((uint8_t*)EEFREQ1,0xE8);//initial frequency 1kHz
eeprom_write_byte((uint8_t*)EEFREQ2,0x03);
eeprom_write_byte((uint8_t*)EEFREQ3,0x00);
eeprom_write_byte((uint8_t*)EEINIT,'T');//marks once that eeprom init is done
//once this procedure is held, no more initialization is performed
}
//------restore last saved values from EEPROM------
SG.mode=eeprom_read_byte((uint8_t*)EEMODE);
SG.fr1=eeprom_read_byte((uint8_t*)EEFREQ1);
SG.fr2=eeprom_read_byte((uint8_t*)EEFREQ2);
SG.fr3=eeprom_read_byte((uint8_t*)EEFREQ3);
SG.freq=(((uint32_t)(SG.fr3)<<16)|((uint32_t)(SG.fr2)<<8)|((uint32_t)(SG.fr1)));
SG.acc=SG.freq/RESOLUTION;
SG.flag=0;
//default 1MHz HS signal freq
SG.HSfreq=1;
SG.deltafreq=100;

SG.pwmFreq = 62500;
SG.pwmDuty = 128;
//------------init DDS output-----------
R2RPORT=0x00;//set initial zero values
R2RDDR=0xFF;//set A port as output
//-------------set ports pins for buttons----------
BDDR&=~(_BV(START)|_BV(UP)|_BV(DOWN)|_BV(RIGHT)|_BV(LEFT));
BPORT|=(_BV(START)|_BV(UP)|_BV(DOWN)|_BV(RIGHT)|_BV(LEFT));

DDRB&=~(_BV(2));
PORTB=(_BV(2));
//---------set ports pins for HS output---------
HSDDR|=_BV(HS);//configure as output
//-----------Menu init--------------
SG.ON=0;//default signal is off
Menu_Update(SG.ON);
//-----------Timer Init-------------
Timer2_Init();
//Start Timer with overflow interrupts
Timer2_Start();
}

int main(void) {	
	Main_Init(); // Initialize
	while(1) {  // infinite loop 
		if(SG.flag) {
			GICR |= (1<<INT0) | (1<<INT1) | (1<<INT2); // set external interrupts to enable stop or modify
			if(SG.mode ==7 ) { // Noise
				while(SG.flag) {
					R2RPORT=rand();
				}
			}
			/*else if(SG.mode==6) { // freq step
				while((SG.flag==1));
			}*/
			else if(SG.mode == 8) { // High speed signal
				Timer2_Start();     // re-activate menu
				Timer1_Start(SG.HSfreq);
				while(SG.flag);
			}
			else if(SG.mode == 9) { // PWM
				Timer2_Start();     // re-activate menu
				OCR1A = SG.pwmDuty;
				Timer1_StartPwm(SG.pwmFreq);
				while(SG.flag);
			}
			else { // start DDS
				while(SG.flag) {
					SG.acc = SG.freq/RESOLUTION; // calculate accumulator value
					SPCR &= ~(1<<CPHA); // clear CPHA bit in SPCR register to allow DDS

					Signal_OUT(SIGNALS[SG.mode],
								(uint8_t)((uint32_t)SG.acc>>16),
								(uint8_t)((uint32_t)SG.acc>>8),
								(uint8_t)SG.acc);
					GICR &= ~((1<<INT0) | (1<<INT1) | (1<<INT2));   // stop external interrupts
					checkButtons();
					GICR |= (1<<INT0) | (1<<INT1) | (1<<INT2); // set external interrupts to enable stop or modify
				}
			}
			
			Timer1_Stop();        // timer off
			R2RPORT = 0x00;       // set signal level to 0
			Menu_Update(SG.ON);   // display generator OFF
			GICR &= ~((1<<INT0) | (1<<INT1) | (1<<INT2));   // stop external interrupts
			HSPORT &= ~(1<<HS);   // set HS pin to LOW
			Timer2_Start();       // start timer menu active
		}
	}
	return 0;
}
