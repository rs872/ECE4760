/*
 * File:        Test of compiler fixed point
 * Author:      Bruce Land
 * Adapted from:
 *              main.c by
 * Author:      Syed Tahmid Mahbub
 * Target PIC:  PIC32MX250F128B
 */

////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config_1_3_2.h"
// threading library
#include "pt_cornell_1_3_2_python.h"

////////////////////////////////////
// graphics libraries
#include "tft_master.h"
#include "tft_gfx.h"
// need for rand function
#include <stdlib.h>
// fixed point types
#include <stdfix.h>
////////////////////////////////////
#include <math.h>

//macro function to get the absolute value
#define abs(x) ((x > 0) ? x: -x)

//used for Python GUI
char new_string = 0;
char new_slider = 0;

// current slider
int slider_id;
float slider_value ; // value could be large


// current string
char receive_string[64];

//Global variables to tune boids behavior
_Accum turn_factor = 0.32;
_Accum visual_range = 20;
_Accum protected_range = 10;
_Accum centering_factor = 0.0005;
_Accum avoid_factor = 0.05;
_Accum matching_factor = 0.05;

//speed limits
_Accum max_speed = 3;
_Accum min_speed = 2;

//pixels to define ranges in the TFT
#define top_margin 50
#define bottom_margin 50
#define left_margin 50
#define right_margin 50

// # of boids 
# define boids 112

//boid arrays for position and velocities for each boid
_Accum x_pos[boids];
_Accum y_pos[boids];
_Accum x_vel[boids];
_Accum y_vel[boids];

//pixel values for the x and y of the tft
#define tft_x 320
#define tft_y 240

//macro to convert between accums,floats and ints
#define float2Accum(a) ((_Accum)(a))
#define Accum2float(a) ((float)(a))
#define int2Accum(a) ((_Accum)(a))
#define Accum2int(a) ((int)(a))


/* Demo code for interfacing TFT (ILI9340 controller) to PIC32
 * The library has been modified from a similar Adafruit library
 */
// Adafruit data:
/***************************************************
  This is an example sketch for the Adafruit 2.2" SPI display.
  This library works with the Adafruit 2.2" TFT Breakout w/SD card
  ----> http://www.adafruit.com/products/1480
  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!
  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

// string buffer
char buffer[60];

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_timer, pt_color, pt_anim ;

// system 1 second interval tick
int sys_time_seconds ;

// === Timer Thread =================================================
// update a 1 second tick counter
static PT_THREAD (protothread_timer(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1) {
        // yield time 1 second
        PT_YIELD_TIME_msec(1000) ;
        tft_fillRect(0,0,100,100,ILI9340_BLACK); //Make background black
        sys_time_seconds++ ; //increment sys time since this thread runs every second
        tft_setCursor(0, 0); 
        tft_setTextColor(ILI9340_WHITE); 
        tft_setTextSize(1);
        //tft_writeString("Time in seconds since boot\n"); 
        tft_setTextColor(ILI9340_YELLOW); tft_setTextSize(2);
        tft_setCursor(0, 10);
        sprintf(buffer,"%d", sys_time_seconds); //write number of seconds to TFT
        tft_writeString(buffer);
        
        tft_setCursor(0, 30);
        //tft_writeString("FPS\n");
        sprintf(buffer,"%d\n", 30); //write the FPS value to the TFT (30 FPS)
        tft_writeString(buffer);
     
        //tft_writeString("Boids\n");
        sprintf(buffer,"%d", boids); // write number of boids to the TFT
        tft_writeString(buffer);

        // draw sys_time
        // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
} // timer thread

// === Animation Thread =============================================
// update a 1 second tick counter
static PT_THREAD (protothread_anim(struct pt *pt))
{
    PT_BEGIN(pt);    
    while(1) {
        // yield time 1 second
        int begin_time = PT_GET_TIME(); //get start time
        int i;
        for (i = 0; i < boids; i++){ //loop through each boid to set its velocity and position
            // erase disk
            tft_drawPixel(Accum2int(x_pos[i]), Accum2int(y_pos[i]), ILI9340_BLACK); //x, y, radius, color
            //initialize variables for the animation
            _Accum xpos_avg = 0;
            _Accum ypos_avg = 0;
            _Accum xvel_avg = 0;
            _Accum yvel_avg = 0;
            _Accum neighboring_boids = 0;
            _Accum close_dx = 0;
            _Accum close_dy = 0;
            
            int j;
            //loop through all other boids except for the one iterated on currently
            for (j = 0; j < boids; j++){ 
                if ( j != i ){
                    //Distance between boids
                    _Accum dx = x_pos[i] - x_pos[j];
                    _Accum dy = y_pos[i] - y_pos[j];
                    
                    //Check if in visual range, soft check without calculating distance to avoid multiplies 
                    if (abs(dx) < visual_range && abs(dy) < visual_range){
                        _Accum squared_distance = dx*dx + dy*dy;
                        
                        
                        //Check radius is within the protected range
                        if (squared_distance < protected_range*protected_range){
                            //Calculate how far away the boid is for x,y
                            close_dx += x_pos[i] - x_pos[j]; 
                            close_dy += y_pos[i] - y_pos[j];  
                        }
                    
                        //Check if distance is actually in visual range
                        else if (squared_distance < visual_range * visual_range){

                        //Add running sum of other boid's velocities and positions
                        xpos_avg += x_pos[j];
                        ypos_avg += y_pos[j];
                        xvel_avg += x_vel[j];
                        yvel_avg += y_vel[j];

                        //Increment tally of boids nearby (in visual range)
                        neighboring_boids += int2Accum(1);
                        }
                        
                    }
                }
            }
            //If there are nearby boids        
            if (neighboring_boids > 0){

               // Get the average of the positions of the nearby boids to get a center of mass
                xpos_avg = xpos_avg/neighboring_boids;
                ypos_avg = ypos_avg/neighboring_boids;
                xvel_avg = xvel_avg/neighboring_boids;
                yvel_avg = yvel_avg/neighboring_boids;

                // Add the centering/matching factors to velocity
                x_vel[i] = (x_vel[i] + 
                           (xpos_avg - x_pos[i])*centering_factor + 
                           (xvel_avg - x_vel[i])*matching_factor);

                y_vel[i] = (y_vel[i] + 
                           (ypos_avg - y_pos[i])*centering_factor + 
                           (yvel_avg - y_vel[i])*matching_factor);
              }
            // Add the avoidance factors to velocity
            x_vel[i] = x_vel[i] + (close_dx*avoid_factor);
            y_vel[i] = y_vel[i] + (close_dy*avoid_factor);
                    
            //If the boid is near an edge, make it turn by turnfactor
            if (y_pos[i] < top_margin){ //Top left corner is (0,0); at top edge, y_pos is min
                y_vel[i] = y_vel[i] + turn_factor;
            }

            if (x_pos[i] > tft_x - right_margin){ //At right edge, x_pos is max
                x_vel[i] = x_vel[i] - turn_factor;
            }         

            if (x_pos[i] < left_margin){ //at left edge, x_pos is min
                x_vel[i] = x_vel[i] + turn_factor;
            }

            if (y_pos[i] > tft_y - bottom_margin){ //At bottom edge, y_pos is max
                y_vel[i] = y_vel[i] - turn_factor;
            } 

            /* Calculate the boid's speed, using alpha max plus beta min algorithm 
               to approximate square root */
            _Accum speed;
            //speed = max + 0.4*min
            if(abs(x_vel[i]) > abs(y_vel[i])) {
                speed =  abs(x_vel[i]) + 0.4 * abs(y_vel[i]); 
            }
            else  {
                speed =  abs(y_vel[i]) + 0.4 * abs(x_vel[i]); 
            }

            //Enforce min and max speed limits
            speed = (1/(speed+ 0.01)); //save reciprocal value to only divide once instead of twice
            if (speed < min_speed) {
                x_vel[i] = (x_vel[i]*speed)*min_speed;
                y_vel[i] = (y_vel[i]*speed)*min_speed;
            }
            if (speed > max_speed){ 
                x_vel[i] = (x_vel[i]*speed)*max_speed;
                y_vel[i] = (y_vel[i]*speed)*max_speed;
            }
            
            //Update boid's position
            x_pos[i] = x_pos[i] + x_vel[i];
            y_pos[i] = y_pos[i] + y_vel[i];
            //  draw disk
            tft_drawPixel(Accum2int(x_pos[i]), Accum2int(y_pos[i]), ILI9340_WHITE); //x, y, radius, color
        
      }
        // NEVER exit while
        if(PT_GET_TIME() - begin_time > 32) mPORTASetBits(BIT_0); //light LED if failed timing of 30fps
        else mPORTAClearBits(BIT_0);
        PT_YIELD_TIME_msec(32 - (PT_GET_TIME() - begin_time)); //elapsed time in msec
      } // END WHILE(1)
  PT_END(pt);
} // animation thread

// === Animation Thread =============================================
// used to update the parameters for the boids algorithm based on slider values on the GUI
static PT_THREAD (protothread_sliders(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1){
        PT_YIELD_UNTIL(pt, new_slider==1);
        // clear flag
        new_slider = 0; 
        if (slider_id == 1){ // turn factor slider
            turn_factor = slider_value/100.0;
        }
        
        if (slider_id==2 ){ //protected range slider
            protected_range = slider_value;
        }
        if (slider_id==3 ){ //visual range slider
            visual_range = slider_value;
        }
        if (slider_id==4 ){ //centering factor slider 
            centering_factor = slider_value/10000.0;
        }
        if (slider_id==5 ){ //avoid factor slider
            avoid_factor = slider_value/100.0;
        }
        if (slider_id==6 ){ //matching factor slider
            matching_factor = slider_value/100.0;
        }
    }
    PT_END(pt);
}

// === Python serial thread ====================================================
// you should not need to change this thread UNLESS you add new control types
static PT_THREAD (protothread_serial(struct pt *pt))
{
    PT_BEGIN(pt);
    static char junk;
    while(1){
        // There is no YIELD in this loop because there are
        // YIELDS in the spawned threads that determine the 
        // execution rate while WAITING for machine input
        // =============================================
        // NOTE!! -- to use serial spawned functions
        // you MUST edit config_1_3_2 to
        // (1) uncomment the line -- #define use_uart_serial
        // (2) SET the baud rate to match the PC terminal
        // =============================================
        
        // now wait for machine input from python
        // Terminate on the usual <enter key>
        PT_terminate_char = '\r' ; 
        PT_terminate_count = 0 ; 
        PT_terminate_time = 0 ;

        // note that there will NO visual feedback using the following function
        PT_SPAWN(pt, &pt_input, PT_GetMachineBuffer(&pt_input) );
        
        // Parse the string from Python
        // There can be toggle switch, button, slider, and string events
        
        // slider
        if (PT_term_buffer[0]=='s'){
            sscanf(PT_term_buffer, "%c %d %f", &junk, &slider_id, &slider_value);
            new_slider = 1;
        }

        
        // string from python input line
        if (PT_term_buffer[0]=='$'){
            // signal parsing thread
            new_string = 1;
            // output to thread which parses the string
            // while striping off the '$'
            strcpy(receive_string, PT_term_buffer+1);
        }                                  
    } // END WHILE(1)   
    PT_END(pt);  
} // thread blink

// === Main  ======================================================

void main(void) {
  // === setup system wide interrupts  ========
  INTEnableSystemMultiVectoredInt();
  
  // === TFT setup ============================
  // init the display in main since more than one thread uses it.
  // NOTE that this init assumes SPI channel 1 connections
  tft_init_hw();
  tft_begin();
  tft_fillScreen(ILI9340_BLACK);
  //240x320 vertical display
  tft_setRotation(1); // Use tft_setRotation(1) for 320x240
  
  mPORTAClearBits(BIT_0 );	//Clear bits to ensure light is off.
  mPORTASetPinsDigitalOut(BIT_0);    //Set port as output
  
  _Accum random;
  //randomize boid arrays
  int i;
  srand(5); //seed for random number
  for (i = 0; i < boids; i++){ //initialize all boids with random position and velocities
      random = int2Accum(rand() % 320); //make random number is within x range of TFT
      x_pos[i] = random;
      // Ensure random value generated is within the left and right margins
      if (x_pos[i] < left_margin) {
          x_pos[i] = left_margin;
      }
      if (x_pos[i] > tft_x - right_margin) {
          x_pos[i] = tft_x - right_margin;
      }
          
      random = int2Accum(rand() % 240); //make random number within y range of TFT
      y_pos[i] = random;
      
      // Ensure random value generated is within the top and bottommargins
      if (y_pos[i] < top_margin) {
          y_pos[i] = top_margin;
      }
      if (y_pos[i] > tft_y - bottom_margin) {
          y_pos[i] = tft_y - bottom_margin;
      }
      
      //set random velocity, the Velocity algorithm in animation thread enforces speed limits
      x_vel[i] = random;
      y_vel[i] = random;
  }
  
  // === config threads ========================
  PT_setup();
  
  // === identify the threads to the scheduler =====
  // add the thread function pointers to be scheduled
  // --- Two parameters: function_name and rate. ---
  // rate=0 fastest, rate=1 half, rate=2 quarter, rate=3 eighth, rate=4 sixteenth,
  // rate=5 or greater DISABLE thread!
  
  pt_add(protothread_timer, 0);
  pt_add(protothread_serial, 0);
  pt_add(protothread_anim, 0);
  pt_add(protothread_sliders,0);
  
  // === initalize the scheduler ====================
  PT_INIT(&pt_sched) ;
  // >>> CHOOSE the scheduler method: <<<
  // (1)
  // SCHED_ROUND_ROBIN just cycles thru all defined threads
  //pt_sched_method = SCHED_ROUND_ROBIN ;
  
  // NOTE the controller must run in SCHED_ROUND_ROBIN mode
  // ALSO note that the scheduler is modified to cpy a char
  // from uart1 to uart2 for the controller
  
  pt_sched_method = SCHED_ROUND_ROBIN ;
  
  // === scheduler thread =======================
  // scheduler never exits
  PT_SCHEDULE(protothread_sched(&pt_sched));
  // ============================================
} // main
