#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "lcd.h"

#define F_CPU	8000000UL
#define F_PWM	1000U
#define N		8
#define TOP		(((F_CPU/F_PWM)/N)-1)
#define CONTRAST_DEF TOP/3

#define BAUD 9600
#define BRC ((F_CPU/16/BAUD)-1) //BAUD PRESCALAR (for Asynch. mode) 

//#define BRC (((F_CPU / (BAUD * 16))) - 1)


#define TX_BUFFER_SIZE 128
#define RX_BUFFER_SIZE 128
char serialBuffer[TX_BUFFER_SIZE];
char rxBuffer[RX_BUFFER_SIZE];

volatile uint8_t serialReadPos=0;
volatile uint8_t serialWritePos=0;
volatile uint8_t rxReadPos=0;
volatile uint8_t rxWritePos=0;

void UART_Init(void){
	/* Set baud rate*/
	UBRRH = 0;
	UBRRL = BRC;
	/*Enable reciver and transmitter*/
	UCSRB = (1 << RXEN) | (1 << TXEN)| (1<<RXCIE)| (1<<TXCIE);//|(1<<UDRIE); // interrupts
	/*set frame format: 8 bit*/
	
	UCSRC = (1 << UCSZ0) | (1 << UCSZ1)|(1 << URSEL); // Character Size 8 bit?
	/*UPM1 UPM0,  even-1 0 odd- 1 1 */
	//UCSRC = _BV(URSEL) | _BV(UCSZ0) | _BV(UCSZ1) ;
}

ISR(USART_UDRE_vect){
	
	lcd_puts_P("empty");
}

ISR(USART_TXC_vect){
	
	//lcd_puts("TX");
	if(serialReadPos != serialWritePos)
	{
		//char c= serialBuffer[serialReadPos];
		//lcd_puts(c);
		UDR = serialBuffer[serialReadPos];		
		serialReadPos++;
		
		if (serialReadPos>=TX_BUFFER_SIZE)
		{
			serialReadPos=0;
		}
	}
	
	
	
}

ISR(USART_RXC_vect){
	
	//lcd_puts("RX");
	lcd_putc(UDR);
	rxBuffer[rxWritePos] = UDR;
	
	//lcd_puts(UDR);
	//lcd_putc (rxBuffer[rxWritePos]);
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
		
		ret = rxBuffer[rxWritePos];
		rxReadPos++;
		
		if(rxReadPos<=RX_BUFFER_SIZE){
			rxReadPos=0;
		}
	}
	
	return ret;
}

volatile unsigned char USART_getc( void )
{
	
// Wait for data to be received
while ( !(UCSRA & _BV(RXC)) );
	//lcd_puts_P("0");
// Get and return received data from buffer
return UDR;

}



void USART_putc (char data) {
// Wait for empty transmit buffer
	while ( !( UCSRA & _BV(UDRE)) );
	// Put data into buffer, i.e., send the data
	UDR = data;
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

void USART_puts( char str[]){
	int i;

	for(i=0;i<strlen(str);i++){
		USART_putc(str[i]);
		_delay_ms(1000);
		
	}		
}

void main(void)
{
	IO_Init();
	UART_Init();	

	sei();
	//potreban delay!!
	char AT[] = "AT"; // 4 znamenke potrebne
	char text_mode[] = "AT+CMGF=1";
	
	char sms[]="AT+CMGS=";
	char num ="00385919390809";
	char sms_text[]="<3";
	
	//USART_putc(65);
	//_delay_ms(1000);
	//USART_putc(84);
	//_delay_ms(1000);
	//USART_putc(13);
	
	USART_puts(text_mode);
	USART_putc(13);
	
	USART_puts(sms);
	USART_putc(34);
	USART_puts(num);
	USART_putc(34);
	_delay_ms(1000);
	USART_putc(13);
	
	USART_puts(sms_text);
	USART_putc(26);// CTRL+z
	USART_putc(13);
		
	//char c= USART_getc();
	
	//USART_putc(AT);
	//USART_putc("A");
	//USART_putc("T");
	

	_delay_ms(200);
	

	
	while(1){}
	
} 


