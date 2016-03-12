
#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

#include "lcd.h"

#define F_CPU			8000000UL		    	// CPU clock in Hertz
#define F_PWM			1000U					// PWM freq ~ 1kHz    dal nam uopce treba pwm ?
#define N				8
#define TOP				(((F_CPU/F_PWM)/N)-1)
#define CONTRAST_DEF	TOP/3

#define BAUD			9600					// bits per second
#define BRC				((F_CPU/16/BAUD)-1)		//BAUD PRESCALAR Asynch. mode
#define TX_BUFFER_SIZE	128
#define RX_BUFFER_SIZE	128


char txBuffer[TX_BUFFER_SIZE];
char rxBuffer[RX_BUFFER_SIZE];

uint8_t txReadPos=0;
uint8_t txWritePos=0;

uint8_t rxReadPos=0;
uint8_t rxWritePos=0;

static void IO_Init(void)
{
	lcd_init(LCD_DISP_ON);
	lcd_clrscr();
}

void appendtx(char c);
void txWrite(char c[]);
char peekChar(void);
char getChar(void);
unsigned char USART_getc (void);
void USART_putc (unsigned char data);



void main(void)
{
	//IO_Init();
	
	// initialize USART
	UBRRH = (BRC>>8);
	UBRRL = BRC;
	
	UCSRB = (1 << RXEN) | (1 << TXEN)| (1<<RXCIE)| (1<<TXCIE);
	UCSRC =  (1 << UCSZ0) | (1 << UCSZ1); // Character Size
	
	// enable interrupt
	sei();
	
	// AT commands -> šalje ih mikrokontroler a GSM prima, šalje potvrdu da je primio i prikazuje na LCD
	char AT[] = "AT";
	char text_mode[] = "AT + CMGF = 1";
	char sms[]="AT+CMGS='00385996834050'+SMS";
	char charater_mode[] = "AT + CMCS = 'GSM'";
	
	//	UDR = "AT + CMGF = 1";
	//	USART_putc(AT);
	//	USART_putc(text_mode);
	//	USART_putc(charater_mode);
	

	//txWrite(text_mode);
	//txWrite(0x0D); //enter after AT!
	USART_putc(AT);
	USART_putc(0x0D);
	
	_delay_ms(200);
	
	char c= USART_getc();
	
	//if(c) lcd_clrscr();
	//lcd_puts(c);
	
	while (1) {
		
		//unsigned char a = USART_getc();
		//lcd_puts (a);
		
		
		//rxBuffer
		
		//if(rxBuffer[k-1]="O");
	}
	
	
}


char peekChar(void){
	char ret= '\0';
	
	if(rxReadPos!=rxWritePos){
		ret= rxBuffer[rxWritePos];
	}
	return ret;
	
}

char getChar(void){ // to s interruptom je isto kao i USART_getc
	
	char ret= '\0';
	if(rxReadPos!=rxWritePos){	
		ret= rxBuffer[rxWritePos];
		rxReadPos++;
		
		if(rxReadPos<=RX_BUFFER_SIZE){
			rxReadPos=0;
		}
	}
	
	return ret;
}

void appendtx(char c){ //to i txWrite i interrupt je isto kao i putc
	
	txBuffer[txWritePos]=c;
	txWritePos++;
	
	if(txWritePos>=TX_BUFFER_SIZE){
		txWritePos=0;
	}
}

void txWrite(char c[]){
	
	for(uint8_t i=0;i<strlen(c);i++)
	{
		appendtx(c[i]);
		if(UCSRA & (1<<UDRE)){
			UDR=0;
		}

	}
}

unsigned char USART_getc (void) {
	while (!(UCSRA & _BV(RXC)));
	lcd_puts_P("get");
	return UDR;
	
}

void USART_putc (unsigned char data) {
	while (!(UCSRA & _BV(UDRE)));
	UDR = data;
	lcd_puts_P("put");
}



ISR(USART_TXC_vect){
	//lcd_puts_P("TX");
	
	if(txReadPos!=txWritePos)
	{
		UDR=txBuffer[txReadPos];
		lcd_puts_P("TX");
		//lcd_puts_P(txBuffer[txReadPos]);
		txReadPos++;
		if (txReadPos>=TX_BUFFER_SIZE){
			txReadPos=0;
		}
	}

}

ISR(USART_RXC_vect){
	
	rxBuffer[rxWritePos]= UDR;
	lcd_puts_P("RX");
	
	rxWritePos++;
	
	if(rxWritePos>=RX_BUFFER_SIZE){
		rxWritePos=0;
	}
}





