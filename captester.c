// captester.c

/*************************************************************************
   captester device driver module
 *************************************************************************/

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"

#include "nrf52_bitfields.h"
#include "nrf_gpio.h"

#include "accel.h"
#include "nrf_drv_twi.h"
#include "nrf_delay.h"
#include "badgetester.h"
#include "swipesense.h"
#include "trace.h"
#include "mesh.h"
#include "unixtime.h"
#include "deviceaddress.h"
#include "uart.h"
#include "app_timer.h"
#include "nrf_drv_gpiote.h"
#include "app_uart.h"   // following are includes we got from meshsniffer
#include "serial.h"
#include "radio.h"
#include "buffers.h"

// rand stuff?
#include <stdio.h>
#include <stdlib.h>

/*************************************************************************
  DEFINES used here
 *************************************************************************/

#define TESTER_START 'G'
#define TESTER_TIMEOUT 27
#define TESTER_TIMEOUT2 5

#define TIMER_1MS 32

#define CAPACITIVE_PIN 24   // orange wire
#define LED_PIN 27          // white wire

// LOOK HERE IF YOU WANT TO EASILY CHANGE CONFIGURATION
// following are the RANGES we want to test! CHANGE THESE HOWEVER YOU WANT! (but never go below 1 unless i say so!)
#define REP_MIN 1           // minimum number of repetitions
#define REP_MAX 30          // max number of repetitions
#define ON_MIN 10           // minimum amount of time on
#define ON_MAX 5000          // max amount of time on
#define OFF_MIN 10          // min amount of time off
#define OFF_MAX 5000         // max amount of time on
#define RANDOM_MIN 1        // min randomness we add/sutract from on/off times.
#define RANDOM_MAX 500      // max randomness we add/subtract from on/off times. SET TO 0 if you don't want randomness
#define BEFORE_TEST 1000        // how much time before we test for errors. probably DON'T need to change
#define TEST_MIN 2500           // minimum amount of time we wait for errors. this is also delay before next round
#define TEST_MAX 25000      // max amount of time we wait for errors. this is also delay before next round

// do you want to manually SET A SEED? define it HERE!
#define SET_SEED 0          // otherwise, being set to 0 means we generate it randomly

// characters we display when we ping/detect signal
#define CHAR_PING ","
#define CHAR_DETECT "#"

/*************************************************************************
  Definition of global flag variables
 *************************************************************************/
uint16_t lastDetected = 0;		// variable to prevent triggering twice
int startTest = -1;				// determine when to start the Test
int ongoingTest = 0;            // determine when to print/keep iterating through loop
int testTrigger = 0;       // this is to check for error cases. if triggered, this isn't good!
int errorFound = 0;         // we set to 1 if we find an error. if it's 1, we display a message and freeze.
int testOnArray[REP_MAX];  // create an array of each "on" time we use, to be displayed if we encounter an error
int testOffArray[REP_MAX]; // same as above, except for "off" times

/*************************************************************************
  Global variables which keep track of some static data
 *************************************************************************/
 int testOnArray[REP_MAX];  // create an array of each "on" time we use, to be displayed if we encounter an error
 int testOffArray[REP_MAX]; // same as above, except for "off" times
 uint16_t waitTimer = 0;			// random number generator based off time to start test
 int testRound = 0;				// keeps track of each round we tested.

/*************************************************************************
  Global static variables for random testing
 *************************************************************************/
 // these are variables we will randomly generate for our test
int repetition;	// how many times we will repeat
int lengthOn;	// how long each time on is (ms)
int lengthOff;	// how long each time off is (ms)
int delayBetween;  // how long between each test
int randomness;		// how much we can randomly nudge around length on/off

/*************************************************************************
  Definition of functions
 *************************************************************************/

/* The function that gets called anytime we have an interrupt, meaning the wallsensor
* blinked at us. It gets called twice per "activation", because the LED blinks twice.
* Also handles the "setup" for us, whose sole purpose is to randomly generate a seed.
*/
void LedIrqHandler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
	// this will print only one every two times (led blinks twice per trigger)
	lastDetected = !lastDetected;
	if (lastDetected)
	{
		if (startTest == 1)		// this is when we've already started testing
		{
			sputs(CHAR_DETECT);	// every time we get an interrupt, we output this
            testTrigger = 1;       // triggered!
		}
		else		// this will only be called once, when we just finish setup
		{
            sputs("$");     // we start with this character so the Raspberry Pi knows to start recording
            sputnl();
			sputs("Let's get started! Our ");   // intro message when we start getting values
            if(SET_SEED == 0)       // set/randomly generate seed based off define value
            {
                sputs("randomly generated seed is: ");
                sputdec(waitTimer, 0);
                srand(waitTimer);
            }
            else
            {
                sputs("preconfigured seed is: ");
                sputdec(SET_SEED, 0);
                srand(SET_SEED);
            }
            sputnl();
            sputs("Whenever we display ");
            sputs(CHAR_PING);
            sputs(" it means we are 'pinging' the device with our capacitors.");
            sputnl();
            sputs("Whenever we display ");
            sputs(CHAR_DETECT);
            sputs(" it means we have received an LED blink response from the device");
            sputnl();
            sputs("First checking for errors... ");
            startTest = 0;		// now we can actually start the testing!
		}
	}
}

/* The constant function that constantly loops on the microprocessor. It tests the wallsensor thing,
 * by turning on/off capacitive plates randomly based on limits/ranges set above, searching
 * for any sort of bugs where the wallsensor continues to blink without being activated.
 * This function is very much designed around the fact that serial only outputs at the end of everything
 * loop of captester_process, so a lot of while/for loops are built around captester_process itself.
 * This way, we get more instant feedback from serial.
*/
void captester_process (void)
{
    if(errorFound)
    {
        nrf_delay_ms(60000);    // just loop and delay. repeat every minute
        sputnl();
        sputs("Error...");
        sputnl();
        sputs("^");             // give this char so the Raspberry Pi keeps spitting out debug time
    }
	else if(startTest == 1)
	{
        if(ongoingTest > 0)
        {
            int randomGen;

            sputs(CHAR_PING);     // every time we toggle the capacitor, output this
            nrf_gpio_pin_set(CAPACITIVE_PIN);       // turn our capacitive stuff on!
            randomGen = rand()%(randomness) - randomness/2;     // generate random number
            if ((lengthOn + randomGen) > 1)         // check that we aren't delaying a negative number
            {
                nrf_delay_ms(lengthOn + randomGen);         // delay!
            }
            else
            {
                nrf_delay_ms(1);
                testOnArray[ongoingTest] = 1;
            }
            testOnArray[ongoingTest] = lengthOn + randomGen;        // store it in our test array
            nrf_gpio_pin_clear(CAPACITIVE_PIN);     // turn our capacitive stuff off! and repeat!
            randomGen = rand()%(randomness) - randomness/2;
            if ((lengthOff + randomGen) > 1)
            {
                nrf_delay_ms(lengthOff + randomGen);         // delay!
            }
            else
            {
                nrf_delay_ms(1);
                testOffArray[ongoingTest] = 1;
            }
            testOffArray[ongoingTest] = lengthOff + randomGen;
            ongoingTest--;  // decrement each time we do it

            if(ongoingTest == 0)    // if ongoingTest has hit 0 just as we decrement
            {
                sputs("  CHECKING FOR ERRORS... ");   // output this to screen, then jump to end of function
            }
        }
        else    // if the test has ended
        {
            // this is to delay while we check for errors
            nrf_delay_ms(BEFORE_TEST);  // initial set-up wait time
            testTrigger = 0;    // set trigger to 0
            nrf_delay_ms(delayBetween); // wait for errors
            if(testTrigger)     // check for errors, then spew our error message if we have them!
            {
                sputs(" PREVIOUS TEST HAD ERROR!!! OMG!!!");
                sputnl();
                sputs("On-time testcases: ");       // display our on-time and off-time testcases
                for(int i=repetition; i>0; i--)
                {
                    sputdec(testOnArray[i], 0);
                }
                sputnl();
                sputs("Off-time testcases: ");
                for(int i=repetition; i>0; i--)
                {
                    sputdec(testOffArray[i], 0);
                }
                sputnl();
                sputs("^");     // send an error character so the raspberry pi knows we hit an error
                errorFound = 1; // set the error message flag!
            }
            else
            {
                // else, clear the test arrays and continue
                sputs("NO ERRORS FOUND.");
                for(int i=0; i<REP_MAX; i++)
                {
                    testOnArray[i] = 0;
                    testOffArray[i] = 0;
                }

                // generate the next round's test case. we do it like this so we can print it first
        		repetition = rand()%(REP_MAX-REP_MIN) + REP_MIN;	// 1-20
        		lengthOn = rand()%(ON_MAX-ON_MIN) + ON_MIN;	// 15-300
        		lengthOff = rand()%(OFF_MAX-OFF_MIN) + OFF_MIN; // 15-300
                delayBetween = rand()%(TEST_MAX-TEST_MIN) + TEST_MIN;
                if(RANDOM_MAX > 0)
                {
                    randomness = rand()%(RANDOM_MAX-RANDOM_MIN) + RANDOM_MIN;    // 1-50
                }
                ongoingTest = repetition;      // this sets up our "for" loop

                sputnl();
                sputs("&");     // use this character to show raspberry pi that we are starting display of another round, so it can show the time
        		sputs("  ROUND: ");
        		sputdec(testRound, 0);
                sputs("  REPETITION: ");
                sputdec(repetition, 0);
                sputs("  ON: ");
                sputdec(lengthOn, 0);
                sputs("  OFF: ");
                sputdec(lengthOff, 0);
                sputs("  DELAY BETWEEN ROUNDS: ");
                sputdec(delayBetween + BEFORE_TEST, 0);
                sputs("  RANDOMNESS: ");
                sputdec(randomness, 0);
                sputnl();

                testRound++;
            }
        }
	}
	else   // else if we haven't gone through setup yet
	{
		waitTimer++;  // increment the waitTimer. this helps generate our random seed
        if (startTest == 0) // this makes sure we reach this point when we setup
        {
            startTest = 1;
        }
	}
}

/*  The function that runs only at the beginning when we set up the microprocessor. Initializees
 * the interrupt and everything else.
*/
void captester_init (void)
{
        sputs("*"); // this character is so the Raspberry Pi knows to stop recording (if it still is), because we've hit a restart.
        sputnl();
        sputs("Captester initialized!!        ");    // intro message
        sputs("You can press the reset/reboot button to restart testing");
        sputnl();
		sputs("Please wave your hand over the thingy to get started");
        sputnl();

		/* Initialization Stuff! ---------------------------- */
		ret_code_t err_code;

		err_code = nrf_drv_gpiote_init();
    	APP_ERROR_CHECK(err_code);

        /* initialize test variables! ------------------------*/
        repetition = 0;	// set everything to zero at first!
        lengthOn = 0;
        lengthOff = 0;
        randomness = 0;

		/* set up the capacitor plate output! --------------- */
		nrf_gpio_cfg_output(CAPACITIVE_PIN);
		//nrf_gpio_cfg_input(LED_PIN, NRF_GPIO_PIN_PULLUP);
		nrf_gpio_pin_write(CAPACITIVE_PIN, 0);
		//nrf_gpio_cfg_input(LED_PIN, NRF_GPIO_PIN_PULLUP);

		/* not too sure what this is.. ---------------------- */
		nrf_drv_gpiote_in_config_t LED_PIN_config = GPIOTE_CONFIG_IN_SENSE_LOTOHI(true);
		LED_PIN_config.pull = NRF_GPIO_PIN_PULLUP;

		/* INTERRUPT STUFF! --------------------------------- */
		err_code = nrf_drv_gpiote_in_init(LED_PIN, &LED_PIN_config, LedIrqHandler);
		APP_ERROR_CHECK(err_code);
		nrf_drv_gpiote_in_event_enable(LED_PIN, true);
}
