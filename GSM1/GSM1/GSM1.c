#define F_CPU	8000000UL
#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <string.h>

#include "lcd.h"


#define F_PWM	1000Ubh
#define N		8
#define TOP		(((F_CPU/F_PWM)/N)-1)
#define CONTRAST_DEF TOP/3

#define BAUD 9600
#define BRC ((F_CPU/16/BAUD)-1) //BAUD PRESCALAR (for Asynch. mode) 

//#define BRC (((F_CPU / (BAUD * 16))) - 1)
//proba

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

volatile char numm[13]; 
volatile uint8_t i=4;
volatile uint8_t over=0;
uint8_t tm=0;
volatile uint8_t f=0;

ISR(USART_RXC_vect){
	
	//lcd_puts("RX");
	
	rxBuffer[rxWritePos] = UDR;
	//rxBuffer[rxWritePos+1] = '/0';
	
	if(rxBuffer[rxWritePos]=='5'){
		if(rxBuffer[rxWritePos-1]=='8'){
			f=1;
			/*
			numm[3] = rxBuffer[rxWritePos];
			numm[2] = rxBuffer[rxWritePos-1];
			numm[1] = rxBuffer[rxWritePos-2];
			numm[0] = rxBuffer[rxWritePos-3];
			numm[0] = rxBuffer[rxWritePos-4];
			*/
		numm[0]='+';
		numm[1]='3';
		numm[2]='8';
		i=3;
				
		}
	}
	
	if(f==1 && i<13 ){
		numm[i]=rxBuffer[rxWritePos];
		//lcd_putc(numm[i]);
		i++;
	}
	if(rxBuffer[rxWritePos]=='K' && rxBuffer[rxWritePos-1]=='O'){
		
		lcd_puts("ignor");
		tm=1;
	}
	
	if( tm==1 && rxBuffer[rxWritePos]=='K' && rxBuffer[rxWritePos-1]=='O'){
		
		lcd_puts("gotovo");
		over=1;
		tm=0;
	}
	
	//lcd_putc(UDR);
	lcd_putc (rxBuffer[rxWritePos]);
	rxWritePos++;
	
	if(rxWritePos>=RX_BUFFER_SIZE){
		rxWritePos=0;
	}
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

volatile char buffer[4];
void ADC_Init(){
	

	//adc enable, prescaler=64 -> clk=115200
	ADCSRA = _BV(ADEN)|_BV(ADPS2)|_BV(ADPS1);

	//AVcc reference voltage
	ADMUX = _BV(REFS0);
}
volatile char *getLight(uint8_t channel)
{
	//choose channel
	ADMUX &= ~(0x7);
	ADMUX |= channel;
	ADMUX |= 1<<ADLAR;
	
	//start conversion
	ADCSRA |= _BV(ADSC);

	//wait until conversion completes
	while (ADCSRA & _BV(ADSC) );
	
	 char ispis[10];
	
	
	strcpy(ispis," ");
// 	for (i = 0; i < 10; i++) {
// 		ispis[i]='\0';
// 	}
// 	
	*ispis=itoa(ADCH,ispis,10);
	
			if (ADCH<200)
			{
				lcd_clrscr();
				//strcpy(ispis, "noc");
				strcat(ispis, " noc");
			}
			else
			{
				lcd_clrscr();
				strcat(ispis, " dan");
				
			}
			_delay_ms(750);
		

	
	return ispis;
}
int main(void)
{
	IO_Init();
	UART_Init();
	ADC_Init();	

	sei();
	
	//potreban delay!!
	char AT[] = "AT"; // 4 znamenke potrebne
	char text_mode[] = "AT+CMGF=1";
///SMS	
	char sms[]="AT+CMGS=";
	char *num ="00385919390809";
	//char sms_text[30];
	
//TEXT MODE
	USART_puts(text_mode);
	USART_putc(13);
	_delay_ms(1000);
	/*
	USART_puts(sms);
	USART_putc(34);
	USART_puts(num);
	USART_putc(34);
	_delay_ms(1000);
	USART_putc(13);
	
	USART_puts(sms_text);
	USART_putc(26);// CTRL+z
	USART_putc(13);
	*/
/// READ MESSAGE
	
	//zahtjev za poruku  s indexom 1
		//lcd_puts("blabla");
	char request[]="at+cmgr=1";	
	char number[9];
	
	

// 	USART_puts(request);
// 	USART_putc(13);
// 	_delay_ms(5000);
// 	
	rxWritePos=0;
	/*
	if (strstr(rxBuffer, "+385")) {
		p = strstr(rxBuffer, "+385"); 
		p = strchr(p, "3");
		//lcd_puts(p);
		strncpy(number, p, 9);
	}
	*/
	//lcd_puts(numm);
	
	while(1){
		
	 	USART_puts(request);
	 	USART_putc(13);
		 _delay_ms(5000);
		

		
		if(over){
			
		
				over=0;
			
	
			if (strstr(rxBuffer, "LUX?")) {
				
				lcd_gotoxy(0,1);
				char *sms_text="IS: ";
			
				volatile char *rez= getLight(1);
				
				char *sms_full_text=strcat(sms_text,rez);
				
				lcd_puts(sms_full_text);	
	/*
				lcd_puts(sms);	
				USART_puts(sms);
			
				lcd_putc(34);
				USART_putc(34);
				lcd_puts(numm);
				USART_puts(numm);
			
				lcd_putc(34);
				USART_putc(34);
				_delay_ms(1000);
			
				lcd_putc(13);
				USART_putc(13);
				lcd_puts(sms_text);	

				USART_puts(sms_text);
			
				USART_putc(26);// CTRL+z
				USART_putc(13);
				
	*/			
				lcd_gotoxy(0,1);
				lcd_puts_P("poslano");
			
				char delete[]="at+cmgd=1";
				USART_puts(delete);
				USART_putc(13);
				_delay_ms(3000);
				int i;
				for (i=0; i < 128; i++) {
					rxBuffer[i] = '0';		
				}
				
			
				over=0;
			
				
				
			}else{
				
			lcd_puts("Nema poruke");
			}
		}
		
		
		
		}
	return 0;
} 
