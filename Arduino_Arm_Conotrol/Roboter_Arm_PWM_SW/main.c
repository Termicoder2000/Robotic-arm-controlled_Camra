/*
 * Roboter_Arm_PWM_SW.c
 *
 * Created: 20.10.2022 08:34:23
 * Author : heppn
 */ 


#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#define BAUD 9600UL
#define UBRR_VAL ((F_CPU+BAUD*8)/(BAUD*16)-1)
#define BAUD_REAL (F_CPU/(16*(UBRR_VAL+1)))
#define BAUD_ERROR ((BAUD_REAL*100)/BAUD)
#if ((BAUD_ERROR<99) || (BAUD_ERROR>101))
#error Fehler der Baudrate grösser 1% !
#endif
#define UART_MAXSTRLEN 20


#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/twi.h>
#include <stdbool.h>

#define SLAVE_ADRESSE 0x50 //Die Slave-Adresse
#define i2c_buffer_size 5
volatile uint8_t i2cdata[i2c_buffer_size];
void init_twi_slave(uint8_t adr);
volatile uint8_t buffer_adr; //"Adressregister" für den Buffer
bool i2c_fertig = false;

#define TWCR_ACK TWCR = (1<<TWEN)|(1<<TWIE)|(1<<TWINT)|(1<<TWEA)|(0<<TWSTA)|(0<<TWSTO)|(0<<TWWC);
#define TWCR_NACK TWCR = (1<<TWEN)|(1<<TWIE)|(1<<TWINT)|(0<<TWEA)|(0<<TWSTA)|(0<<TWSTO)|(0<<TWWC);
#define TWCR_RESET TWCR = (1<<TWEN)|(1<<TWIE)|(1<<TWINT)|(1<<TWEA)|(0<<TWSTA)|(1<<TWSTO)|(0<<TWWC);

void init_twi_slave(uint8_t adr)
{
	TWAR= (adr<<1); //Adresse setzen
	TWCR &= ~(1<<TWSTA)|(1<<TWSTO);
	TWCR|= (1<<TWEA) | (1<<TWEN)|(1<<TWIE);
	buffer_adr=0x00;
}

void uart_init(void)
{
	UBRR0 = UBRR_VAL;
	UCSR0B |= ((1<<RXEN0)|(1<<RXCIE0)|(1<<TXEN0));
	UCSR0C = ((1<<UCSZ01)|(1<<UCSZ00));
}
//TxD
void uart_putc(char c)
{
	while (!(UCSR0A & (1<<UDRE0))) {} // warten bis Sendenpuffer leer ist
	UDR0 = c; // sende Zeichen
}

void uart_puts (char *s)
{
	while (*s) // so lange *s != '\0'
	{
		uart_putc(*s);
		s++;
	}
}

void uart_puti( const int val )
{
	char buffer[8];
	uart_puts( itoa(val, buffer, 10) );
}


int main(void)
{
    sei();
	
	DDRB |= ((1<<PB1) | (1<<PB2));				// OC1A = PB1 / OC1B = PB2 ist Ausgang für SERVO
	DDRB |= (1<<PB3);							// Ausgang Timer 2
	
	
	//Timer 1
	TCCR1A |= (1<<WGM11)|(0<<WGM10);			// Fast PWM-Modus Mode 14
	TCCR1B |= (1<<WGM13)|(1<<WGM12); 			//
	TCCR1A |= (1<<COM1A1)|(0<<COM1A0);			// non-inverting Mode
	TCCR1A |= (1<<COM1B1)|(0<<COM1B0);			// non-inverting Mode
	TCCR1B |= ((0<<CS12)|(1<<CS11)|(0<<CS10));	// intern Prescaler=8
	
	//Interrupt enable, um "Start-Signal" für Timer 2 zu benutzen!
	TIMSK1 |= (1<<TOIE1);
	TIMSK2 |= (1<<OCIE2A);
	
	TCNT1 = 0;									// mit 0 starten.
	ICR1=39999;									// ICR1 mit 39999 laden für 20 ms
	
	// 2 ms -> 3999
	// 1 ms -> 2000 (eig 1999,5 -> es gehen aber nur Ganzzahlen)
	//Höhe (vom Gesicht) - 2000 bis 4200!!
	OCR1A = 2000;									// OCR1A mit 2999 laden für 1,5 ms
	//Nähe (zum Gesicht) - 2500 bis 
	OCR1B = 2800;									
	OCR2A = 150;
	
	uart_init();
	init_twi_slave(SLAVE_ADRESSE);
	sei();
	uart_puts("Test!\n");
	
	int xdiff = 0;
	int ydiff = 0;
	int diffmax = 0;
	bool x_pos = false;
	bool y_pos = false;
	int buff_timer_x = 150;
	int buff_timer_y = 2000;
	
    while (1) 
    {
		if(i2c_fertig)
		{
			//Gesamtwert der Differenz berechnen 
			//Befindet sich das Gesicht links oben im Bild, sind beide Werte negativ
			switch(i2cdata[0])
			{
				//X Positiv 
				case 1: 
					xdiff = i2cdata[1] + i2cdata[2];
					xdiff = xdiff / 50;
					switch(xdiff)
					{
						case 0:
						break;
						case 1:
						break;
						case 2:
						if((OCR2A - 5) > 66) OCR2A = OCR2A - 5;
						else OCR2A = 66;
						break;
						case 3:
						if((OCR2A - 10) > 66) OCR2A = OCR2A - 10;
						else OCR2A = 66;
						break;
						default:
						if((OCR2A - 10) > 66) OCR2A = OCR2A - 10;
						else OCR2A = 66;
						break;
					}
				break;
				
				//Y Positiv
				case 2:
					ydiff = i2cdata[1] + i2cdata[2];
					ydiff = ydiff / 50;
					switch(ydiff)
					{
						case 0:
						break;
						case 1:
						break;
						case 2:
						if((OCR1A - 250) > 2000) OCR1A = OCR1A - 250;
						else OCR1A = 2000;
						break;
						case 3:
						if((OCR1A -500) > 2000) OCR1A = OCR1A - 500;
						else OCR1A = 2000;
						break;
						default:
						OCR1A = 2000;
						break;
					}
				break;
				//X Negativ
				case 3:
					xdiff = i2cdata[1] + i2cdata[2];
					xdiff = xdiff / 50;
					switch(xdiff)
					{
						case 0:
						break;
						case 1:
						break;
						case 2:
						if((OCR2A + 5) < 250) OCR2A = OCR2A + 5;
						else OCR2A = 250;
						break;
						case 3:
						if((OCR2A + 10) < 250) OCR2A = OCR2A + 10;
						else OCR2A = 250;
						break;
						default:
						if((OCR2A + 10) < 250) OCR2A = OCR2A + 10;
						else OCR2A = 250;
						break;
					}
				break;
				//Y Negativ
				case 4:
					ydiff = i2cdata[1] + i2cdata[2];
					ydiff = ydiff / 50;
					switch(ydiff)
					{
						case 0:
						break;
						case 1:
						break;
						case 2:
						if((OCR1A + 250) < 4200) OCR1A = OCR1A + 250;
						else OCR1A = 4200;
						break;
						case 3:
						if((OCR1A + 500) < 4200) OCR1A = OCR1A + 500;
						else OCR1A = 4200;
						break;
						default:
						OCR1A = 4200;
						break;
					}
				break;					
				default:
				break;
			}

			uart_puts("i2cdata:\n");
			for(uint8_t i = 0; i < buffer_adr; i++)
			{
				uart_puti(i2cdata[i]);
				uart_puts("\n");
			}
			uart_puts("\n");
			buffer_adr = 0x00;
			i2c_fertig = false;
		}
    }
}
//Starte Timer 0, wenn Timer 1 ICR1 erreicht - um 20 ms Periodendauer einzuhalten
//Somit muss Timer 0 nur eine Periodendauer von max 2 ms erreichen (zwischen 1 ms und 2 ms muss das High-Signal dauern)
//Prescaler 128 -> 2 ms entsprechen einem Zählerwert von 248
//Prescaler 128 -> 1 ms entsprechen einem Zählerwert von 104

//Servos können von 0,5 - 2,5 ms angesteuert werden -> siehe Datenblatt technical MeArmPi
ISR(TIMER1_OVF_vect) 
{ 
	PORTB |= (1<<PB3);
	TCCR2B |= ((1<<CS22)|(0<<CS21)|(1<<CS20));	// Timer 2 starten - intern Prescaler=128
	TCNT2 = 0;
}

ISR(TIMER2_COMPA_vect)
{
	PORTB &=~ (1<<PB3);
	TCCR2B &=~ ((0<<CS22)|(0<<CS21)|(0<<CS20));	// Timer 2 anhalten
}

ISR (TWI_vect)
{
	uint8_t data=0;

	switch (TW_STATUS) //TWI-Statusregister prüfen und nötige Aktion bestimmen
	{
		// Slave Receiver
		case TW_SR_SLA_ACK: // 0x60 Slave Receiver, Slave wurde adressiert
		TWCR_ACK; // nächstes Datenbyte empfangen, ACK danach senden
		break;
		case TW_SR_DATA_ACK:				// 0x80 Slave Receiver, ein Datenbyte wurde empfangen
		if(!i2c_fertig)
		{
			data = TWDR;					//Empfangene Daten auslesen
			if(data == 255)			//255 als Trennzeichen. Da \n in ascii 255 ist, was einer legalen Servoeinstellung entspricht
			{
				i2cdata[buffer_adr] = '\0';
				i2c_fertig = true;
			}
			else
			{
				if(buffer_adr<i2c_buffer_size)
				{
					i2cdata[buffer_adr] = data;			//Daten in Buffer schreibe
					buffer_adr++;						//Buffer-Adresse weiterzählen für nächsten Schreibzugriff
				}
				else
				{
					i2cdata[buffer_adr] = '\0';
					i2c_fertig = true;
				}
			}
		}
		TWCR_ACK;
		break;

		//Slave transmitter
		case TW_ST_SLA_ACK: //0xA8 Slave wurde im Lesemodus adressiert und hat ein ACK zurückgegeben.
		case TW_ST_DATA_ACK: //0xB8 Slave Transmitter, Daten wurden angefordert
		TWCR_ACK;
		break;
		case TW_SR_STOP:
		TWCR_ACK;
		break;
		case TW_ST_DATA_NACK: // 0xC0 Keine Daten mehr gefordert
		case TW_SR_DATA_NACK: // 0x88
		case TW_ST_LAST_DATA: // 0xC8  Last data byte in TWDR has been transmitted (TWEA = “0”); ACK has been received
		default:
		TWCR_RESET;
		break;
	}
}

