#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "lcd.h"
#include <string.h>
//ovo je proba LALALALA
#define F_CPU	8000000UL
			
#define F_PWM	1000U
#define N		8
#define TOP		(((F_CPU/F_PWM)/N)-1)
#define CONTRAST_DEF TOP/3

#define BAUD 9600
//#define BRC ((F_CPU/16/BAUD)-1) //BAUD PRESCALAR (for Asynch. mode) 

#define BRC (((F_CPU / (BAUD * 16))) - 1)

#define TX_BUFFER_SIZE 128
#define RX_BUFFER_SIZE 128
volatile char serialBuffer[TX_BUFFER_SIZE];
volatile char rxBuffer[RX_BUFFER_SIZE];
volatile uint8_t serialReadPos=0;
volatile uint8_t serialWritePos=0;
volatile uint8_t rxReadPos=0;
volatile uint8_t rxWritePos=0;

void UART_Init(void){
	/* Set baud rate*/
	UBRRH = (BRC>>8);
	UBRRL = BRC;
	/*Enable reciver and transmitter*/
	UCSRB = (1 << RXEN) | (1 << TXEN)| (1<<RXCIE)| (1<<TXCIE); // interrupts
	/*set frame format: 8 bit*/
	
	UCSRC =  (1 << UCSZ0) | (1 << UCSZ1); // Character Size 8 bit?
	/*UPM1 UPM0,  even-1 0 odd- 1 1 */
}


void appendSerial(char c){
	
	//lcd_puts("a");
	serialBuffer[serialWritePos]=c;	
	serialWritePos++;
	
	if(serialWritePos>=TX_BUFFER_SIZE)
	{
	serialWritePos=0;
	}	
}

void serialWrite(char c[]){
	
	for(uint8_t i = 0;i <strlen(c);i++)
	{
		appendSerial(c[i]);
	
	}
		
		if(UCSRA & (1<<UDRE))
		{
			UDR=0;
		}

}

ISR(USART_TXC_vect){
	
	if(serialReadPos != serialWritePos)
	{
		//char c= serialBuffer[serialReadPos];
		//lcd_puts(c);
		UDR = serialBuffer[serialReadPos];
		lcd_puts (serialBuffer[serialReadPos]);
		//lcd_puts_P("TX");
		serialReadPos++;
		
		if (serialReadPos>=TX_BUFFER_SIZE)
		{
			serialReadPos=0;
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


char peekChar(void){
	char ret= '\0';
	
	if(rxReadPos!=rxWritePos){
		
		ret= rxBuffer[rxWritePos];
		}
	return ret;
	
}

char getChar(void){
	
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


static void IO_Init(void)
{
	/* initialize display, cursor off */
	lcd_init(LCD_DISP_ON);
	/* clear display and home cursor */
	lcd_clrscr();
    /* put string to display (line 1) with linefeed */
   // lcd_puts_P("Hello world\n");	
}



void main(void)
{
	IO_Init();
	UART_Init();	

	sei();
	/*	
	if (bit_is_set(UCSRA,TXC))
	{
		lcd_puts_P("TXCset");
	}
	
	UDR = 'A';
	_delay_ms(1000);

	
	UDR = 'T';
	_delay_ms(1000);	
	
	UDR = 'E';
	_delay_ms(1000);
	
	char c = UDR;
	*/
	
	char AT[] = "abcdefgh";
	char text_mode[] = "AT + CMGF = 1";
	char sms[]="AT+CMGS='00385996834050'+SMS";
	char charater_mode[] = "AT + CMCS = 'GSM'";
	
//	UDR = "AT + CMGF = 1";
	
//	USART_putc(AT);
//	USART_putc(text_mode);
//	USART_putc(charater_mode);
	

	serialWrite(AT);
//	serialWrite(0x0D); //enter after AT!
	
	//USART_putc(AT);
	//USART_putc(0x0D);
	_delay_ms(200);
		
	if (bit_is_set(UCSRA,TXC))
	{
		lcd_puts_P("TXCset");
	}
	
	//char c= USART_getc();
	char c= getChar();
	
	_delay_ms(200);
	

	
	if (bit_is_set(UCSRA,RXC))
	{
		lcd_puts_P("RXCset");
	}
	
	while(1){
	
	}
	
	
} 


