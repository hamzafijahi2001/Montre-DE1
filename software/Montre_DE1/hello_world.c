#include "system.h"
#include "sys/alt_stdio.h"
#include "sys/unistd.h"   // contient le header de usleep
#include "sys/alt_irq.h"
#define ON 1
#define OFF 0
#define blink_delay 50000 // temps d'attente pour le clignotement de la montre pendant la mise � jour
// All global variables
unsigned char seven_seg_table[] =
	  {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
	   0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x00 };
short H,M,S,ms;  // variables de la montre
short chrono_M, chrono_S,chrono_ms; // variables du chronom�tre
short MJ=OFF, chrono=OFF, stop=ON, clear=OFF; // variables de contr�le des inputs
short blinking_R=OFF,blinking_L=OFF;           // variables de controle du clignotement

int press = 0;
volatile int edge_capture;
// All pointers
volatile int * TIMER_ptr = (int *) TIMER_BASE;
volatile int * KEY_ptr = (int *) KEY_BASE;
volatile int * SW_ptr = (int *) SW_BASE;
volatile int * green_LED_ptr = (int *) LEDG_BASE;
volatile int * red_LED_ptr = (int *) LEDR_BASE;
volatile int * HEX_ptr = (int *) HEX_BASE;

// Init function prototypes
void init_timer();
void init_key();

// Interrupt function prototypes
void handle_timer_interrupts(void* context, alt_u32 id);
void handle_key_interrupts(void* context, alt_u32 id);

// Other function prototypes
void timer_montre();
void timer_chrono();
void show(short R, short L); // affichage des deux parties du HEX
void show_blinking(short R, short L, char ch); // affichage d'une des parties du HEX en fonction de 'ch'
void blinking(); // clignotement de la partie de la montre (heure ou minute) � mettre � jour, � l'aide des deux fonctions pr�c�dentes
void HEX_update_display(); // actualisation du HEX
void SW_update_input(); // reception des inputs de SW et mise � jour des variables de controle

int main()
{
	// Inits
	init_timer();
	init_key();
	alt_putstr("Hello from Nios II!\n");
	while (1){
		SW_update_input(); // choix montre/chrono
		* red_LED_ptr = * SW_ptr ; // affichage de l'etat de SW sur LEDR
		HEX_update_display(); // Mise � jour du display HEX
		usleep(500);
	}

	return 0;
}

/* ----------- Init functions  -------------------- */

void init_timer()
	{
	    /* Recast the edge_capture pointer to match the alt_irq_register() function
	     * prototype. */
	    void* edge_capture_ptr = (void*) &edge_capture;
	    /* set the interval timer period for scrolling the HEX displays */
		*(TIMER_ptr + 1) = 0x7;	// STOP = 0, START = 1, CONT = 1, ITO = 1
	    alt_irq_register( TIMER_IRQ, edge_capture_ptr,handle_timer_interrupts );
	}

void init_key()
	{
		void* edge_capture_ptr = (void*) &edge_capture;
		* (KEY_ptr + 2) = 0xE;
		alt_irq_register( KEY_IRQ, edge_capture_ptr,handle_key_interrupts );
	}

/* --------------- End of init functions  -------------------- */


/* ----------- Interrupt functions  -------------------- */

void handle_timer_interrupts(void* context, alt_u32 id)
	{
	    /* Reset the Button's edge capture register. */
		*(TIMER_ptr) = 0; 				// Clear the interrupt

		timer_montre();

		if(chrono==ON ){
			timer_chrono();
		}
		// La LED green montre l'evolution de la seconde S de la montre
		if(chrono==OFF && MJ==OFF){
			*green_LED_ptr = S;
		}
		else{
			*green_LED_ptr = 0;
		}

	}

void handle_key_interrupts(void* context, alt_u32 id)
	{
		press = * (KEY_ptr +3) ;
		if (press&0x2){
			// chrono
			if(chrono==ON){
				stop=OFF; // d�clencher le chrono
			}
			// Montre
			else{
				// on/off de la mise � jour
				if (MJ==OFF){
					MJ = ON;
					blinking_L=ON;
					blinking_R=OFF;
				}
				else{
					MJ = OFF;
					blinking_L=OFF;
					blinking_R=OFF;
				}
			}

		}
		else if (press&0x4){
			//chrono
			if(chrono==ON){
				stop=ON;  // pause chrono
			}
			//Montre
			else{ // changement de la partie � mettre � jour
				if(blinking_L==ON && blinking_R==OFF){
					blinking_L=OFF;
					blinking_R=ON;
				}else{
					if(blinking_L==OFF && blinking_R==ON){
						blinking_L=ON;
						blinking_R=OFF;
					}
				}
			}
		}
		else if (press&0x8) {
			//chrono
			if(chrono==ON){
				clear=ON; // arr�ter/r�initialiser le chrono
			}
			//Montre
			else{ // incr�mentation de la partie choisie
				if(MJ==ON && blinking_L==ON){
					M= (M+1)%60;
				}
				if(MJ==ON && blinking_R==ON){
					H= (H+1)%24;
				}
			}
		}

		* (KEY_ptr + 3) = 0; // clear register of key interrupt
	}

/* ----------- End of interrupt functions  -------------------- */

/* ------------- Other functions  -------------------- */

void timer_montre(){
	ms++;
	if( MJ == ON ){
		ms =0;
		S=0;
	}
	if(ms==1000){
		ms=0;
		S++;
	}
	if(S==60){
		S=0;
		M++;
	}
	if(M==60){
		M=0;
		H++;
	}
	if(H==24){
		H=0;
	}
}

void timer_chrono(){
	if( clear == ON ){
		chrono_ms=0;
		chrono_S=0;
		chrono_M=0;
		clear=OFF;
		stop=ON;
	}
	if( stop == OFF ){
		chrono_ms ++;
	}
	if(chrono_ms==1000){
		chrono_ms=0;
		chrono_S++;
	}
	if(chrono_S==60){
		chrono_S=0;
		chrono_M++;
	}
	if(chrono_M==60){
		chrono_M=0;
	}
}

void show(short R, short L){

	short a,b,c,d;
    a= L%10;
    b=L/10;
    c=R%10;
    d=R/10;
   * HEX_ptr = ((seven_seg_table[d]<<24)& 0xFF000000) |
			   ((seven_seg_table[c]<<16)& 0x00FF0000) |
			   ((seven_seg_table[b]<< 8)& 0x0000FF00) |
			   ((seven_seg_table[a])& 0x000000FF);
}

void show_blinking(short R, short L, char ch){
	short a,b,c,d;
		a= L%10;
	    b=L/10;
	    c=R%10;
	    d=R/10;
	if(ch=='R'){
		* HEX_ptr =((seven_seg_table[17]<<24)& 0xFF000000) |
				   ((seven_seg_table[17]<<16)& 0x00FF0000) |
				   ((seven_seg_table[b]<< 8)& 0x0000FF00)  |
				   ((seven_seg_table[a])& 0x000000FF);
	}else{
		* HEX_ptr =((seven_seg_table[d]<<24)& 0xFF000000) |
				   ((seven_seg_table[c]<<16)& 0x00FF0000) |
				   ((seven_seg_table[17]<< 8)& 0x0000FF00)|
				   ((seven_seg_table[17])& 0x000000FF);
	}
}

void blinking(){
	char ch;
	if(blinking_L==ON){
		ch='L';
	}else{ // blinking_R est donc ON
		ch='R';
	}
	//chrono
	if( chrono == ON ){
		show_blinking(chrono_M,chrono_S,ch);
		usleep(blink_delay);
		show(chrono_M,chrono_S);
		usleep(blink_delay);
	}
	//Montre
	else{
		show_blinking(H,M,ch);
		usleep(blink_delay);
		show(H,M);
		usleep(blink_delay);
	}
}

void HEX_update_display(){
	if(blinking_L==ON || blinking_R==ON){
		blinking();
	}
	else{
		//chrono
		if(chrono==ON){
			show(chrono_M,chrono_S);
		}
		//Montre
		else{
			show(H,M);
		}
	}
}

void SW_update_input(){
	press = * SW_ptr  ;
	if (press&0x1){
		chrono = ON;
		MJ=OFF;
		blinking_L = OFF;
		blinking_R = OFF;
	}
	else{
		chrono = OFF;
	}
}

/* ----------- End of other functions  -------------------- */
