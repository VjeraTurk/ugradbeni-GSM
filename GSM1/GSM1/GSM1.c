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

//Init IO, UART and ADC
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
//USART serial communication functions and interrupts
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
		_delay_ms(500);
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
volatile uint8_t tm_flag=0; //text mode flag: 1 - request sent 2 - OK answer received
volatile uint8_t rq_flag=0; //text mode flag: 1 - command sent 2 - OK answer received
volatile uint8_t del_flag=0; //text mode flag: 1 - command sent 2 - OK answer received
ISR(USART_RXC_vect)
{
	rxBuffer[rxWritePos] = UDR;
	//na vrhu
 	//lcd_putc (rxBuffer[rxWritePos]);
	
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
	
		if(del_flag==1){
			//lcd_puts("deleted");
			del_flag=2;
		}
	
	}
	
	rxWritePos++;
	if(rxWritePos>=RX_BUFFER_SIZE)
    {
		rxWritePos=0;
	}
}
/*
int enable_text_mode(void)

Function sends AT command for entering text mode to GSM, and sets (3 state) tm_flag to 1. The check if OK response from GSM is received is in ISR_RXC. 
OK response changes tm_flag in ISR_RXC. After 'O' 'K' is recieved tm_flag is set to 2. Back in this function, after waiting for answer from GSM in _delay_ms(),
if tm_flag equals 2 (meaning OK is received) return 1, else return 0. If returning 0 function is called all over again from main until 1 is received.
while(!enable_text_mode());

int tm_flag
= 0 command not yet sent
= 1 command sent
= 2 text mode entered

return 0; OK not received
return 1; OK received
*/
int enable_text_mode(void){

char text_mode[] = "AT+CMGF=1";
	
		_delay_ms(3000);
		USART_puts(text_mode);
		tm_flag=1;
		USART_putc(13); //ENTER
		_delay_ms(3000);
		lcd_clrscr();
	if (tm_flag==2) return 1;
	
	else return 0;
}
void request_sms(char index){
	
	char request[]="at+cmgr=";
	USART_puts(request);
	USART_putc(index);
	lcd_clrscr();
	rq_flag=1;
	USART_putc(13); //ENTER
	_delay_ms(3000);
	
}
void delete_sms(char index){
	
	char delete[]="at+cmgd=";
	USART_puts(delete);
	USART_putc(index);
	del_flag=1;
	USART_putc(13);
	_delay_ms(3000);
	if(del_flag==2) {
		del_flag=0;
		lcd_puts("deleted");
		_delay_ms(1000);
	
	}
		
}
/*	void refresh_rxBuffer()
Function "empties" the rxBuffer and sets R/W positions on index 0- begening of rxBuffer;
*/
void refresh_rxBuffer(){
	*rxBuffer='\0';
	rxReadPos=0;
	rxWritePos=0;
}
/* int get_from_number(int read )

Function takes int read as argument, 
read==1 meaning message is UNREAD and phone number starts at position 24 (T-mobile HR default(?)) 
read==2 meaning message is READ and phone number starts at position 22 (T-mobile HR default (?))

It is assumed phone number has 13 digits. +385yy xxx xx xx

*dodati novu provjeru umjesto trenutne za to?nije rezultate!:
while(isdigit(rxBuffer[i])){
	from_number[j]=rxBuffer[i];
	j++;
	i++;
} 
I drugacije odrediti digit_pos (npr. tražiti drugi '+')*
	
Knowing on which position phone number starts, it extracts phone number from  rxBuffer (which contains the response to SMS request),
and saves it to global variable char from_phone_number.

*/
volatile char from_number[13];// volatile
int get_from_number(int read ){
	
	lcd_clrscr();
	
	int digit_pos=24; //UNREAD
	
	if(read==2) digit_pos=digit_pos-2; //READ
	
	int i=0, j=0;
	
	for(i=digit_pos;i<34;i++){
		from_number[j]=rxBuffer[i];
		j++;
	}
	return 1;
	
}
/*
int read_rxBuffer(void);

Function reads (shows in first row on lcd, and rewrites if more than 16 char) what is written in rxBuffer after GSM finishes responding 
to AT command (request message). 

There are 3 answers we are looking for:
1)	+CME: ERROR 321
2)	+CMGR: "REC UNREAD","0","<phone number>",,"yy/MM/dd,hh:mm:ss±zz"
<sms text>

OK

3)	+CMGR: "REC READ","0","<phone number>",,"yy/MM/dd,hh:mm:ss±zz"
<sms text>

OK


1) There is no message with this index.
2) There is UNREAD message, meaning phone number starts at position 24 of response.  
3) There is READ message, meaning phone number starts at position 22 of response (UN ads 2 more characters). 
(This case is rare but covered. It would occur if message was not deleted or responded to because of hardware brown out etc.)

4) other errors like ERROR 4 will cause code to repeat request (return 0).

Global variable int sms_flag is set, because int read is local and cannot be read from main.
*/
volatile int sms_flag=0;
int read_rxBuffer(void){

	lcd_clrscr();
	
	while(rxReadPos!=rxWritePos){
		
		if(rxReadPos%16==0 || rxReadPos==0) lcd_gotoxy(0,0);
		
		lcd_putc(rxBuffer[rxReadPos]);
		rxReadPos++;
		_delay_ms(175);
			
	}
	
	if(strstr(rxBuffer,"+CMS ERROR: 32")){
	
		lcd_puts("no message");
		return 1; // ERROR 321 -no message with this index, return 1 so code can perside to next message;
	
	} else if (strstr(rxBuffer,"OK")) {
		lcd_puts("message found"); //OK - continue;
	
		}else{
		
		lcd_puts("ERROR 4 or similar");
		return 0; //ERROR 4 or some other error occured- return 0 so code can repeat request for the same index message;
		} 
	//case OK: continue	
	int read=-1;
	
	if(strstr(rxBuffer,"READ")){ //found READ in message- do check if 'UN'(READ)		
		read=0;
		sms_flag=1;
	}
	
	if(strstr(rxBuffer,"UNREAD")){ // found UNREAD
		read=1; // UNREAD message found, number starts at position 24
	} else  read=2; //READ message found number starts at position 22
	
	_delay_ms(1000);
	lcd_clrscr();
	
	if(read==1) lcd_puts("THE UNREAD SMS");
	if(read==2) lcd_puts("THE READ SMS");
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
/* void LUX();

If sms_flag is set, LUX()function is entered. 
It looks for code word "LUX?" in received message. If code word is found getLight() function mesures light intensity 
and returns value between 1-255. Along with "dan" or "noc" (day(>200) or night(<200)) this value is send back via SMS.

*iskoristiti prednosti txBuffera, napuniti ga sa cijelom komandom, ispisati komandu na lcd i poslati!*
*/

void LUX(){
	//sms define
	char sms[]="AT+CMGS="; //sms command
	char *sms_text="Intenzitet: ";
	char rez[10];
	char *dannoc="d/n"; // warning udefined
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
			
			_delay_ms(1000);
			lcd_clrscr();

			lcd_gotoxy(0,0);
				
			//lcd_puts(sms);
			//USART_puts(sms);
			lcd_putc(34); // "
			//USART_putc(34); // "
				
			lcd_puts(from_number);
			//USART_puts(from_number);
			lcd_putc(34); // "
			//USART_putc(34);
			_delay_ms(1000);
			//lcd_gotoxy(0,1);
			lcd_putc(13);
			//USART_putc(13);
			_delay_ms(1000);

			//lcd_puts(sms_text);
			//USART_puts(sms_text);
			//lcd_puts(rez);
			//USART_puts(rez);
			lcd_puts(dannoc);
			//USART_puts(dannoc);
				
			//USART_putc(26);// CTRL+z
			//USART_putc(13); //ENTER
			_delay_ms(3000);
	
			lcd_clrscr();
			lcd_gotoxy(0,0);
			
			lcd_puts_P("poslano");
			_delay_ms(1000);
			
			lcd_clrscr();
			lcd_gotoxy(0,0);
							
		}else {
			lcd_clrscr();
			lcd_gotoxy(0,0);
			lcd_puts("Kriva kodna rijec");
			_delay_ms(1000);
			lcd_clrscr();
			lcd_gotoxy(0,0);
		/*poslati sms kriva kodna rijec? */
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
	lcd_clrscr();
	lcd_gotoxy(0,0);
	lcd_puts("text mode");
	_delay_ms(1000);
	lcd_clrscr();
	
	//refresh (empty) rxBuffer
	char index='1';
	//check 9 messages
	for(index='1';index!='9';index++){
			
		lcd_clrscr();
		lcd_gotoxy(0,0);	
		lcd_putc(index);
		_delay_ms(1000);
		lcd_clrscr();
		lcd_gotoxy(0,0);
		rq_flag=0;
		sms_flag=0;
		refresh_rxBuffer();
			
		request_sms(index);	//_ms_delay() within request_sms
		
		while(!read_rxBuffer()){ //until there is "OK" or "+CMS ERROR: 321" in rxBuffer, request for message	
			refresh_rxBuffer();
			rq_flag=0;
			sms_flag=0;
			*from_number='\0';
			request_sms(index);
		}
		
		if(sms_flag){
			lcd_clrscr();
			lcd_gotoxy(0,0);
			lcd_puts("dignut sms flag");
			_delay_ms(1000);
			lcd_clrscr();
			lcd_gotoxy(0,0);
			
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
	char *num ="+385996834050";
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
/// READ MESSAGE PROBE
/*	
	char request[]="at+cmgr=1";//zahtjev za poruku  s indexom 1
    USART_puts(request);
    USART_putc(13);
    _delay_ms(5000);
*/
}
