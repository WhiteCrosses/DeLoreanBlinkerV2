#define F_CPU 3330000

#define P50Hz 3
#define P100Hz 4

#include <avr/io.h>
#include <stdlib.h>
#include <avr/interrupt.h> 

#define MSEC_TO_TIM_TICKS(x) ((x)/10)
#define MTTT(x) MSEC_TO_TIM_TICKS(x)

#define LED_0 0x02        //PB1
#define LED_1 0x01        //PB0
#define LED_2 0x04        //PA2        
#define LED_3 0x08        //PA3
#define LED_4 0x80        //PA7

#define AUX 0x02;         //PA1


class LEDOutput {
  public:

    LEDOutput() {
        PORTB.DIR = 0xFF;
        PORTA.DIR = 0xFD;
        PORTA.PIN1CTRL |= (1<<3);
    }

    void Set(uint8_t o) {
      //o two lowest bits, output PB0 PB1
      //but need to rotate these bits
      uint8_t tmp = o & 0x03;
      PORTB.OUT = ((tmp & (1<<0))<<1) | ((tmp & (1<<1))>>1);
      //PORTA.OUT = 0xFF;

      //o bits 4-2
      //PA2, PA3, PA7
      o |= (o & (1<<4))<<3;
      PORTA.OUT = o & (1<<2 | 1<<3 | 1 << 7);
    }
};

/*
Instruction set:
    OPCODE  ARGS
0b  0 0     0 0 0 0 0 0
 
 Args range: 0 - 64

 PLAY    : 00  
 JMP     : 01  Jump always back, relative jump, steps number in args
 CNTRJMP : 10  Jump back 'time' times, steps number in args
*/



enum OPCODE {
  OPCODE_PLAY = 0,
  OPCODE_JMP = 1,
  OPCODE_CNTRJMP = 2
};

typedef struct animEntry {
  OPCODE opcode : 2;
  uint8_t args : 6;
  uint8_t leds;
  uint16_t time; 

} animEntry;


/*
  Container for animation + it's play context:
  Contains information about current step and repeat counter
  So it's possible to pause/continue animation if needed
  Decided to go this way as overhead is only 2 bytes per animation
  And it gives possibility to store animation context instead of 
  storing it externally if needed
*/
class Animation {
  private:
    const animEntry* anim;
    const uint8_t animSize;

    uint8_t current_step;
    uint8_t repeat_cntr;

  public:
    Animation(const animEntry *_anim, uint8_t _animSize) : 
      anim(_anim), animSize(_animSize), current_step(0), repeat_cntr(0) { };

    inline const animEntry* currentStep() {
      return &(anim[current_step]);
    }

    inline const animEntry* nextStep() {
      if ((current_step+1) >= this->animSize) {  //If animation ends without repeat, just report last step
        this->current_step = this->animSize; //One after last element, because that's next step
        return &(anim[current_step]);
      }

      const animEntry* tmp = &(anim[current_step]); 
      current_step++;
      return tmp;
    }

    inline void moveStepCounter(uint8_t n){
      if(n >= this->current_step)
        this->current_step = 0;
      else
        this->current_step -= n;
    }

    inline void setRepeatCounter(uint8_t n){
      repeat_cntr = n;
    }

    //Single call to repeat decreases repeat counter and returns true if anim
    //should be repeated, or false when not
    inline bool repeat(){
      if(repeat_cntr > 1) {
        repeat_cntr--;
        return true;
      }
      repeat_cntr = 0;
      return false;
    }

    inline bool repeatActive() {
      return repeat_cntr > 0;
    }

    const inline void reset() {
      current_step = 0;
      repeat_cntr = 0;
    }

};

/*
  Class responsible for executing opcodes for each instruction
  Controlling delay and flow of single animation
*/
class AnimationPlayer {
  private:
    const Animation* a;
    LEDOutput* out;

    uint8_t delayCounter = 0;
  public:

    AnimationPlayer(LEDOutput* _out) : out(_out) {};

    void setActiveAnimation(const Animation* new_a) {
      a = new_a;
    }

    void resetAnimation() {
      a->reset();
    }

    void step() {
      
      //If currently waiting, decrease delay counter and do nothing - wait
      if(delayCounter > 0){
        delayCounter--;
        return;
      }

      const animEntry* step = a->nextStep();

      switch(step->opcode) {
        case OPCODE_PLAY: 
          out->Set(step->leds);
          this->delayCounter = step->time;
          break;
        case OPCODE_JMP:
          a->moveStepCounter(step->args+1); //+1 because stepCounter is pointing to next instruction
          break;
        case OPCODE_CNTRJMP:
          if (a->repeatActive()) {
            if (a->repeat())
              a->moveStepCounter(step->args+1);
            return;
          } else {
          a->setRepeatCounter(step->time);
          a->moveStepCounter(step->args+1); 
          }
          break;
        default:
          break;
      }
    }
};

/*
  This is only class containing information about all animations
  Reponsible for selecting animations using externally provided method
  (sth like strategy design pattern used)
*/
class AnimationManager{
  private:
    const Animation** animList;
    uint8_t animListSize;
    const AnimationPlayer& p;


    uint8_t selectedAnimation = 0;
  public:
    AnimationManager(const Animation** _animList, uint8_t _animListSize, AnimationPlayer& _p) : 
      animList(_animList), animListSize(_animListSize), p(_p) {

        p.setActiveAnimation(animList[0]);
        p.resetAnimation();
      };

      void play(uint16_t freq){
        uint8_t newSelectedAnimation;
        //TODO: Depending on frequency select correct animation
        //Think how to connect external signal handling
        //Suggestion: create small simple interface class similar to LEDOutput
        //Which will give frequency as number 0, 1, 2: 0 -> 0 to 10hz, 1-> 11 to 70hz, 2-> over 70hz
        if(freq < 20)
          newSelectedAnimation = 0;
        else if (freq >= 40 && freq < 90)
          newSelectedAnimation = 1;
        else if (freq > 90 && freq < 150)
          newSelectedAnimation = 2;
        else 
          newSelectedAnimation = 3;

        if(newSelectedAnimation != selectedAnimation) {
          p.resetAnimation();
          p.setActiveAnimation(animList[newSelectedAnimation]);
          selectedAnimation = newSelectedAnimation;
        }
         
        //TODO: When animation changes, reset old animation
        p.step();
      }

};

constexpr animEntry animationSlow[] = 
{
  {OPCODE_PLAY, 0, 0b11110, MTTT(500)},
  {OPCODE_PLAY, 0, 0b11101, MTTT(500)}, 
  {OPCODE_PLAY, 0, 0b11011, MTTT(500)},
  {OPCODE_PLAY, 0, 0b10111, MTTT(500)},
  {OPCODE_JMP, 4, 0b00000, 0}, 
  {OPCODE_JMP, 1, 0b00000, 100} //Last step should be jump back 
};

constexpr animEntry animationMedium[] = 
{
  {OPCODE_PLAY, 0, 0b11110, MTTT(30)},
  {OPCODE_PLAY, 0, 0b11100, MTTT(30)},
  {OPCODE_PLAY, 0, 0b11101, MTTT(30)},
  {OPCODE_PLAY, 0, 0b11001, MTTT(30)}, 
  {OPCODE_PLAY, 0, 0b10011, MTTT(30)},
  {OPCODE_PLAY, 0, 0b10111, MTTT(30)},
  {OPCODE_JMP, 7, 0b00000, 3},
  {OPCODE_JMP, 1, 0b00000, 100} //Last step should be jump back 
};

constexpr animEntry animationFast[] = 
{
  {OPCODE_PLAY, 0, 0b00000, MTTT(500)},
  {OPCODE_PLAY, 0, 0b11111, MTTT(200)},
  {OPCODE_CNTRJMP, 2, 0b00000, 1},
  {OPCODE_PLAY, 0, 0b00000, MTTT(500)},
  {OPCODE_PLAY, 0, 0b10000, MTTT(20)},
  {OPCODE_PLAY, 0, 0b00000, MTTT(150)},
  {OPCODE_PLAY, 0, 0b10000, MTTT(20)},  
  {OPCODE_PLAY, 0, 0b00000, MTTT(100)},
  {OPCODE_JMP, 5, 0b00000, 0},
  {OPCODE_JMP, 1, 0b00000, 100} //Last step should be jump back 
};

constexpr animEntry animationLast[] = 
{
  {OPCODE_PLAY, 0, 0b00000, MTTT(1000)},
  {OPCODE_JMP, 1, 0b00000, 3},
  {OPCODE_JMP, 1, 0b00000, 100} //Last step should be jump back 
};

const LEDOutput leds;

const Animation a1(animationSlow, sizeof(animationSlow)/sizeof(animEntry));
const Animation a2(animationMedium, sizeof(animationMedium)/sizeof(animEntry));
const Animation a3(animationFast, sizeof(animationFast)/sizeof(animEntry));
const Animation a4(animationLast, sizeof(animationLast)/sizeof(animEntry));


const Animation* animList[4] = {&a2, &a3, &a4, &a1};

AnimationPlayer player(&leds); 

AnimationManager mgr(animList, 4, player);

volatile uint8_t flag_interrupt = 0;
volatile uint8_t flag_250ms = 0;
volatile uint8_t counter_interrupt = 0;

//Interrupt each 10ms
ISR(TCA0_OVF_vect)
{
  flag_interrupt = 1; //Set flag only, do job in main loop
  counter_interrupt++;
  if(counter_interrupt >= 25) {
    flag_250ms = 1;
    counter_interrupt = 0;
  }

  TCA0.SINGLE.INTFLAGS = 0x01;
}

int main() {

   //Clock divider to 64 and enable bit
  TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV64_gc;
  //Top value to period register (519 = 1/100th of a second)
  TCA0.SINGLE.PER = 519;  //1000Hz - 100ms
  //Enable overflow interrupt at bit 0 of interrupt controll
  TCA0.SINGLE.INTCTRL = 0x01;
  TCA0.SINGLE.CTRLA |= 1;

  //Enable interrupts globally by writing a '1' to the Global Interrupt Enable bit (I) in the CPU Statusregister (CPU.SREG).
  CPU_SREG |= 0x80;

  uint16_t last_input_frequency = 0;
  uint8_t curr_input_cntr = 0;
  uint8_t last_input = (PORTA.IN & (1 << 1));

  while(1){

    //Play the animation each 10ms!
    if(flag_interrupt) {
      flag_interrupt = 0;
      mgr.play(last_input_frequency);
    }

    //Each cycle check for input signal switch (freq meas)
    uint8_t curr_input = (PORTA.IN & (1 << 1));
    if(last_input != curr_input) {
      if(curr_input_cntr < 255) //protection against too high frequency
        curr_input_cntr++;
    }
    last_input = curr_input;

    //Each 250ms update frequency
    if(flag_250ms) {
      flag_250ms = 0;
       //Multiply by 4 (250ms * 4) and divide by 2 to get freq from pulses number 
      last_input_frequency = ((uint16_t)curr_input_cntr) * 4;
      curr_input_cntr = 0;
    }

  }

  return 0;
}
