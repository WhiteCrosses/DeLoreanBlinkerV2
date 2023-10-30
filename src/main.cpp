#define F_CPU 3330000
#define ALLOFF 0xFF

#define SELECTED_PROGRAM 1

#define LED_TOP 0x0D
#define LED_MID 0x0B
#define LED_BOT 0x0E

#define LED_0 0x02        //PB1
#define LED_1 0x01        //PB0
#define LED_2 0x04        //PA2        
#define LED_3 0x08        //PA3
#define LED_4 0x80        //PA7

#define AUX 0x02;         //PA1

#define ANIMATION_2_FRAMES 6
#define ANIMATION_1_FRAMES 17
#define ANIMATION_0_FRAMES 6


#define DELAY 1000

#include <avr/io.h>
#include <avr/interrupt.h> 
#include <util/delay.h>
#include <avr/sleep.h>
#include <util/atomic.h>

volatile int clock_counter = 0;
volatile uint8_t pulse_counter = 0;
volatile int selected_animation = 1;

const int animation_2[ANIMATION_2_FRAMES][6] = {
  {0, 0, 0, 0, 1, 100},
  {0, 0, 0, 1, 0, 100},
  {0, 0, 0, 0, 1, 100},
  {0, 0, 0, 1, 0, 100},
  {0, 0, 0, 0, 1, 100},
  {0, 0, 0, 1, 0, 100}
};

const int animation_0[ANIMATION_0_FRAMES][6] = {
  {1, 1, 1, 0, 1, 5},
  {1, 1, 0, 1, 1, 5},
  {1, 0, 1, 1, 1, 5},
  {0, 1, 1, 1, 1, 5},
  {1, 0, 1, 1, 1, 5},
  {1, 1, 0, 1, 1, 5}

};
const int animation_1[ANIMATION_1_FRAMES][6] = {
  {0, 0, 0, 0, 1, 200},
  {0, 0, 0, 1, 1, 5},
  {0, 0, 1, 0, 1, 5},
  {0, 1, 0, 0, 1, 5},
  {1, 0, 0, 0, 1, 5},
  {0, 0, 0, 1, 1, 4},
  {0, 0, 1, 0, 1, 4},
  {0, 1, 0, 0, 1, 4},
  {1, 0, 0, 0, 1, 4},
  {0, 0, 0, 1, 0, 3},
  {0, 0, 1, 0, 1, 3},
  {0, 1, 0, 0, 0, 3},
  {1, 0, 0, 0, 1, 3},
  {0, 0, 0, 1, 0, 2},
  {0, 0, 1, 0, 1, 2},
  {0, 1, 0, 0, 0, 2},
  {1, 0, 0, 0, 1, 2}
};

const int state_machine[21][2]={
  {13, 1},
  {13, 2},
  {3, 8},
  {4, 1},
  {15, 5},
  {13, 6},
  {7, 8},
  {0, 1},
  {3, 9},
  {10, 9},
  {11, 1},
  {12, 1},
  {0, 1},
  {14, 1},
  {15, 1},
  {16, 1},
  {17, 1},
  {18, 1},
  {19, 1},
  {20, 1},
  {0, 1}
};

volatile int state = 0;
volatile uint8_t current_frame = 0;
ISR(TCA0_OVF_vect)
{
  //ending conditions
  pulse_counter++;
  if((state == 12) && (!(PORTA.IN & (1 << 1)))){    //50Hz
    if(selected_animation != 1){
      current_frame = 0;
    }
    selected_animation = 1;
    
  }
  else if((state == 7) && (!(PORTA.IN & (1 << 1)))){ //100Hz
    if(selected_animation != 2){
        current_frame = 0;
    }
    selected_animation = 2;
  }
  else if((state == 20) && (!(PORTA.IN & (1 << 1)))){ //Silent
    if(selected_animation != 0){
        
        current_frame = 0;
    }
    selected_animation = 0;
  }

  if((selected_animation == 0) && (pulse_counter >= animation_0[current_frame][5])){
    current_frame++;
    pulse_counter = 0;
  }
  else if((selected_animation == 1) && (pulse_counter >= animation_1[current_frame][5])){
    current_frame++;
    pulse_counter = 0;
  }
  else if((selected_animation == 2) && (pulse_counter >= animation_2[current_frame][5])){
    current_frame++;
    pulse_counter = 0;
  }

  if(PORTA.IN & (1 << 1)){
    state = state_machine[state][1];
  }
  else{
    state = state_machine[state][0];
  }
  
  TCA0.SINGLE.CTRLESET = 0x08;
}
int main(void)
{
  //CCP = 0xD8;

  //Clock divider to 64 and enable bit
  TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV64_gc;
  TCA0.SINGLE.CTRLA |= 1;

  //Top value to period register (523 = 1/1000th of a second)
  TCA0.SINGLE.PER = 1307;  //400Hz u = 1000/400

  //Enable overflow interrupt at bit 0 of interrupt controll
  TCA0.SINGLE.INTCTRL = 0x01;

  //Enable interrupts globally by writing a '1' to the Global Interrupt Enable bit (I) in the CPU Statusregister (CPU.SREG).
  CPU_SREG |= 0x80;

  PORTB.DIR = 0xFF;
  PORTA.DIR = 0xFD;

  
  PORTA.OUT = 0xFD;
  PORTB.OUT = 0xFF;
  PORTA.PIN6CTRL = 0x00;

  uint8_t animation_counter = 0;
  while(1){

    uint8_t tempB = 0;
    uint8_t tempA = 0;
    uint8_t val = 0;
    if(selected_animation == 0){

      if(current_frame > ANIMATION_0_FRAMES - 1){
        current_frame = 0;
      }

      val = animation_0[current_frame][0];
      tempB |= (val << 1);
      val = animation_0[current_frame][1];
      tempB |= (val << 0);

      val = animation_0[current_frame][2];
      tempA |= (val << 2);
      val = animation_0[current_frame][3];
      tempA |= (val << 3);
      val = animation_0[current_frame][4];
      tempA |= (val << 7);

      


    }
    else if(selected_animation == 1){

      if(current_frame > ANIMATION_1_FRAMES - 1){
        current_frame = 0;
      }

      val = animation_1[current_frame][0];
      tempB |= (val << 1);
      val = animation_1[current_frame][1];
      tempB |= (val << 0);

      val = animation_1[current_frame][2];
      tempA |= (val << 2);
      val = animation_1[current_frame][3];
      tempA |= (val << 3);
      val = animation_1[current_frame][4];
      tempA |= (val << 7);


      if(current_frame == 5){
        animation_counter++;
        current_frame = 1;
        if(animation_counter == 5){
          animation_counter = 0;
          current_frame = 6;
        }
      }
      else if(current_frame == 8){
        animation_counter++;
        current_frame = 6;
        if(animation_counter == 15){
          animation_counter = 0; 
          current_frame = 9;
        }
      }
        
      else if(current_frame == 12){
        animation_counter++;
        current_frame = 9;
        if(animation_counter == 15){
          current_frame = 0;
          animation_counter = 0;
        }
      }

      else if(current_frame == 16){
        animation_counter++;
        current_frame = 13;
        if(animation_counter == 30){
          current_frame = 0;
          animation_counter = 0;
        }
      }

    }
    else if(selected_animation == 2){

      if(current_frame > ANIMATION_2_FRAMES - 1){
        current_frame = 0;
      }

      val = animation_2[current_frame][0];
      tempB |= (val << 1);
      val = animation_2[current_frame][1];
      tempB |= (val << 0);

      val = animation_2[current_frame][2];
      tempA |= (val << 2);
      val = animation_2[current_frame][3];
      tempA |= (val << 3);
      val = animation_2[current_frame][4];
      tempA |= (val << 7);
    }
    PORTB.OUT = tempB;
    PORTA.OUT = tempA;
  }

}