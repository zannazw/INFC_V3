/*
 */

#define F_CPU 16000000UL
#include <xc.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#define SPI_DDR DDRB
#define SS      PINB2
#define MOSI    PINB3
#define SCK     PINB5
#define D_C		PIND2		//display: Data/Command
#define Reset	PIND3		//display: Reset

#define BUTTON1_PRESSED !(PIND & (1 << PIND1))
#define BUTTON2_PRESSED !(PINB & (1 << PINB1))

volatile uint16_t counter;

void SPISend8Bit(uint8_t data);
void SendCommandSeq(const uint16_t * data, uint32_t Anzahl);

ISR(TIMER1_COMPA_vect){
	counter++;	
}

void Waitms(const uint16_t msWait){
	static uint16_t aktTime, diff;
	uint16_t countertemp;
	cli();              //da 16 bit Variablen könnte ohne cli() sei() sich der Wert
	aktTime = counter;  //von counter ändern, bevor er komplett aktTime zugewiesen wird.
	sei();              //Zuweisung erfolgt wg. 8 bit controller in 2 Schritten. 
	do {
			cli();
			countertemp = counter;
			sei();
			  diff = countertemp + ~aktTime + 1;
	  } while (diff	< msWait); 	
}

void init_Timer1(){
	TCCR1B |= (1<<CS10) | (1<<WGM12);	// TimerCounter1ControlRegisterB Clock Select |(1<<CS10)=>prescaler = 1; WGM12=>CTC mode
	TIMSK1 |= (1<<OCIE1A);				// TimerCounter1 Interrupt Mask Register: Output Compare Overflow Interrupt Enable
	OCR1A = 15999;						// direkte Zahl macht Sinn; overflow register OCR1A berechnet mit division 64 => unlogischer Registerwert
}

void SPI_init()
{
	//set CS, MOSI and SCK to output
	SPI_DDR |= (1 << SS) | (1 << MOSI) | (1 << SCK);

	//enable SPI, set as master, and clock to fosc/4 or 128
	SPCR = (1 << SPE) | (1 << MSTR);// | (1 << SPR1) | (1 << SPR0); 4MHz bzw. 125kHz
	//SPSR |= 0x1;
}

void SPISend8Bit(uint8_t data){
	//	farbe = data;
	PORTB &= ~(1<<SS);				//CS (SS) low
	SPDR = data;					//load data into register
	while(!(SPSR & (1 << SPIF)));	//wait for transmission complete
	PORTB |= (1<<SS);				//CS high
}

void SendCommandSeq(const uint16_t * data, uint32_t Anzahl){
	uint32_t index;
	uint8_t SendeByte;
	for (index=0; index<Anzahl; index++){
		PORTD |= (1<<D_C);						//Data/Command auf High => Kommando-Modus
		SendeByte = (data[index] >> 8) & 0xFF;	//High-Byte des Kommandos
		SPISend8Bit(SendeByte);
		SendeByte = data[index] & 0xFF;			//Low-Byte des Kommandos
		SPISend8Bit(SendeByte);
		PORTD &= ~(1<<D_C);						//Data/Command auf Low => Daten-Modus
	}
}

void Display_init(void){
	const uint16_t InitData[] ={
		//Initialisierungsdaten fuer 16 Bit Farben Modus
		0xFDFD, 0xFDFD,
		//pause
		0xEF00, 0xEE04, 0x1B04, 0xFEFE, 0xFEFE,
		0xEF90, 0x4A04, 0x7F3F, 0xEE04, 0x4306, //0x7F3F für 16-Bit Farbenmodus
		//pause
		0xEF90, 0x0983, 0x0800, 0x0BAF, 0x0A00,
		0x0500, 0x0600, 0x0700, 0xEF00, 0xEE0C,
		0xEF90, 0x0080, 0xEFB0, 0x4902, 0xEF00,
		0x7F01, 0xE181, 0xE202, 0xE276, 0xE183,
		0x8001, 0xEF90, 0x0000,
		//pause
		0xEF08,	0x1800,	0x1200, 0x1583,	0x1300, //ganzer Bildschrirm
		0x16AF 	//Hochformat 132 x 176 Pixel
	};
	Waitms(300);
	PORTD &= ~(1<<Reset);	//Reset-Eingang des Displays auf Low => Beginn Hardware-Reset
	Waitms(75);
	PORTB |= (1<<SS);		//SSEL auf High
	Waitms(75);
	PORTD |= (1<<D_C);		//Data/Command auf High
	Waitms(75);
	PORTD |= (1<<Reset);	//Reset-Eingang des Displays auf High => Ende Hardware Reset
	Waitms(75);
	SendCommandSeq(&InitData[0], 2);
	Waitms(75);
	SendCommandSeq(&InitData[2], 10);
	Waitms(75);
	SendCommandSeq(&InitData[12], 23);
	Waitms(75);
	SendCommandSeq(&InitData[35], 6); //Neu: wollen 6 Wörter schreiben als Kommand
}
void init_Buttons(){
    //Configure Button1 and Button2 as Input
    DDRD &= ~(1 << DDD1); //PortD1 = Button1
    DDRB &= ~(1 << DDB1); //PortB1 = Button2
    
    //Enable Pull-Up
    PORTD |= (1 << PORTD1); 
    PORTB |= (1 << PORTB1); 
}

void init_Timer0(){
    sei(); //Enable Global Interrupt Bit 
    
    TIMSK0 |= (1 << OCIE0A); //Enable Timer0 Compare Match A Interrupt 
    
    //Set CTC Mode 
    TCCR0A |= (1 << WGM01); 
    TCCR0A &= ~(1 << WGM00); 
    TCCR0B &= ~(1 << WGM02); 
    
    OCR0A = 250; //Timer Match A after 1000ms
    
    //Set Prescaler at 64 and start Timer
    TCCR0B |= (1 << CS00) | (1 << CS01);
    TCCR0B &= ~(1 << CS02); 
}

ISR(TIMER0_COMPA_vect){
    static volatile uint8_t debounceCounter = 0;
    static volatile uint8_t buttonPressedLongEnough = 0;
 
    if(BUTTON1_PRESSED || BUTTON2_PRESSED) {
        if(debounceCounter < 80) { //Button is pressed for the length of 80ms 
            debounceCounter++; 
        } else {
            buttonPressedLongEnough = 1; 
        }
    } else { //Button is not pressed or pressed for too short a duration 
        debounceCounter = 0; 
        buttonPressedLongEnough = 0; 
    }
    
    if(buttonPressedLongEnough == 1) { //button held stable in pressed state 
        if (BUTTON1_PRESSED) {
            setNewLeftPixel();
        } else {
            setNewRightPixel();
        }
    }
}

static uint16_t WindowData[] = {
    0xEF08,	0x1800,	
    0x1223, 0x1531,	0x135A, 0x166D 
};

static uint16_t WindowDataFixedLeft[] = {
    0xEF08,	0x1800,	
    0x1200, 0x150E,	0x135A, 0x166D 
};

static uint16_t WindowDataFixedRight[] = {
    0xEF08,	0x1800,	
    0x1275, 0x1583,	0x135A, 0x166D 
};

void setNewLeftPixel(void){
    clearOldPixel();
    
    //Kommando-Modus
    if (WindowData[2] > 0x1200 && WindowData[3] > 0x150E){
        for(uint8_t i = 2; i < 4; i++){
           WindowData[i]--;  
        }
        SendCommandSeq(WindowData,6);
    } else {
        SendCommandSeq(WindowDataFixedLeft, 6);
    }

    //Daten-Modus
    for(uint16_t i = 0; i < 15*20; i++){
        SPISend8Bit(0x07);
        SPISend8Bit(0xE0);
    }
}

void setNewRightPixel(void){
    clearOldPixel();
    
    //Kommando-Modus
    if (WindowData[2] < 0x1275 && WindowData[3] < 0x1583){
        for(uint8_t i = 2; i < 4; i++){
           WindowData[i]++;  
        }
        SendCommandSeq(WindowData,6);
    } else {
        SendCommandSeq(WindowDataFixedRight, 6);
    }

    //Daten-Modus
    for(uint16_t i = 0; i < 15*20; i++){
        SPISend8Bit(0x07);
        SPISend8Bit(0xE0);
    }
}

void clearOldPixel(void) {
    //Kommando-Modus für Löschung
    SendCommandSeq(WindowData, 6); 
    
    //Daten-Modus für Löschung
    for(uint16_t i = 0; i < 15*20; i++){
        SPISend8Bit(0xFF);
        SPISend8Bit(0xE0);
    }
}

int main(void){
	DDRB &= ~(1<<PORTB1);
	PORTB |= (1<<PORTB1);
	DDRD &= ~(1<<PORTD1);
	PORTD |= (1<<PORTD1);
	DDRD |= (1<<D_C)|(1<<Reset);		//output: PD2 -> Data/Command; PD3 -> Reset

	init_Timer1();
	SPI_init();
	sei();
	Display_init();
    
	for(uint16_t i = 0; i < 132*176; i++){
        //Ganze Zeit gelb senden im 16 Bit Modus
        //Sind bereits im Datenmodus
        SPISend8Bit(0xFF); //send yellow
        SPISend8Bit(0xE0);
    }
    
    SendCommandSeq(WindowData, 6);
    
    for(uint16_t i = 0; i < 15*20; i++){
        //Grün senden im 16 Bit Modus
        SPISend8Bit(0x07);
        SPISend8Bit(0xE0);//send green
    }
    
    init_Buttons();
    init_Timer0();
    
	while(1){
    }
}
