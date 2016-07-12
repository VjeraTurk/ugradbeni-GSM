/**
 * @file GSM1.c
 * @author Lovro Spetic, Vjera Turk, Patricija Zubalic
 * @date 12 Jul 2016
 * @brief This is the main file where AT commands are sent to gsm module requesting received SMS messages saved on SIM card within GSM module.
 * SMS messages are being analyzed and replayed to with light intensity and light is switched off and on depending on SMS content.
 */

#define  F_CPU           8000000UL

#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <string.h>

#include "lcd.h"

#define F_PWM           1000U 
#define N               8
#define TOP               (((F_CPU/F_PWM)/N)-1)
#define RED_DEF    TOP/3

#define BAUD            9600
#define BRC             ((F_CPU/16/BAUD)-1) //BAUD PRESCALAR (for Asynch. mode)
#define TX_BUFFER_SIZE  128
#define RX_BUFFER_SIZE  255 

#define RED		PD4
#define GREEN	PD3
#define BLUE	PD2

#define LED	PA2
#define SLEEP	PA3

/**3-state flag: 0 - command not sent,  1 - command sent, 2 - OK answer received from GSM module*/
volatile uint8_t tm_flag=0;
/**3-state flag: 0 - command not sent,  1 - command sent, 2 - OK answer received from GSM module*/
volatile uint8_t rq_flag=0;
/**3-state flag: 0 - command not sent,  1 - command sent, 2 - OK answer received from GSM module*/
volatile uint8_t del_flag=0;
/**2-state flag: 0 - sms not (jet) found,  1 - a valid READ or UNREAD sms found */
volatile int sms_flag=0;


/////// U A R T communication ////////////
char txBuffer[TX_BUFFER_SIZE];
/**

*/
char rxBuffer[RX_BUFFER_SIZE];

volatile uint8_t txReadPos=0;
volatile uint8_t txWritePos=0;
volatile uint8_t rxReadPos=0;
volatile uint8_t rxWritePos=0;
/**
 * @brief USART Interrupt service routine which fills rxBuffer the moment new data is received
 * 
 * Depending on which flag is previously set to '1', it knows what the received content OK represents 
 * (an answer to which AT command: request, delete, text mode)
 * It then sets flag to '2', enabling actions that follow after the return from the interrupt service routine. 
 */
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
//////////////////////////////////////////



/**
 * @brief A function for initializing LCD, ADC and IO pins
 * 
 */
void init();


/**
 * @brief This function sends at command for entering text mode. It is called in while loop
 *	while(!enable_text_mode()); until OK answer is received from GSM, meaning it has entered the text mode.
 *
 * @param number[] phone number to send sms to
 * @param sms_text[] text to send
 * @return 0 or 1 depending on gsm answer (if answer is OK return is 1 else 0)
 */
int enable_text_mode();


/**
 * @brief This function sends at command for requesting sms of certain index.
 * For indexes higher than 9, it also sends out index's tenner. 
 * (The highest index it can request is 99, but since SIM car memory can handle around 25 messages it is not required)
 *
 * @param index 
 * @param tenner
 */
void request_sms(char index, char tenner);


/**
 * @brief Function analyzes  what is written in rxBuffer after GSM finishes responding to AT command (requesting SMS message).
 * There are 3 answers it is looking for:
 * 1)	+CME: ERROR 321
 * 2)	+CMGR: "REC UNREAD","0","<phone number>",,"yy/MM/dd,hh:mm:ss+zz"
 * <sms text>
 *
 * OK
 *
 * 3)	+CMGR: "REC READ","0","<phone number>",,"yy/MM/dd,hh:mm:ss+zz"
 * <sms text>
 *
 * OK
 * 1) There is no message with this index.
 * 2) There is UNREAD message, meaning phone number starts at position 24 of response.
 * 3) There is READ message, meaning phone number starts at position 22 of response (UN ads 2 more characters before phone number).
 * (This case is rare but covered. It would occur if message was not deleted or responded to the first time it is read)
 *
 * 4) If other errors (CME and CMS) and booting lines (which start with +SIND) repeat are received request is repeated (return 0) after awhile.
 * Global variable int sms_flag is set if UNREAD or READ message is found.
 *
 * @return 1 if there is no message on requested index or if there is
 * @return 0 if the error has occurred and request has to be repeated
 */
int read_rxBuffer();


/**
 * @brief Function extracts phone number from rxBuffer, after it is confirmed it containes a valid READ or UNREAD message in read_rxBuffer() function
 * @param read
 * read==1 meaning message is UNREAD and phone number starts at position 24 
 * read==2 meaning message is READ and phone number starts at position 22
 * It is assumed phone number has 12 digits. 385yy xxx xx xx
 * Knowing on which position phone number starts, it extracts phone number from rxBuffer
 * and saves it to global variable char from_phone_number.

*/
int get_from_number(int);
	volatile char from_number[13];

/** 
 * @brief If sms_flag is set to 2, meaning there is an SMS in rxBuffer, LUX()function is entered from main.
 * It seeks code word "LUX?" in received SMS. 
 * If code word "LUX?" is found getLight() function mesures light intensity
 * and returns value between 1-255. Along with "dan" or "noc" (day(>200) or night(<200)) this value is send back via SMS.
 *
 * If the light is off during night, it also sends a question if you would like to turn the light on.
 * If the light is on during daylight, it sends a question if you would like to turn the light off.
 * If the next message it receives is "DA" (yes) it switches the light off or on.
 */
void LUX();
volatile uint8_t getLight(uint8_t);
	volatile uint8_t upaljeno = 0;
	volatile char from_number_lux[13];
/**
 * @brief This function sends at command for deleting sms of certain index. 
 * For indexes higher than 9, it also sends out index's tenner. 
 * (The highest index it can request is 99, but since SIM car memory can handle around 25 messages it is not required)
 * It than waits for GSM module to reply with OK, meaning sms was successfuly deleted.
 *
 * @param index 
 * @param tenner
 */
void delete_sms(char index, char tenner);

/**
 * @brief This function puts rxBuffer content to 16x2 LCD screen
 *
 */
void see_rxBuffer();


/**
 * @brief This function sets rxReadPos and rxWritePos to 0 and "empties" rxBuffer
 *
 * It is used after end of analyzing rxBuffer content to empty the buffer which contains received data. Every new received answer is the only answer in rxBuffer.
 * Buffer always contains only last received answer form GSM. All tough rxBuffer is circular, 
 * in normal situations, it's full capacity and overflow are never reached because of use of this function. 
 *
 */
void refresh_rxBuffer();



void RED_light();
void GREEN_light();
void BLUE_light();

void rainbow();




/**
 * @brief This function is for sending sms containing given text to given number
 *
 * @param number[] phone number to send sms to
 * @param sms_text[] text to send
 */
void send_sms(char number[], char sms_text[]);
void _4_sms_test();

void echo();
void date_time_check();
void reboot();

void sleep_mode();


/**
 * @brief Main
 *
 * Function is called once the program is started
 */
int main(void)
{
	
	init();
	rainbow();

	sei();

	while(!enable_text_mode());
	

	char index;
	char tenner;

	//ovdje si:
	
	///PORTA&=~_BV(SLEEP); //disable sleep mode?
	///PORTA|=_BV(SLEEP);	//enable sleep mode?	
	///sleep_mode();
	
	while (1) {
		for(tenner='0';tenner!=':';tenner++){
			for(index='1';index!=':';index++){

				rq_flag=0;
				sms_flag=0;
				refresh_rxBuffer();
				request_sms(index, tenner);    //_ms_delay() within request_sms
			
				//until there is "OK" or "+CMS ERROR: 321" in rxBuffer, request for message
				while(!read_rxBuffer())
				{
					//see_rxBuffer();
					refresh_rxBuffer();
					rq_flag=0;
					sms_flag=0;
					*from_number='\0';
					request_sms(index, tenner);
				}
			
				if(sms_flag){
					LUX();
					delete_sms(index, tenner);
					refresh_rxBuffer();
					sms_flag=0;
					*from_number='\0';
				}
			
			}
		}
	}
}


///////////////////////////////////////////////////
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

	//PA2 RED LED
	DDRA |= _BV(LED); //LED
}

int enable_text_mode(void){

	char text_mode[] = "AT+CMGF=1";
	
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
		tm_flag=0; 
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

void request_sms(char index, char tenner){
	
	refresh_rxBuffer();
	
	char request[]="AT+CMGR=";
	lcd_clrscr();
	BLUE_light();
	lcd_gotoxy(0,0);
	lcd_puts_P("Dohvacam poruku");
	lcd_gotoxy(0,1);
	
	lcd_puts_P ("indexa ");

	if(tenner!='0'){
		lcd_putc(tenner);
	}

	lcd_putc(index);
	
	USART_puts(request);
	if(tenner!='0'){
		USART_putc(tenner);
	}

	USART_putc(index);

	rq_flag=1;
	USART_putc(13); //ENTER
	
	lcd_gotoxy(9, 1);
	lcd_putc('.');
	_delay_ms(1000);
	lcd_gotoxy(10, 1);
	lcd_putc('.');
	_delay_ms(1000);
	lcd_gotoxy(11, 1);
	lcd_putc('.');
	_delay_ms(1000);
	
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
		return 1; // ERROR 321 -no message with this index, return 1 so code can proceed to next sms
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
		
		
		lcd_clrscr();
		_delay_ms(3000);
		see_rxBuffer();
		refresh_rxBuffer();
		//reboot();
		//while(!enable_text_mode());
		
		return 0; //CMS ERROR has occurred- return 0 so code can repeat request for the same index message
		
	}else if(strstr(rxBuffer,"+CME ERROR:"))
	{
		lcd_clrscr();
		lcd_gotoxy(0,0);
		lcd_puts_P("CME ERROR");
		_delay_ms(1000);
		lcd_gotoxy(0,0);
		

		lcd_clrscr();
		//lcd_puts(rxBuffer);
		_delay_ms(3000);
		see_rxBuffer();
		refresh_rxBuffer();
	
		
		return 0; //CME ERROR has occurred- return 0 so code can repeat request for the same index message
		

	}else if (strstr(rxBuffer,"OK"))
	{
		int read=0;

		if(strstr(rxBuffer,"UNREAD")){ // found UNREAD
			read=1; // UNREAD message found, number starts at position 24
			sms_flag=1;
			get_from_number(read);
			return 1;
		}
		
		else if(strstr(rxBuffer,"READ")){ //found READ 
			read=2;//READ message found number starts at position 22
			sms_flag=1;
			get_from_number(read);
			return 1;
			} else {
			

		// if GSM gets out of text mode it will reply to sms request with string of numbers (PDU mode) and OK at the end,
		// code will end up here, where text mode will be reentered:
			
			while(!enable_text_mode());
			return 0; 
		}
		
	}
	
	return 0;
}

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

void LUX(){

	char sms[]="AT+CMGS="; //sms command
	char *sms_text="intenzitet iznosi: ";
	char rez[10];
	char *dannoc="d/n";
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
				after_lux = 1; //indicates the light is supposed to be turned off if next sms from this number is "DA"
			}
			
			}else{
			dannoc=" noc";
			
			if (!upaljeno) {
				dannoc=" noc. Upaliti svjetlo(DA)?";
				for (i=0; i<13; i++) {
					from_number_lux[i] = from_number[i];
				}
				after_lux = 1; //indicates the light is supposed to be turned on if next sms from this number is "DA"
			}
		}
		
		refresh_rxBuffer();
		
		///in progeress:
		/// tog i tog datuma, u toliko sati intenzitet je ___
		///char present[]=present=date_time_check();
		
		itoa(adch,rez,10);
		
		
		USART_puts(sms);
		USART_putc(34);
		
		USART_puts(from_number);
		USART_putc(34);
		_delay_ms(1000);
		USART_putc(13);
		_delay_ms(1000);

		///USART_puts(present);
		
		USART_puts(sms_text);
		USART_puts(rez);
		USART_puts(dannoc);
		USART_putc(26); // CTRL+z
		
		_delay_ms(3000);
		
		
		GREEN_light();
		
		lcd_clrscr();
		lcd_gotoxy(0,0);
		lcd_puts_P("Poslan odgovor");
		lcd_gotoxy(0,1);
		lcd_puts_P("na poruku.");
		_delay_ms(2000);
		
		see_rxBuffer();
		BLUE_light();
		
		lcd_clrscr();
		lcd_gotoxy(0,0);
		

		} else if (strstr(rxBuffer, "DA")) {
		if ((after_lux) && (!strcmp(from_number, from_number_lux))) {
			
			if (!upaljeno)
			{
				PORTA |= _BV(LED);
				upaljeno = 1;
			}
			
			else
			{
				PORTA &=~_BV(LED);
				upaljeno = 0;
			}
			
			after_lux = 0;
		}
		
		}else if(strstr(rxBuffer, "DUGA")){

		rainbow();
		
		
		}else{
		//wrong key word.
		
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
		
		_delay_ms(3000);
		
		GREEN_light();
		
		lcd_clrscr();
		lcd_gotoxy(0,0);
		lcd_puts_P("Poslan odgovor");
		lcd_gotoxy(0,1);
		lcd_puts_P("na krivu poruku.");
		_delay_ms(2000);
		
		
		see_rxBuffer();
		BLUE_light();
		
	}

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

void delete_sms(char index, char tenner){
	
	refresh_rxBuffer();
	char delete[]="AT+CMGD=";
	USART_puts(delete);
	
	
	if(tenner!='0'){
		USART_putc(tenner);
	}
	
	USART_putc(index);
	
	
	del_flag=1;
	USART_putc(13);
	_delay_ms(4000);
	if(del_flag==2) {
		del_flag=0;
		lcd_clrscr();
		lcd_puts_P("izbrisana poruka");
		lcd_gotoxy(0,1);
		lcd_puts("br ");

		if(tenner!='0'){
			lcd_putc(tenner);
		}
		lcd_putc(index);
		_delay_ms(2000);
		see_rxBuffer();
		_delay_ms(1000);
	}
}

///////////////////////////////////////////

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

void refresh_rxBuffer(){
	*rxBuffer='\0';
	rxReadPos=0;
	rxWritePos=0;
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

void rainbow(){


	//pink: blue+ pwm red TOP/2 (Barbie) ili TOP/1.1 (fuksija)
	//purple: blue+ pwm red TOP/3.5
	//yello: green+ pwm red TOP/1.5
	//orange: green + pwm red TOP/1.2 ili 1.1

	
	
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
	PORTD|=_BV(RED) | _BV(GREEN) | _BV(BLUE);
	//PORTD|=_BV(GREEN) | _BV(BLUE);
	
	sei();
}

////////////////////////////////////////////////

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
void _4_sms_test(){

	char p_sms[]="AT+CMGS=";
	//char p_number[]="385976737211";
	char p_number[]="385996834050";

	char p_text[]="LUX?";
	char p_text2[]="LUKS?";
	char p_text3[]="DUGA?";
	char p_text4[]="DA";


	send_sms(p_number,p_text);
	send_sms(p_number,p_text2);
	send_sms(p_number,p_text3);
	send_sms(p_number,p_text4);
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

//ovdje si:
void sleep_mode(){
	
	char sleep[]="AT+S32K";
	
	refresh_rxBuffer();
	lcd_clrscr();
	lcd_puts_P("sleep mode...");
	USART_puts(sleep);
	USART_putc(13); //ENTER
	_delay_ms(5000);
	lcd_clrscr();
	see_rxBuffer();
	_delay_ms(1000);
	
}


/* code in progress:

void money_check(){
	
	//ATD*100# 
	//http://stackoverflow.com/questions/22238243/gsm-atd-command-to-check-my-balance
	//AT+CUSD=[<n>[,<str>[,<dcs>]]]
	char money[]="AT+CUSD=[<n>[,<str>[,<dcs>]]]";

	refresh_rxBuffer();

	USART_puts(money);
	USART_putc(34); //Enter
	//USART_putc(26); // CTRL+z
	_delay_ms(3000);

	see_rxBuffer();
}
void pay(){
	
}
*/
/*
dodati novu provjeru umjesto trenutne za tocnije rezultate!:
while(isdigit(rxBuffer[i])){
	from_number[j]=rxBuffer[i];
	j++;
	i++;
}
I drugacije odrediti digit_pos (npr. tražiti ")
*/