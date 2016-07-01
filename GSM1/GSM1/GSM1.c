#define  F_CPU           8000000UL

#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <string.h>

#include "lcd.h"

#define F_PWM           1000Ubh
#define N               8
#define TOP               (((F_CPU/F_PWM)/N)-1)
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

volatile uint8_t tm_flag=0; //text mode flag: 1 - request sent 2 - OK answer received
volatile uint8_t rq_flag=0; //request flag: 1 - command sent 2 - OK answer received
volatile uint8_t del_flag=0; //delete flag: 1 - command sent 2 - OK answer received

volatile char from_number[13];
volatile char from_number_lux[13];
volatile int sms_flag=0;

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

void refresh_rxBuffer(){
    *rxBuffer='\0';
    rxReadPos=0;
    rxWritePos=0;
}

volatile uint8_t upaljeno = 0;
void USART_putc(char);
void USART_puts(char*);
int read_rxBuffer();
int enable_text_mode();
void request_sms(char);
void delete_sms(char);
int get_from_number(int);
volatile uint8_t getLight(uint8_t);
void LUX();

//“ATD*100#” -> za poziv
int main(void)
{
    //initialize IO
    lcd_init(LCD_DISP_ON);
    lcd_clrscr();
    
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
    
    sei();    
    _delay_ms(3000);
    
    DDRB |=_BV(PB2) | _BV(PB1) | _BV(PB0); //RGB
    DDRA |= _BV(PA7); //LED
    
    PORTB |=_BV(PB2); //RED ON
    
    while(!enable_text_mode());
    
    lcd_clrscr();
    lcd_gotoxy(0,0);

    PORTB &=~_BV(PB2); //RED OFF    
    PORTB |=_BV(PB0); //BLUE ON

    char index;
    
    
    while (1) {
        
        //check 2 messages
        for(index='1';index!='2';index++){
            lcd_clrscr();
            lcd_gotoxy(0,0);
            lcd_puts_P("Dohvacam poruku"); 
            lcd_gotoxy(0,1);
            lcd_puts_P ("indexa ");    
            lcd_putc(index);
            lcd_putc('.');
            _delay_ms(1000);
            lcd_gotoxy(8, 1);
            lcd_putc('.');
            _delay_ms(1000);
            lcd_gotoxy(9, 1);
            lcd_putc('.');
            _delay_ms(1000);
            lcd_gotoxy(10, 1);
            lcd_putc('.');
            
            rq_flag=0;
            sms_flag=0;
            refresh_rxBuffer();
            
            request_sms(index);    //_ms_delay() within request_sms
        
            //until there is "OK" or "+CMS ERROR: 321" in rxBuffer, request for message    
            while(!read_rxBuffer())
            { 
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

//USART serial communication functions and interrupts
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
    
    if(strstr(rxBuffer,"+CMS ERROR: 32"))
    {
        lcd_puts("no message");
        return 1; // ERROR 321 -no message with this index, return 1 so code can perside to next message;
    }
    
    else if (strstr(rxBuffer,"OK")) 
    {
    //    lcd_puts("message found"); //OK - continue;
    }
    
    else
    {
        lcd_puts("ERROR 4 or similar");
        return 0; //ERROR 4 or some other error occured- return 0 so code can repeat request for the same index message;
    }
     
    //case OK: continue    
    int read=0;
    
    /*
    if(strstr(rxBuffer,"READ")){ //found READ in message- do check if 'UN'(READ)        
        read=2;//READ message found number starts at position 22
        sms_flag=1;
    }
    
    if(strstr(rxBuffer,"UNREAD")){ // found UNREAD
        read=1; // UNREAD message found, number starts at position 24
    } else {
        
        return 0; 
    }
    */
    
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
    }
    
    else {
        return 0; //vraca 0 i za error 4 i za OK koji se ne odnosi na procitanu ili ne procitanu poruku nego na nešto drugo
    }
    
    
    /*_delay_ms(1000);
    lcd_clrscr();
    
    if(read==1) lcd_puts("UNREAD SMS");
    if(read==2) lcd_puts("READ SMS");
    _delay_ms(2000);
    lcd_clrscr();*/

}


int enable_text_mode(void){

    char text_mode[] = "AT+CMGF=1";
    
        
    //enter TEXT MODE
    lcd_gotoxy(0,0);
    lcd_puts_P("Ulazim u text");
    lcd_gotoxy(0,1);
    lcd_puts_P("mode");
    _delay_ms(1000);
    lcd_gotoxy(4, 1);
    lcd_putc('.');
    _delay_ms(1000);
    lcd_gotoxy(5, 1);
    lcd_putc('.');
    _delay_ms(1000);
    lcd_gotoxy(6, 1);
    lcd_putc('.');
    
    //_delay_ms(3000);
    USART_puts(text_mode);
    tm_flag=1;
    USART_putc(13); //ENTER
    _delay_ms(3000);
    lcd_clrscr();
    if (tm_flag==2) return 1;
    else return 0;
}

void request_sms(char index){
    
    char request[]="AT+CMGR=";
    USART_puts(request);
    USART_putc(index);
    lcd_clrscr();
    rq_flag=1;
    USART_putc(13); //ENTER
    _delay_ms(3000);
    
}

void delete_sms(char index){
    
    char delete[]="AT+CMGD=";
    USART_puts(delete);
    USART_putc(index);
    lcd_gotoxy(0,0);
    lcd_puts("izbrisana poruka");
    lcd_gotoxy(0,1);
    lcd_puts("br ");
    lcd_puts(index);
    
    del_flag=1;
    USART_putc(13);
    _delay_ms(3000);
    if(del_flag==2) { 
        del_flag=0;
        lcd_puts("deleted");
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
            USART_putc(13); //ENTER
            _delay_ms(3000);
            
            
            PORTB |=_BV(PB1); //GREEN ON
            PORTB &=~_BV(PB0); //BLUE OFF

            lcd_clrscr();
            lcd_gotoxy(0,0);
            lcd_puts_P("Poslan odgovor");
            lcd_gotoxy(0,1);
            lcd_puts_P("na poruku.");
            _delay_ms(2000);
            
            PORTB &=~_BV(PB1); //GREEN OFF
            PORTB |=_BV(PB0); //BLUE ON
            
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
                
        }else{/*
            USART_puts(sms);
            USART_putc(34);
            USART_puts(from_number);
            USART_putc(34);
            _delay_ms(1000);
            USART_putc(13);
            _delay_ms(1000);

            USART_puts("Kriva kodna rijec.");
            USART_putc(26);// CTRL+z
            USART_putc(13); //ENTER
            _delay_ms(3000);*/
            
            PORTB |=_BV(PB1); //GREEN ON
            PORTB &=~_BV(PB0); //BLUE OFF
            
            lcd_clrscr();
            lcd_gotoxy(0,0);
            lcd_puts_P("Poslan odgovor");
            lcd_gotoxy(0,1);
            lcd_puts_P("na poruku.");
            _delay_ms(2000);
            
            PORTB &=~_BV(PB1); //GREEN OFF
            PORTB |=_BV(PB0); //BLUE ON
            
        }

}
