// notice in this project, the value for ppl_m in system_stm32f4xx.c file is set to 8 instead of 25.

#include "stm32f4xx.h"
#include "stdbool.h"
#include "main.h"

GPIO_InitTypeDef GPIO_Initstructure;
TIM_TimeBaseInitTypeDef timer_InitStructure;
EXTI_InitTypeDef EXTI_InitStructure; // External interrupt
NVIC_InitTypeDef NVIC_InitStructure; // NVIC struct

// use for setup the clock speed for TIM2
const uint16_t TIMER_2_PRESCALER = 232;
const uint16_t TIMER_2_PERIOD = 2999;
const uint16_t TIMER_2_FREQUENCY = 120;

// setup the clock spped for TIM3
const uint16_t TIMER_3_PRESCALER = 2100 - 1;
const uint16_t TIMER_3_PERIOD = 10000 - 1;
const uint16_t TIMER_3_FREQUENCY = 4;

// setup the clock spped for TIM4
// this timer used for idle looping LED
const uint16_t TIMER_4_PRESCALER = 280 - 1;
const uint16_t TIMER_4_PERIOD = 10000 - 1;
const uint16_t TIMER_4_FREQUENCY = 30;


// LED names
const uint16_t GREEN = GPIO_Pin_12;
const uint16_t ORANGE = GPIO_Pin_13;
const uint16_t RED = GPIO_Pin_14;
const uint16_t BLUE = GPIO_Pin_15;

// size option for coffee
const uint16_t SMALL = GPIO_Pin_13; // orange
const uint16_t MEDIUM = GPIO_Pin_14; // red
const uint16_t EXTRA_LARGE = GPIO_Pin_15; // blue

// define few timing for events
const uint16_t LONG_PRESS_TIME = 2; // 3 seconds holding for long press.
const float MIN_PRESS_TIME = 0.05; // the min single press should need 0.05 second.
const float DOUBLE_CLICK_TIME = 0.5; // double press should be with in 0.5 second.
const uint16_t IDLE_TIME = 5;
const int SOUND_OUTPUT = 1; // output the sound for 1 second

// used to help calculate the time interval for some event
unsigned int timer_for_button_hold = 0;
unsigned int timer_for_button_released = 0;
unsigned int timer_for_idle = 0;

// keep track button states
bool is_button_up = true;

// variables used for checking clicks
bool within_double_click_period = false;
bool button_clicked = false;
bool is_single_click = false;
bool is_double_click = false;
bool is_long_click = false; // need to hold the button for more than 3 seconds

// the state for the machine
bool program_started = false;
bool display_default_timer = false;
bool is_programming_state = false; // this state allows user to change the timing for different size of coffee
bool is_idle = false;
bool is_finish_blink = false;

uint16_t error_LED_1 = 0;
uint16_t error_LED_2 = 0;
uint16_t display_LED_1 = 0;
int num_blink = 0;

int coffee_size = 0;
const int MAX_SIZE_OPTOIN = 3;

// predefine timing for coffee machine
uint16_t time_small = 2;
uint16_t time_medium = 4;
uint16_t time_ex_large = 6;
uint16_t new_num_click = 0;

fir_8 filt;
bool output_sound = false;
int timer_for_sound = 0;
bool start_sound_timer = false;


void UpdateMachineStatus(void);


void InitLEDs() {
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

  GPIO_Initstructure.GPIO_Pin = GREEN | ORANGE | RED | BLUE;
  GPIO_Initstructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_Initstructure.GPIO_OType = GPIO_OType_PP;
  GPIO_Initstructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Initstructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_Init(GPIOD, & GPIO_Initstructure);
}

void InitButton() {// initialize user button
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

  GPIO_Initstructure.GPIO_Mode = GPIO_Mode_IN;
  GPIO_Initstructure.GPIO_OType = GPIO_OType_PP;
  GPIO_Initstructure.GPIO_Pin = GPIO_Pin_0;
  GPIO_Initstructure.GPIO_PuPd = GPIO_PuPd_DOWN;
  GPIO_Initstructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_Init(GPIOA, & GPIO_Initstructure);
}

void InitTimer_2() {
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

  timer_InitStructure.TIM_Prescaler = TIMER_2_PRESCALER;
  timer_InitStructure.TIM_CounterMode = TIM_CounterMode_Up;
  timer_InitStructure.TIM_Period = TIMER_2_PERIOD;
  timer_InitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
  timer_InitStructure.TIM_RepetitionCounter = 0;
  TIM_TimeBaseInit(TIM2, & timer_InitStructure);
  TIM_Cmd(TIM2, ENABLE);
  TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
}

void InitTimer_3() {
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	
	timer_InitStructure.TIM_Prescaler = TIMER_3_PRESCALER;
	timer_InitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	timer_InitStructure.TIM_Period = TIMER_3_PERIOD;
	timer_InitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	timer_InitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM3, &timer_InitStructure);
	TIM_Cmd(TIM3, ENABLE);
	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
}

void InitTimer_4() {
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
	
	timer_InitStructure.TIM_Prescaler = TIMER_4_PRESCALER;
	timer_InitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	timer_InitStructure.TIM_Period = TIMER_4_PERIOD;
	timer_InitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	timer_InitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM4, &timer_InitStructure);
	TIM_Cmd(TIM4, ENABLE);
	TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
}

void EnableTimer2Interrupt() {
  NVIC_InitTypeDef nvicStructure;
  nvicStructure.NVIC_IRQChannel = TIM2_IRQn;
  nvicStructure.NVIC_IRQChannelPreemptionPriority = 0;
  nvicStructure.NVIC_IRQChannelSubPriority = 1;
  nvicStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init( & nvicStructure);
}

void EnableTimer3Interrupt() {
	NVIC_InitTypeDef nvicStructure;
	nvicStructure.NVIC_IRQChannel = TIM3_IRQn;
	nvicStructure.NVIC_IRQChannelPreemptionPriority = 0;
	nvicStructure.NVIC_IRQChannelSubPriority = 1;
	nvicStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvicStructure);
}

void EnableTimer4Interrupt() {
	NVIC_InitTypeDef nvicStructure;
	nvicStructure.NVIC_IRQChannel = TIM4_IRQn;
	nvicStructure.NVIC_IRQChannelPreemptionPriority = 0;
	nvicStructure.NVIC_IRQChannelSubPriority = 1;
	nvicStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvicStructure);
}

void DisableTimer4Interrupt() {
	NVIC_InitTypeDef nvicStructure;
	nvicStructure.NVIC_IRQChannel = TIM4_IRQn;
	nvicStructure.NVIC_IRQChannelPreemptionPriority = 0;
	nvicStructure.NVIC_IRQChannelSubPriority = 1;
	nvicStructure.NVIC_IRQChannelCmd = DISABLE;
	NVIC_Init(&nvicStructure);
}

// NVIC : Nested Vectored Interrupt Controller for EXTI in IRQ
void EnableEXTIInterrupt() {
  NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init( & NVIC_InitStructure);
}

// customize the EXTI for button 
void InitEXTI() {
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

  // Selects the GPIOA pin 0 used as external interrupt source
  SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);

  EXTI_InitStructure.EXTI_Line = EXTI_Line0;
  EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;

  //EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
  //EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
  EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;

  EXTI_Init( & EXTI_InitStructure);
}


void LEDOn(uint16_t GPIO_Pin) {
  GPIO_SetBits(GPIOD, GPIO_Pin);
}

void LEDOff(uint16_t GPIO_Pin) {
  GPIO_ResetBits(GPIOD, GPIO_Pin);
}

void TIM2_IRQHandler() {
  //Checks whether the TIM2 interrupt has occurred or not
  if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    
		timer_for_button_hold++;
    timer_for_button_released++;
	
    if (!is_button_up && timer_for_button_hold >= LONG_PRESS_TIME * TIMER_2_FREQUENCY) {
      is_long_click = true;
			
      // if not reset timer to 0, is_long_click will always be true;
      timer_for_button_hold = 0;
			timer_for_idle = 0;
			is_idle = false;
    }
		
		if ( !is_idle && program_started ) {
			timer_for_idle++;
			
			if ( timer_for_idle > IDLE_TIME * TIMER_2_FREQUENCY ) {
				if ( is_programming_state ) {
					// simply exit from programming mode
					is_programming_state = false;
				} else {
					is_idle = true;
					is_finish_blink = false;
				}
				timer_for_idle = 0;
			}
		}
		
		if (start_sound_timer) {
			timer_for_sound++;
			output_sound = timer_for_sound <= SOUND_OUTPUT * TIMER_2_FREQUENCY;
		}
		
  }
}

void TIM3_IRQHandler()
{
	// checks whether the tim3 interrupt has occurred or not
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
	{
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
		if ( num_blink && display_LED_1) {
			GPIO_ToggleBits(GPIOD, display_LED_1);
			num_blink--;
			timer_for_idle = 0;
		}
		if ( !num_blink ) is_finish_blink = true;
	}
}

void TIM4_IRQHandler()
{
	// checks whether the tim4 interrupt has occurred or not
	if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET)
	{
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
		if( is_idle ) {
			if ( num_blink <= 0 && !is_finish_blink ) {
				if ( coffee_size == 0 ) {
					LEDOff(MEDIUM);
					LEDOff(EXTRA_LARGE);
					LEDOn(SMALL);
					num_blink = time_small * 2;
				}
				else if ( coffee_size == 1 ) {
					LEDOff(SMALL);
					LEDOff(EXTRA_LARGE);
					LEDOn(MEDIUM);
					num_blink = time_medium * 2;
				}
				else if ( coffee_size == 2 ) {
					LEDOff(MEDIUM);
					LEDOff(SMALL);
					LEDOn(EXTRA_LARGE);
					num_blink = time_ex_large * 2;
				} 
				display_LED_1 = GREEN;
			}else {
				if ( !num_blink ) {
					LEDOn(RED);
					LEDOn(ORANGE);
					LEDOn(BLUE);
					start_sound_timer = true;
				}
				//coffee_size = (coffee_size + 1 ) % 3;
				//is_finish_blink = false;
			}
		}
	}
}

bool CanUpdateClickState() {
	return (!is_long_click && 
					program_started && 
					!is_button_up && 
					timer_for_button_hold >= MIN_PRESS_TIME * TIMER_2_FREQUENCY);
}

void EXTI0_IRQHandler() {
  // Checks whether the interrupt from EXTI0 or not
  if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
		
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0)) {
      if (is_button_up) {
        timer_for_button_hold = 0;
      }
      is_button_up = false;
    } else {
      if (CanUpdateClickState()) {
				if (!is_single_click && !is_double_click) {
					is_single_click = true;
				} else if (is_single_click && !is_double_click) {
					is_single_click = false;
					is_double_click = true;
				}
      }
      is_button_up = true;
      timer_for_button_released = 0;
    }
		
		// reset the timer for idle when button event happened
		timer_for_idle = 0;
		
		if ( is_idle ) {
			is_idle = false;
			coffee_size = -1;
		}
		
    // Clears the EXTI line pending bit
    EXTI_ClearITPendingBit(EXTI_Line0);
  }
}

int GetMaxTime() {
	int max = time_small;
	if (max < time_medium) {
		max = time_medium;
	}
	if (max < time_ex_large) {
		max = time_ex_large;
	}
	return max;
}

void UpdateCoffeeTiming() {
	if (coffee_size == 0) time_small = new_num_click;
	else if (coffee_size == 1) time_medium = new_num_click;
	else if (coffee_size == 2) time_ex_large = new_num_click;
}

void UpdateProgrammingStatus() {
	if (display_default_timer && num_blink <= 0) {
		// only response to the click when finish blinking the LED.
		within_double_click_period = (timer_for_button_released <= (DOUBLE_CLICK_TIME * TIMER_2_FREQUENCY));
		if (is_single_click && !within_double_click_period) {
			new_num_click++;
			is_single_click = false;
		} else if(is_long_click) {
			
			if ( new_num_click > 0) {
				UpdateCoffeeTiming();
				new_num_click = 0;
				is_programming_state = false;
			}
			is_long_click = false;
			is_double_click = false;
			is_single_click = false;
			is_button_up = true;
		} else if(is_double_click)  {
			// if the user press too fast, then it will count as 2 clicks 
			new_num_click+=2;
			is_double_click = false;
		}
	} else {
		// this just for reset the button click,
		// because even user click button when displaying the LED,
		// program should ignore it.

		if (is_single_click) is_single_click = false;
		if (is_long_click) is_long_click = false;
		if (is_double_click) is_double_click = false;
	}
}

void UpdateNonProgrammingStatus() {
	within_double_click_period = (timer_for_button_released <= (DOUBLE_CLICK_TIME * TIMER_2_FREQUENCY));
	if (is_single_click && !within_double_click_period) {
		coffee_size = (coffee_size + 1) % MAX_SIZE_OPTOIN;
		is_single_click = false;
	} else if (is_double_click) {
		is_programming_state = true;
		new_num_click = 0;
		timer_for_idle = 0;
		display_default_timer = false;
		is_double_click = false;
	}
}

void UpdateMachineStatus() {
  if (program_started) {
    if (is_programming_state) {
			UpdateProgrammingStatus();
    } else
			UpdateNonProgrammingStatus();
  } else if (is_long_click && !program_started) {
    is_long_click = false;
    program_started = true;

    /*
    NOTE that the following set is_button_up is not "right"
    this is just a quick bug fix.
    BUG: when start the machine by holding the button, as soon as we release the button, 
    it also treate it as a button click, so the LED will switch from oriange to red
    */
    is_button_up = true;
  }

}

void DisplayLED() {
	if ( coffee_size == 0) {
		display_LED_1 = SMALL;
		num_blink = time_small * 2;
	}
	else if (coffee_size == 1) {
		display_LED_1 = MEDIUM;
		num_blink = time_medium * 2;
	}
	else if (coffee_size == 2) {
		display_LED_1 = EXTRA_LARGE;
		num_blink = time_ex_large * 2;
	}
	display_default_timer = true;
}

void ShowProgrammingLED() {
	// every time when first enter programming mode, it should tell user the pre-defined time
	if (!display_default_timer)
		DisplayLED();
	
	
	if ( num_blink <= 0 ) { 
		LEDOn(GREEN);
		if ( is_button_up ) LEDOn(display_LED_1);
		else LEDOff(display_LED_1);
	}
}

void ShowSizeLED() {
	LEDOff(GREEN);
	if (coffee_size == 0) {
		LEDOff(EXTRA_LARGE);
		LEDOff(MEDIUM);
		LEDOn(SMALL);
	} else if (coffee_size == 1) {
		LEDOff(SMALL);
		LEDOff(EXTRA_LARGE);
		LEDOn(MEDIUM);
	} else if (coffee_size == 2) {
		LEDOff(MEDIUM);
		LEDOff(SMALL);
		LEDOn(EXTRA_LARGE);
	}
}	

void ShowLED() {
	if (is_programming_state) {
		ShowProgrammingLED();
	} else
		ShowSizeLED();
}

int main() {
	InitLEDs();
	InitButton();
	InitEXTI();
	InitTimer_2();
	InitTimer_3();
	InitTimer_4();
	EnableTimer2Interrupt();
	EnableTimer3Interrupt();
	EnableEXTIInterrupt();
	
	/*
	This block is for sound setup
	*/
	SystemInit();

	//enables GPIO clock for PortD
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(GPIOD, &GPIO_InitStructure);

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

	codec_init();
	codec_ctrl_init();

	I2S_Cmd(CODEC_I2S, ENABLE);

	initFilter(&filt);
	
	/*
	sound set up
	*/
	
	
	while(1) {
		UpdateMachineStatus();
		if (program_started)  {
			if ( !is_idle )	{
				DisableTimer4Interrupt();
				ShowLED();
				output_sound = false;
				timer_for_sound = 0;
				start_sound_timer = false;
			} else	{
				EnableTimer4Interrupt();
			}
			
			if (SPI_I2S_GetFlagStatus(CODEC_I2S, SPI_I2S_FLAG_TXE) && output_sound)
    	{
    		SPI_I2S_SendData(CODEC_I2S, sample);

    		//only update on every second sample to insure that L & R ch. have the same sample value
    		if (sampleCounter & 0x00000001)
    		{
    			sawWave += NOTEFREQUENCY;
    			if (sawWave > 1.0)
    				sawWave -= 2.0;

    			filteredSaw = updateFilter(&filt, sawWave);
    			sample = (int16_t)(NOTEAMPLITUDE*filteredSaw);
    		}
    		sampleCounter++;
    	}
			
		}
	}
}

// the following code is for sound
// a very crude FIR lowpass filter
float updateFilter(fir_8* filt, float val)
{
	uint16_t valIndex;
	uint16_t paramIndex;
	float outval = 0.0;

	valIndex = filt->currIndex;
	filt->tabs[valIndex] = val;

	for (paramIndex=0; paramIndex<8; paramIndex++)
	{
		outval += (filt->params[paramIndex]) * (filt->tabs[(valIndex+paramIndex)&0x07]);
	}

	valIndex++;
	valIndex &= 0x07;

	filt->currIndex = valIndex;

	return outval;
}

void initFilter(fir_8* theFilter)
{
	uint8_t i;

	theFilter->currIndex = 0;

	for (i=0; i<8; i++)
		theFilter->tabs[i] = 0.0;

	theFilter->params[0] = 3;
	theFilter->params[1] = 3;
	theFilter->params[2] = 3;
	theFilter->params[3] = 3;
	theFilter->params[4] = 3;
	theFilter->params[5] = 3;
	theFilter->params[6] = 3;
	theFilter->params[7] = 3;
}

