#define  F_CPU	        8000000UL

#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <string.h>

#include "lcd.h"

#define F_PWM	        1000Ubh
#define N		        8
#define TOP		        (((F_CPU/F_PWM)/N)-1)
#define CONTRAST_DEF    TOP/3

#define BAUD            9600
#define BRC             ((F_CPU/16/BAUD)-1) //BAUD PRESCALAR (for Asynch. mode)
#define TX_BUFFER_SIZE  128
#define RX_BUFFER_SIZE  255

char txBuffer[TX_BUFFER_SIZE];
char rxBuffer[RX_BUFFER_SIZE];

volatile uint8_t txReadPos=0;
volatile uint8_t txWritePos=0;
volatile uint8_t rxReadPos=0;
volatile uint8_t rxWritePos=0;

//Init IO UART and ADC
static void IO_Init(void)
{
	// initialize display, cursor off
	lcd_init(LCD_DISP_ON);

	// clear display and home cursor
	lcd_clrscr();

    // put string to display (line 1) with linefeed
    // lcd_puts_P("Hello world\n");
}
static void UART_Init(void)
{
	/* Set baud rate*/
	UBRRH = 0;
	UBRRL = BRC;

	/*Enable reciver and transmitter*/
	UCSRB = (1 << RXEN) | (1 << TXEN)| (1<<RXCIE)| (1<<TXCIE);//|(1<<UDRIE); // interrupts

	/*set frame format: 8 bit*/
	UCSRC = (1 << UCSZ0) | (1 << UCSZ1)|(1 << URSEL); // Character Size 8 bit
	/*UPM1 UPM0,  even-1 0 odd- 1 1  ? sta je ovo, je to potrebno?*/
}
static void ADC_Init()
{
	//adc enable, prescaler=64 -> clk=115200
	ADCSRA = _BV(ADEN)|_BV(ADPS2)|_BV(ADPS1);

	//AVcc reference voltage
	ADMUX = _BV(REFS0);
}
//U(S)ART serial communication functions and interrupts
void USART_putc (char data)
{
	// Wait for empty transmit buffer
	while ( !( UCSRA & _BV(UDRE)) );

	// Put data into buffer, i.e., send the data
	lcd_putc(data);
	UDR = data;
}
void USART_puts(char str[])
{
	int i;

	for(i=0; i<strlen(str); i++)
	{
		USART_putc(str[i]);
		_delay_ms(1000);
	}
}

ISR(USART_TXC_vect)
{
	if(txReadPos != txWritePos)
	{
		UDR = txBuffer[txReadPos];
		//lcd_putc(txBuffer[txReadPos]);
		txReadPos++;

		if (txReadPos>=TX_BUFFER_SIZE)
		{
			txReadPos=0;
		}
	}

}
volatile uint8_t tm_flag=0; //text mode flag: 1 - request sent 2 - OK answer recieved
volatile uint8_t rq_flag=0; //text mode flag: 1 - command sent 2 - OK answer recieved
ISR(USART_RXC_vect)
{
	rxBuffer[rxWritePos] = UDR;

	if(rxBuffer[rxWritePos]=='K' && rxBuffer[rxWritePos-1]=='O')
    {
		if(tm_flag==1){
		
			lcd_puts("text mode");
			tm_flag=2;
		}
		
		if(rq_flag==1){
			//lcd_puts("msg read");
			rq_flag=2;
		}
	}
	
	lcd_putc (rxBuffer[rxWritePos]);
	rxWritePos++;

	if(rxWritePos>=RX_BUFFER_SIZE)
    {
		rxWritePos=0;
	}
}
//empty rxBuffer
void refresh_rxBuffer(){
	*rxBuffer='\0';
	rxReadPos=0;
	rxWritePos=0;
}

int enable_text_mode(void){

char text_mode[] = "AT+CMGF=1";
	
		_delay_ms(3000);
		USART_puts(text_mode);
		tm_flag=1;
		USART_putc(13); //enter
		_delay_ms(3000);
	
	if (tm_flag==2) return 1;
	
	else return 0;
}

void request_sms(char index){
	char request[]="at+cmgr=";
	USART_puts(request);
	USART_putc(index);
	lcd_clrscr();
	rq_flag=1;
	USART_putc(13); //enter
	_delay_ms(3000);
}

void delete_sms(char index){
	
	char delete[]="at+cmgd=";
	USART_puts(delete);
	USART_putc(index);
	USART_putc(13);
	_delay_ms(3000);
	
}

//volatile char from_number[13];
char from_number[13];
int get_from_number(int read ){
	
	lcd_clrscr();
	
	int digit_pos=24; //UNREAD
	
	if(read==2) digit_pos=22; //READ
	
	int i=0, j=0;
	
	for(i=digit_pos;i<34;i++){
		from_number[j]=rxBuffer[i];
		j++;
	}
	return 1;
	
}
volatile int sms_flag=0;
int read_RxBuffer(void){

	lcd_clrscr();
	
	if(strstr(rxBuffer,"321")){
		lcd_puts("no message");
		return 1; // ERROR 321
	} else if (strstr(rxBuffer,"OK")) {
		lcd_puts("ok"); //OK
	}else{
		lcd_puts("ERROR 4");
		return 0; //ERROR 4
		} 
	int read=-1;
	if(strstr(rxBuffer,"READ")){		
		read=0;
		sms_flag=1;
	}
	
	if(strstr(rxBuffer,"UNREAD")){
		read=1; // UNREAD message found, number starts at position 21
	} else  read=2; //READ message found number starts at position 19
	
	_delay_ms(1000);
	
	lcd_clrscr();
	
	while(rxReadPos!=rxWritePos){
		
	if(rxReadPos%16==0 || rxReadPos==0) lcd_gotoxy(0,0);
		lcd_putc(rxBuffer[rxReadPos]);
		rxReadPos++;	
		_delay_ms(200);
	
	}
	lcd_clrscr();
	
	if(read==1) lcd_puts(":)");
	_delay_ms(1000);
	lcd_clrscr();
	
	if(read){
		get_from_number(read);
		return 1;
	}
	return 0;
}
volatile uint8_t getLight(uint8_t channel)
{
	//choose channel
	ADMUX &= ~(0x7);
	ADMUX |= channel;
	ADMUX |= 1<<ADLAR;

	//start conversion
	ADCSRA |= _BV(ADSC);

	//wait until conversion completes
	while (ADCSRA & _BV(ADSC) );
	return ADCH;
}
void LUX(){
	//sms define
	char sms[]="AT+CMGS="; //sms command
	char *sms_text="Intenzitet: ";
	char rez[10];
	char *dannoc="san"; //ne definiran
	uint8_t adch=0;
	
	if(strstr(rxBuffer, "LUX?")){
				
			lcd_gotoxy(0,1);
			adch=getLight(1);
				
			if (adch>200){	
				dannoc=" dan";
			}else if(adch<200){
				dannoc=" noc";
			}
				
			itoa(adch,rez,10);
				
			lcd_gotoxy(0,0);
			lcd_puts(rez);
			lcd_puts(dannoc);
			
			_delay_ms(2000);
			lcd_clrscr();
			lcd_gotoxy(0,0);
				
			lcd_puts(sms);
			USART_puts(sms);
			lcd_putc(34);
			USART_putc(34);
				
			lcd_puts(from_number);
			USART_puts(from_number);
			lcd_gotoxy(0,1);
			lcd_putc(34);
			USART_putc(34);
			_delay_ms(2000);
			lcd_gotoxy(0,1);
			lcd_putc(13);
			USART_putc(13);
			_delay_ms(1000);

			lcd_puts(sms_text);
			USART_puts(sms_text);
			lcd_puts(rez);
			USART_puts(rez);
			lcd_puts(dannoc);
			USART_puts(dannoc);
				
			USART_putc(26);// CTRL+z
			USART_putc(13);
			_delay_ms(3000);
	
			lcd_gotoxy(0,1);
			lcd_puts_P("poslano");
					
		
		}	

}
int main(void)
{
	IO_Init();
	UART_Init();
	ADC_Init();
	sei();
	
	_delay_ms(3000);
	
	//enter TEXT MODE
	while(!enable_text_mode());
	
	//refresh (empty) rxBuffer
	refresh_rxBuffer();
	char index='1';

	//check 9 messages
	for(index='1';index!='9';index++){
			
		//until there is "OK" or "321" in rxBuffer, request for message
	
		request_sms(index);	
	
		while(!read_RxBuffer()){
			request_sms(index);
			refresh_rxBuffer();
		}
		
		if(sms_flag){			
			LUX();
			
			delete_sms(index);
			refresh_rxBuffer();
			sms_flag=0;
			*from_number='\0';
		}
		
	}
/// DEFINE AND SEND SMS PROBE
    /*
	char sms[]="AT+CMGS=";
	char *num ="00385996834050";
    char sms_text[30]="Probni sms";

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
/*	
	char request[]="at+cmgr=1";//zahtjev za poruku  s indexom 1
    USART_puts(request);
    USART_putc(13);
    _delay_ms(5000);
*/

}
