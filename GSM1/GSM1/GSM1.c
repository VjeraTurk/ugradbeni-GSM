#define  F_CPU           8000000UL

#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <string.h>

#include "lcd.h"

#define F_PWM           1000U //bh
#define N               8
#define TOP               (((F_CPU/F_PWM)/N)-1)
#define RED_DEF    TOP/3

#define BAUD            9600
#define BRC             ((F_CPU/16/BAUD)-1) //BAUD PRESCALAR (for Asynch. mode)
#define TX_BUFFER_SIZE  128
#define RX_BUFFER_SIZE  255 //255

#define RED		PD4
#define GREEN	PD3
#define BLUE	PD2

char txBuffer[TX_BUFFER_SIZE];
char rxBuffer[RX_BUFFER_SIZE];

volatile uint8_t txReadPos=0;
volatile uint8_t txWritePos=0;
volatile uint8_t rxReadPos=0;
volatile uint8_t rxWritePos=0;

volatile uint8_t tm_flag=0; //text mode flag: 1 - request sent 2 - OK answer received
volatile uint8_t rq_flag=0; //request flag: 1 - command sent 2 - OK answer received
volatile uint8_t del_flag=0; //delete flag: 1 - command sent 2 - OK answer received
//proba

volatile int sms_flag=0;
//initialize IO (LCD), UART, ADC, RED LED
void init(){
	//initialize IO
	lcd_init(LCD_DISP_ON);
	lcd_clrscr();
	//backlight pins
	DDRD |=_BV(RED) | _BV(GREEN) | _BV(BLUE); //RGB PB2 PB1 PB0
	PORTD|=_BV(RED) | _BV(GREEN) | _BV(BLUE);
	
	
	//initialize UART
	//set baud rate
	UBRRH = 0;
	UBRRL = BRC;
	//enable receiver and transmitter
	UCSRB = (1 << RXEN) | (1 << TXEN)| (1<<RXCIE)| (1<<TXCIE);
	//set frame format: 8 bit, character size 8 bit
	UCSRC = (1 << UCSZ0) | (1 << UCSZ1)|(1 << URSEL);
	
	//initialize ADC
	//adc enable, prescaler=64 -> clk=115200
	ADCSRA = _BV(ADEN)|_BV(ADPS2)|_BV(ADPS1);
	//AVcc reference voltage
	ADMUX = _BV(REFS0);

	//PA7 RED LED
	DDRA |= _BV(PA7); //LED
}
//USART serial communication functions and interrupts
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
ISR(USART_RXC_vect)
{
	rxBuffer[rxWritePos] = UDR;
	//lcd_putc (rxBuffer[rxWritePos]);
	
	if(rxBuffer[rxWritePos]=='K' && rxBuffer[rxWritePos-1]=='O')
	{
		if(tm_flag==1){
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
void USART_putc (char data)
{
	// Wait for empty transmit buffer
	while ( !( UCSRA & _BV(UDRE)) );

	// Put data into buffer, i.e., send the data
	//lcd_putc(data);
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
void refresh_rxBuffer(){
	*rxBuffer='\0';
	rxReadPos=0;
	rxWritePos=0;
}
void see_rxBuffer(){

	lcd_clrscr();
	rxReadPos=0;
	
	while(rxReadPos!=rxWritePos){
		//if(rxReadPos%16==0 || rxReadPos==0) lcd_gotoxy(0,0);
		if (rxReadPos==0) lcd_gotoxy(0,0);
		else if (rxReadPos==16 || rxReadPos==48) lcd_gotoxy(0,1);
		else if (rxReadPos==32 || rxReadPos==64) {
			lcd_clrscr();
			lcd_gotoxy(0,0);
		}
		lcd_putc(rxBuffer[rxReadPos]);
		rxReadPos++;
		_delay_ms(175);
	}
}
void send_sms(char number[], char sms_text[]){
	
	char AT_send_sms[]="AT+CMGS=";
	
	refresh_rxBuffer();
	
	USART_puts(AT_send_sms);
	USART_putc(34);
	USART_puts(number);
	USART_putc(34);
	_delay_ms(1000);
	USART_putc(13);
	_delay_ms(3000);
	see_rxBuffer();
	_delay_ms(3000);
	USART_puts(sms_text);
	USART_putc(26); // CTRL+z
	_delay_ms(3000);
	
	see_rxBuffer();
}
void _3_sms_test(){

	char p_sms[]="AT+CMGS=";
	char p_number[]="385976737211";
	char p_text[]="Kad ce";
	char p_text2[]="vise";
	char p_text3[]="praznici?";

	send_sms(p_number,p_text);
	send_sms(p_number,p_text2);
	send_sms(p_number,p_text3);
}


void date_time_check(){
	
	char cclk[]="at+cclk?";
	
	lcd_clrscr();
	lcd_puts_P("Datum i vrijeme:");
	refresh_rxBuffer();
	USART_puts(cclk);
	USART_putc(13); //ENTER
	_delay_ms(5000);
	lcd_clrscr();
	_delay_ms(1000);
	see_rxBuffer();
}

void RED_light(){
	PORTD |=_BV(RED); //RED ON
	
	PORTD &=~_BV(BLUE); //BLUE OFF
	PORTD &=~_BV(GREEN);	//GREEN OFF
}

void BLUE_light(){
	PORTD |=_BV(BLUE); //BLUE ON
	
	PORTD &=~_BV(RED);
	PORTD &=~_BV(GREEN);
}
void GREEN_light(){
	PORTD |=_BV(GREEN); //RED ON
	
	PORTD &=~_BV(RED);
	PORTD &=~_BV(BLUE);
}

int enable_text_mode();
void request_sms(char);
int read_rxBuffer();

int get_from_number(int);
volatile char from_number[13];

volatile uint8_t getLight(uint8_t);

void LUX();
volatile uint8_t upaljeno = 0;
volatile char from_number_lux[13];


void delete_sms(char);
void reboot();
//“ATD*100#” -> za poziv

//pink: blue+ pwm red TOP/2 (Barbie) ili TOP/1.1 (fuksija)
//purple: blue+ pwm red TOP/3.5
//yello: green+ pwm red TOP/1.5
//orange: green + pwm red TOP/1.2 ili 1.1


void rainbow(){
	
	cli();
	
	TCCR1A |= _BV(COM1B1)|_BV(WGM11)|_BV(WGM10);
	TCCR1B |= _BV(WGM13)|_BV(WGM12);
	OCR1A   = TOP;				// set TOP value 
	OCR1B   = RED_DEF;		// set default contrast value
	
	TIMSK  |= _BV(OCIE1A);

	TCCR1B |= _BV(CS11) | _BV(CS10);
	
	RED_light();		//RED
	_delay_ms(1000);
	
	GREEN_light();
	OCR1B   = TOP/1.1; 	// ORANGE
	_delay_ms(1000);
		
	OCR1B   = TOP/1.5; // YELLOW
	_delay_ms(1000);
	
	OCR1B   = TOP/3; //GREEN
	GREEN_light();
	
	_delay_ms(1000);
	
	BLUE_light();
	OCR1B   = TOP/10; //INDIGO
	_delay_ms(1000);
	
	OCR1B   = TOP/3; //VIOLET
	_delay_ms(1000);
	
	TCCR1A = 0;
	TCCR1B = 0;
	OCR1A  = 0;
	
	TCCR1B &= ~_BV(CS11);
	TCCR1B &= ~_BV(CS10);
	
	TIMSK  &=~_BV(OCIE1A);

	DDRD |=_BV(RED) | _BV(GREEN) | _BV(BLUE); //RGB PB2 PB1 PB0
	//PORTD|=_BV(RED) | _BV(GREEN) | _BV(BLUE);
	PORTD|=_BV(GREEN) | _BV(BLUE);
	
	sei();
}

int main(void)
{
	
	init();
	rainbow();


	//sei();
	
	while(!enable_text_mode());
	
	char index;
	
	//	_3_sms_test(); -radi
	//	date_time_check(); -radi
	//	reboot(); -radi
	//	date_time_check(); -radi
	
	while (1) {
		
		for(index='1';index!='9';index++){

			rq_flag=0;
			sms_flag=0;
			refresh_rxBuffer();
			request_sms(index);    //_ms_delay() within request_sms
			
			//until there is "OK" or "+CMS ERROR: 321" in rxBuffer, request for message
			while(!read_rxBuffer())
			{
				//see_rxBuffer();
				refresh_rxBuffer();
				rq_flag=0;
				sms_flag=0;
				*from_number='\0';
				request_sms(index);
			}
			
			if(sms_flag){
				LUX();
				//delete_sms(index);
				refresh_rxBuffer();
				sms_flag=0;
				*from_number='\0';
			}
			
		}
	}
}

int enable_text_mode(void){

	char text_mode[] = "AT+CMGF=1";
	//enter TEXT MODE
	
	RED_light();
	
	refresh_rxBuffer();
	lcd_clrscr();
	lcd_gotoxy(0,0);
	lcd_puts_P("Ulazim u text");
	lcd_gotoxy(0,1);
	lcd_puts_P("mode");
	USART_puts(text_mode);
	tm_flag=1;
	USART_putc(13); //ENTER
	
	_delay_ms(2000);
	lcd_gotoxy(4, 1);
	lcd_putc('.');
	_delay_ms(1000);
	lcd_gotoxy(5, 1);
	lcd_putc('.');
	_delay_ms(1000);
	lcd_gotoxy(6, 1);
	lcd_putc('.');
	_delay_ms(1000);
	
	if (tm_flag==2){
		tm_flag=0; //novo
		lcd_clrscr();
		
		BLUE_light();
		return 1;
		
		} else {
		lcd_clrscr();
		_delay_ms(500);
		lcd_puts_P("rxBuffer:");
		_delay_ms(500);
		see_rxBuffer();
		return 0;
	}
}



int read_rxBuffer(void){

	lcd_clrscr();
	
	while(rxReadPos!=rxWritePos){
		//if(rxReadPos%16==0 || rxReadPos==0) lcd_gotoxy(0,0);
		if (rxReadPos==0) lcd_gotoxy(0,0);
		else if (rxReadPos==16 || rxReadPos==48) lcd_gotoxy(0,1);
		else if (rxReadPos==32 || rxReadPos==64) {
			lcd_clrscr();
			lcd_gotoxy(0,0);
		}
		if (rq_flag == 2) lcd_putc(rxBuffer[rxReadPos]);
		rxReadPos++;
		_delay_ms(175);
	}
	

	if(strstr(rxBuffer,"+CMS ERROR: 321"))
	{
		lcd_clrscr();
		lcd_puts("nema poruke");
		_delay_ms(1000);
		see_rxBuffer();
		refresh_rxBuffer();
		return 1; // ERROR 321 -no message with this index, return 1 so code can perside to next message;
	}
	else if(strstr(rxBuffer,"+SIND"))
	{
		
		lcd_clrscr();
		lcd_puts_P("+SIND");
		_delay_ms(5000);
		//while(!enable_text_mode());
		return 0;
	}
	else if(strstr(rxBuffer,"+CMS ERROR:"))
	{
		lcd_clrscr();
		lcd_gotoxy(0,0);
		lcd_puts_P("CMS ERROR");
		_delay_ms(1000);
		lcd_gotoxy(0,0);
		
		/////novo:
		lcd_clrscr();
		//lcd_puts(rxBuffer);
		_delay_ms(3000);
		see_rxBuffer();
		refresh_rxBuffer();
		//reboot();
		//while(!enable_text_mode());
		
		/////end novo
		
		return 0; //ERROR 4 or some other error occured- return 0 so code can repeat request for the same index message;
		
	}else if(strstr(rxBuffer,"+CME ERROR:"))
	{
		lcd_clrscr();
		lcd_gotoxy(0,0);
		lcd_puts_P("CME ERROR");
		_delay_ms(1000);
		lcd_gotoxy(0,0);
		
		/////novo:
		lcd_clrscr();
		//lcd_puts(rxBuffer);
		_delay_ms(3000);
		see_rxBuffer();
		refresh_rxBuffer();
		/////end novo
		
		return 0; //ERROR 4 or some other error occured- return 0 so code can repeat request for the same index message
		

	}else if (strstr(rxBuffer,"OK"))
	{
		
		//case OK: continue
		int read=0;

		if(strstr(rxBuffer,"UNREAD")){ // found UNREAD
			read=1; // UNREAD message found, number starts at position 24
			sms_flag=1;
			get_from_number(read);
			return 1;
		}
		
		else if(strstr(rxBuffer,"READ")){ //found READ in message
			read=2;//READ message found number starts at position 22
			sms_flag=1;
			get_from_number(read);
			return 1;
			} else {
			
			while(!enable_text_mode());
			return 0; //vraca 0 i za error 4 i za OK koji se ne odnosi na procitanu ili ne procitanu poruku nego na nešto drugo
			
		}
		
	}
	
	return 0;
}

void reboot(){
	
	char reboot[]="at+cfun=1,1";
	
	refresh_rxBuffer();
	lcd_clrscr();
	lcd_puts_P("rebooting...");
	USART_puts(reboot);
	USART_putc(13); //ENTER
	_delay_ms(5000);
	lcd_clrscr();
	see_rxBuffer();
	_delay_ms(1000);
	
}
void echo(){

	char echo[]="ate1";
	
	USART_puts(echo);
	USART_putc(13); //ENTER
	refresh_rxBuffer();
	lcd_clrscr();
	lcd_puts_P("echo...");
	_delay_ms(5000);
	lcd_clrscr();
	see_rxBuffer();
	_delay_ms(1000);
}

void request_sms(char index){
	
	refresh_rxBuffer();
	char request[]="AT+CMGR=";
	lcd_clrscr();
	BLUE_light();
	lcd_gotoxy(0,0);
	lcd_puts_P("Dohvacam poruku");
	lcd_gotoxy(0,1);
	lcd_puts_P ("indexa ");
	lcd_putc(index);
	
	USART_puts(request);
	USART_putc(index);
	rq_flag=1;
	USART_putc(13); //ENTER
	
	//_delay_ms(3000);
	lcd_gotoxy(8, 1);
	lcd_putc('.');
	_delay_ms(1000);
	lcd_gotoxy(9, 1);
	lcd_putc('.');
	_delay_ms(1000);
	lcd_gotoxy(10, 1);
	lcd_putc('.');
	_delay_ms(1000);
	
}

void delete_sms(char index){
	
	char delete[]="AT+CMGD=";
	USART_puts(delete);
	USART_putc(index);
	
	del_flag=1;
	USART_putc(13);
	_delay_ms(3000);
	if(del_flag==2) {
		del_flag=0;
		lcd_gotoxy(0,0);
		lcd_puts("izbrisana poruka");
		lcd_gotoxy(0,1);
		lcd_puts("br ");
		lcd_puts(index);
		

		_delay_ms(1000);
	}
}
//ISPRAVITI !
int get_from_number(int read){
	
	lcd_clrscr();
	
	int digit_pos=24; //UNREAD
	int pos=36;
	
	if(read==2) {
		digit_pos=digit_pos-2; //READ
		pos=34;
	}
	
	int i=0, j=0;
	
	for(i=digit_pos;i<pos;i++){
		from_number[j]=rxBuffer[i];
		j++;
	}
	return 1;
	
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
	char *dannoc="d/n"; // warning undefined
	uint8_t adch=0;
	uint8_t i;
	static uint8_t after_lux=0;
	
	if(strstr(rxBuffer, "LUX?")){
		
		lcd_gotoxy(0,1);
		adch=getLight(1);
		
		if (adch>200){
			dannoc=" dan";
			
			if (upaljeno) {
				dannoc = " dan. Ugasiti svjetlo(DA)? ";
				
				for (i=0; i<13; i++) {
					from_number_lux[i] = from_number[i];
				}
				after_lux = 1; //priprema za gasenje ledice ako sljedeca poruka bude "DA"
			}
			
			}else{
			dannoc=" noc";
			
			if (!upaljeno) {
				dannoc=" noc. Upaliti svjetlo(DA)?";
				for (i=0; i<13; i++) {
					from_number_lux[i] = from_number[i];
				}
				after_lux = 1; //priprema za paljenje ledice ako sljedeca poruka bude "DA"
			}
		}
		//novo:
		refresh_rxBuffer();
		//end novo
		itoa(adch,rez,10);
		
		USART_puts(sms);
		USART_putc(34);
		
		USART_puts(from_number);
		USART_putc(34);
		_delay_ms(1000);
		USART_putc(13);
		_delay_ms(1000);

		USART_puts(sms_text);
		USART_puts(rez);
		USART_puts(dannoc);
		USART_putc(26); // CTRL+z
		//USART_putc(13); //ENTER-->>>NEEE!!!
		
		_delay_ms(3000);
		
		
		GREEN_light();
		
		lcd_clrscr();
		lcd_gotoxy(0,0);
		lcd_puts_P("Poslan odgovor");
		lcd_gotoxy(0,1);
		lcd_puts_P("na poruku.");
		_delay_ms(2000);
		
		BLUE_light();
		
		
		///novo
		see_rxBuffer();
		///end novo

		lcd_clrscr();
		lcd_gotoxy(0,0);/**/
		
		
		} else if (strstr(rxBuffer, "DA")) {
		if ((after_lux) && (!strcmp(from_number, from_number_lux))) {
			
			if (!upaljeno)
			{
				PORTA |= _BV(PA7);
				upaljeno = 1;
			}
			
			else
			{
				PORTA &=~_BV(PA7);
				upaljeno = 0;
			}
			
			after_lux = 0;
		}
		
		}else{
		//krive kodna rijec:
		
		refresh_rxBuffer();
		
		USART_puts(sms);
		USART_putc(34);
		USART_puts(from_number);
		USART_putc(34);
		_delay_ms(1000);
		USART_putc(13);
		_delay_ms(1000);

		USART_puts("Kriva kodna rijec.");
		USART_putc(26);// CTRL+z
		//USART_putc(13); //ENTER->>>>NEEE!!!
		_delay_ms(3000);
		
		GREEN_light();
		
		lcd_clrscr();
		lcd_gotoxy(0,0);
		lcd_puts_P("Poslan odgovor");
		lcd_gotoxy(0,1);
		lcd_puts_P("na krivu poruku.");
		_delay_ms(2000);
		
		BLUE_light();
		
		see_rxBuffer();
		
	}

}