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
#define RX_BUFFER_SIZE  128

char txBuffer[TX_BUFFER_SIZE];
char rxBuffer[RX_BUFFER_SIZE];

volatile uint8_t txReadPos=0;
volatile uint8_t txWritePos=0;
volatile uint8_t rxReadPos=0;
volatile uint8_t rxWritePos=0;

void UART_Init(void)
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

ISR(USART_TXC_vect)
{

	if(txReadPos != txWritePos)
	{
		UDR = txBuffer[txReadPos];
		txReadPos++;

		if (txReadPos>=TX_BUFFER_SIZE)
		{
			txReadPos=0;
		}
	}

}

volatile char from_number[13];
volatile uint8_t number_position=4;
volatile uint8_t read_over=0;
volatile uint8_t last_ok=0;
volatile uint8_t number_ok=0;

ISR(USART_RXC_vect)
{
	rxBuffer[rxWritePos] = UDR;

	if(rxBuffer[rxWritePos]=='5')
    {
		if(rxBuffer[rxWritePos-1]=='8')
		{
            number_ok=1;
            from_number[0]='+';
            from_number[1]='3';
            from_number[2]='8';
            number_position=3;
		}
	}

	if(number_ok==1 && number_position<13 )
    {
		from_number[number_position]=rxBuffer[rxWritePos];
		number_position++;
	}

	//kada prvi put dobijemo ok
	if(rxBuffer[rxWritePos]=='K' && rxBuffer[rxWritePos-1]=='O')
    {
		lcd_puts("1.OK");
		last_ok=1;
	}

	//uvijek dvaput ok? za sta je prvi a za sta drugi?
	if( last_ok==1 && rxBuffer[rxWritePos]=='K' && rxBuffer[rxWritePos-1]=='O')
	{
		lcd_puts("2.OK");
		read_over=1;
		last_ok=0;
	}

    //sve ispisujemo ?
	lcd_putc (rxBuffer[rxWritePos]);
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
	UDR = data;
}


static void IO_Init(void)
{
	// initialize display, cursor off
	lcd_init(LCD_DISP_ON);

	// clear display and home cursor
	lcd_clrscr();

    // put string to display (line 1) with linefeed
    // lcd_puts_P("Hello world\n");
}

void USART_puts( char str[])
{
	int i;

	for(i=0; i<strlen(str); i++)
    {
		USART_putc(str[i]);
		_delay_ms(1000);
	}
}

volatile char buffer[4]; // nije mi bas jasno zasto 4?
void ADC_Init()
{
	//adc enable, prescaler=64 -> clk=115200
	ADCSRA = _BV(ADEN)|_BV(ADPS2)|_BV(ADPS1);

	//AVcc reference voltage
	ADMUX = _BV(REFS0);
}

volatile char getLight(uint8_t channel)
{
	//choose channel
	ADMUX &= ~(0x7);
	ADMUX |= channel;
	ADMUX |= 1<<ADLAR;

	//start conversion
	ADCSRA |= _BV(ADSC);

	//wait until conversion completes
	while (ADCSRA & _BV(ADSC) );

    volatile char ispis[10];
	//strcpy(ispis," ");
	strcpy(ispis,'\0');
	int i=0;
	
	for(i=0;i<10;i++){
		ispis[i]=" ";
	}
	
	//*ispis=itoa(ADCH,ispis,10); //
	
	itoa(ADCH,ispis,10);


    if (ADCH<200)
    {
        lcd_clrscr();
        //strcpy(ispis, "noc");
        //strcat(ispis, " noc");
    } else
    {
        lcd_clrscr();
        //strcat(ispis, " dan");
    }
    _delay_ms(750);

	return ispis;
}

volatile uint8_t getLight1(uint8_t channel)
{
	//choose channel
	ADMUX &= ~(0x7);
	ADMUX |= channel;
	ADMUX |= 1<<ADLAR;

	//start conversion
	ADCSRA |= _BV(ADSC);

	//wait until conversion completes
	while (ADCSRA & _BV(ADSC) );

    volatile uint8_t ispis[10];
	//strcpy(ispis," ");
	strcpy(ispis,'\0');
	int i=0;
	
	for(i=0;i<10;i++){
		ispis[i]=" ";
	}
	
	//*ispis=itoa(ADCH,ispis,10); //
	
	itoa(ADCH,ispis,10);


    if (ADCH<200)
    {
        lcd_clrscr();
        //strcpy(ispis, "noc");
        //strcat(ispis, " noc");
    } else
    {
        lcd_clrscr();
        //strcat(ispis, " dan");
    }
    _delay_ms(750);

	return ADCH;
}

int main(void)
{
	IO_Init();
	UART_Init();
	ADC_Init();

	sei();

	//potreban delay!!
	
	_delay_ms(5000);
	
	char AT[] = "AT"; // 4 znamenke potrebne
	char text_mode[] = "AT+CMGF=1";

    ///SMS
	char sms[]="AT+CMGS=";
	char *num ="00385996834050";
	//char sms_text[30]="Probni sms";

    //TEXT MODE
	_delay_ms(3000);
	USART_puts(text_mode);
	USART_putc(13);
	_delay_ms(3000);
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
		 _delay_ms(10000);



		if(read_over)
        {
				//read_over=0;

			if (strstr(rxBuffer, "LUX?"))
            {

				lcd_gotoxy(0,1);
				char *sms_text="Intenzitet: ";
				uint8_t adch=getLight1(1);
				char rez[10];
				char *dannoc; 
				if (adch>200){	
					dannoc=" dan";
				}else if(adch<200){
					dannoc=" noc";
				}
				
				
				lcd_gotoxy(0,0);
				itoa(adch,rez,10);
				
				lcd_puts(rez);
				lcd_puts(dannoc);
				
				
				//char *sms_full_text=strcat(sms_text,rez);
				//lcd_puts(sms_full_text);
	
				lcd_puts(sms);
				USART_puts(sms);

				lcd_putc(34);
				USART_putc(34);
				///kod staje!!!
				//lcd_puts(from_number);
				USART_puts(from_number);
				lcd_gotoxy(0,1);
				lcd_putc(34);
				USART_putc(34);
				_delay_ms(3000);

				lcd_putc(13);
				USART_putc(13);
				_delay_ms(1000);

				lcd_puts(sms_text);
				USART_puts(sms_text);
				USART_puts(rez);
				USART_puts(dannoc);
				
				USART_putc(26);// CTRL+z
				USART_putc(13);
				_delay_ms(3000);
	
				lcd_gotoxy(0,1);
				lcd_puts_P("poslano");

				char delete[]="at+cmgd=1";
				USART_puts(delete);
				USART_putc(13);
				
				_delay_ms(3000);
				
				int i;
				for (i=0; i < 128; i++)
                {
					rxBuffer[i] = '0';
				}

				read_over=0;

			}

			else
			{
                lcd_puts("Nema poruke");
			}

		}

    }

	return 0;
}
