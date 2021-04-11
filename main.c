/*
 * EE4524 project 2
 * Ultrasound project
 * Description: Distance is measured using HCSR04 ultrasonic module, this either displays the distance
 * on the data visualizer or adjusts the servo accordingly
 *
 * Created: 20/04/2019 
 * Author : Eoghan O'Connor 16110625
 *			
 */ 

#define F_CPU 16000000UL

//Include files
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>

//Declare functions
void sendmsg (char *s);
void init_USART();
void init_Timer();
void servo_display();
void continueous_display(void);

//USART send message variables
unsigned char qcntr = 0,sndcntr = 0;   //indexes into the queue
unsigned char queue[50];       //character queue
char message_buff[50]; //char array for storing messages to print

//Flag variables
unsigned int cont_mode=0;
unsigned int servo_display=0;

//timer variables and time period variables
unsigned char timecount;    // Extends TCNT of Timer1
unsigned int start_edge, end_edge;  // globals for times.
unsigned long Time_Period_High;
unsigned long Time_Period_Low;

//variable for storing distance value
volatile unsigned long distance;


/************************************************************************/
/* Main Function														*/
/************************************************************************/
int main(void)
{
	char ch;

	DDRB = 0b00001000; // port B
	DDRD = 0b00010000; //port D bit 4 output

	init_USART();
	init_Timer();
	
	sei();
	
	//initializing variables
	start_edge = 0;
	end_edge = 0;
	
	while(1)
	{
		if (UCSR0A & (1<<RXC0)) //check for character received
		{
			ch = UDR0;
			switch (ch)
			{
				/*Servo displays the distance in terms of angle*/
				case'S':
				case's':
				servo_display = 1;
				break;
				/*Exits the servo displays mode*/
				case'E':
				case'e':
				servo_display = 0;
				break;
				
				/*send distance to the data visualizer*/
				case 'M':
				case 'm':
				sprintf(message_buff,"%.2lucm from target",distance);
				sendmsg(message_buff);
				break;
				
				/*Continue display mode*/
				case 'V':
				case 'v':
				cont_mode= 1;
				break;
				/*Turn off Continue display mode*/
				case 'W':
				case 'w':
				cont_mode= 0;
				break;
				
				default:
				sprintf(message_buff,"Not a command",);
				sendmsg(message_buff);
				break;
			}//end of switch
		}//end of if
		
		continueous_display();
		
	}// end of while
}//end of main

/************************************************************************/
/* INTIALIZATIONS														*/
/************************************************************************/

void init_USART()
{
	UCSR0A = 0x00;
	UCSR0B = (1<<RXEN0) | (1<<TXEN0) | (1<<TXC0);  /*enable receiver, transmitter and transmit interrupt*/
	UBRR0 = 103;  /*baud rate = 9600*/
}



void init_Timer()
{
	TCCR1B = (1<<ICES1) | (1<<CS11);// input capture on falling edge, clk/8
	TIMSK1 = (1<<ICIE1) | (1<<TOIE1);  // capture and overflow interrupts enabled

	
	TCCR0B = (1<<CS00);// clk/1 as TC0 clock source

	
	TCCR2A = (2<<COM2A0)|(3<<WGM20);// Enable Fast PWM Mode, TOP = 0xff, non-inverting mode
	TCCR2B = (7<<CS20);// clk/1024 as TC2 clock source
}

/************************************************************************/
/* ISR INTERRUPTS														*/
/************************************************************************/

//USART INTERRUPT
ISR(USART_TX_vect)
{
	//send next character and increment index
	if (qcntr != sndcntr)
	UDR0 = queue[sndcntr++];
}


ISR(TIMER1_OVF_vect)
{
	++timecount;	//Increment overflow counter on interrupt
	TCNT0 = delay_10us;/*reset timer0*/
	
	PORTD ^= 0x10;/*toggle port D bit 4 every 10us*/
}

ISR(TIMER1_CAPT_vect)
{
	static unsigned long last_time_period; /*variable used to check if time period has changed*/
	unsigned long clocks;//unsigned long clocks;     /* count of clocks in the pulse - not needed outside the ISR, so make it local */
	end_edge = ICR1;        /* The C compiler reads two 8bit regs for us  */
	clocks = ((unsigned long)timecount * 65536) + (unsigned long)end_edge - (unsigned long)start_edge;
	timecount = 0;     // Clear timecount for next time around
	start_edge = end_edge;
	
	// Save its time for next time through here
	if(TCCR1B &(1<<ICES1))/*detect falling edge*/
	{
		Time_Period_Low = clocks/2; // in microsecs
		TCCR1B ^= 1<<ICES1;/*invert ICES1 bit of TCCR1B*/
	}
	else
	{
		Time_Period_High = clocks/2; // in microsecs
		TCCR1B ^= 1<<ICES1;/*invert ICES1 bit of TCCR1B*/
	}

	/*Calculate time period*/
	Time_Period = Time_Period_High + Time_Period_Low;
	
	/*Check if time period has changed*/
	if(last_time_period != Time_Period)
	
	/*update last time period*/
	last_time_period = Time_Period;
	distance = Time_Period_High/58;
	
}



/************************************************************************/
/* FUNCTIONS														    */
/************************************************************************/

/************************************************************************************/
/* USART sendmsg function															*/
/*this function loads the queue and													*/
/*starts the sending process														*/
/************************************************************************************/

void sendmsg (char *s)
{

	qcntr = 0;    //preset indices
	sndcntr = 1;  //set to one because first character already sent
	
	queue[qcntr++] = 0x0d;   //put CRLF into the queue first
	queue[qcntr++] = 0x0a;
	while (*s)
	queue[qcntr++] = *s++;   //put characters into queue
	
	UDR0 = queue[0];  //send first character to start process
	
	
	
}

/*
* Converts the distance calculated from the ultrasound to an angle
* on the servo motor. Distances between 0-160cm, servo angle 0-180 degrees
*/
void servo_display()
{
	if(distance == 0)
	OCR2A = 0;
	if(distance <= 10)
	OCR2A = 2;
	else if(distance >= 10 && distance <= 20)
	OCR2A = 4;
	else if(distance >= 20 && distance <= 30)
	OCR2A = 6;
	else if(distance >= 30 && distance <= 40)
	OCR2A = 8;
	else if(distance >= 40 && distance <= 50)
	OCR2A = 10;
	else if(distance >= 50 && distance <= 60)
	OCR2A = 12;
	else if(distance >= 60 && distance <= 70)
	OCR2A = 14;
	else if(distance >= 70 && distance <= 80)
	OCR2A = 16;
	else if(distance >= 80 && distance <= 90)
	OCR2A = 18;
	else if(distance >= 90 && distance <= 100)
	OCR2A = 20;
	else if(distance >= 100 && distance <= 110)
	OCR2A = 22;
	else if(distance >= 110 && distance <= 120)
	OCR2A = 24;
	else if(distance >= 120 && distance <= 130)
	OCR2A = 26;
	else if(distance >= 130 && distance <= 140)
	OCR2A = 28;
	else if(distance >= 140 && distance <= 150)
	OCR2A = 30;
	else if(distance >= 150 && distance > 160)
	OCR2A = 32;
	else
	OCR2A = 34;
}




/*
* Continuous function
* This displays either the adc reading or the 555 timer if either are selected
*/
void continueous_display(void)
{
	if(qcntr == sndcntr)
	{
		if(cont_mode)
		{
			sprintf(message_buff,"Distance is %d cm",distance);
			sendmsg(message_buff);
			
		}//end of cont mode if statement
		
		
	}//end if
	
}//end of function


