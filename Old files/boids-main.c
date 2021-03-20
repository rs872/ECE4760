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

char new_string = 0;
char new_slider = 0;

// current slider
int slider_id;
float slider_value ; // value could be large


// current string
char receive_string[64];

//Global Accums
_Accum turn_factor = 0.2;
_Accum visual_range = 20;
_Accum protected_range = 2;
_Accum centering_factor = 0.0005;
_Accum avoid_factor = 0.05;
_Accum matching_factor = 0.05;

//pixels/frame
_Accum max_speed = 3;
_Accum min_speed = 2;

//pixels
#define top_margin 50
#define bottom_margin 50
#define left_margin 50
#define right_margin 50

// of boids 
# define boids 1

//boid arrays
_Accum x_pos[boids];
_Accum y_pos[boids];
_Accum x_vel[boids];
_Accum y_vel[boids];

#define tft_x 320
#define tft_y 240

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

     tft_setCursor(0, 0);
     tft_setTextColor(ILI9340_WHITE);  tft_setTextSize(1);
     tft_writeString("Time in seconds since boot\n");
      while(1) {
        // yield time 1 second
        PT_YIELD_TIME_msec(1000) ;
        sys_time_seconds++ ;
        
        // draw sys_time
        tft_fillRoundRect(0,10, 100, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 10);
        tft_setTextColor(ILI9340_YELLOW); tft_setTextSize(2);
        sprintf(buffer,"%d", sys_time_seconds);
        tft_writeString(buffer);
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
        PT_YIELD_TIME_msec(32);
        
        int i;
        for (i = 0; i < boids; i++){
            // erase disk
            tft_fillCircle(Accum2int(x_pos[i]), Accum2int(y_pos[i]), 4, ILI9340_BLACK); //x, y, radius, color
            _Accum xpos_avg = 0;
            _Accum ypos_avg = 0;
            _Accum xvel_avg = 0;
            _Accum yvel_avg = 0;
            _Accum neighboring_boids = 0;
            _Accum close_dx = 0;
            _Accum close_dy = 0;
            
            int j;
            for (j = 0; j < boids; j++){
                if ( j != i ){
                    
                    //Distance between boids
                    _Accum dx = x_pos[i] - x_pos[j];
                    _Accum dy = y_pos[i] - y_pos[j];
                    
                    //Is it in visual range
                    if (abs(dx) < visual_range && abs(dy) < visual_range){
                        _Accum squared_distance = dx*dx + dy*dy;
                        
                        
                        //Is squared distance less than the protected range?
                        if (squared_distance < protected_range*protected_range){
                            //If so, calculate difference in x/y-coordinates to nearfield boid
                            close_dx += x_pos[i] - x_pos[j]; 
                            close_dy += y_pos[i] - y_pos[j];  
                        }
                    
                        //If not in protected range, is the boid in the visual range?
                        else if (squared_distance < visual_range * visual_range){

                        //Add other boid's x/y-coord and x/y vel to accumulator variables
                        xpos_avg += x_pos[j];
                        ypos_avg += y_pos[j];
                        xvel_avg += x_vel[j];
                        yvel_avg += y_vel[j];

                        //Increment number of boids within visual range
                        neighboring_boids += 1;
                        }
                        

                    //    # If there were any boids in the visual range . . .            
                    //    if (neighboring_boids > 0): 
                    //
                    //        # Divide accumulator variables by number of boids in visual range
                    //        xpos_avg = xpos_avg/neighboring_boids 
                    //        ypos_avg = ypos_avg/neighboring_boids
                    //        xvel_avg = xvel_avg/neighboring_boids
                    //        yvel_avg = yvel_avg/neighboring_boids
                    //
                    //        # Add the centering/matching contributions to velocity
                    //        boid.vx = (boid.vx + 
                    //                   (xpos_avg - boid.x)*centering_factor + 
                    //                   (xvel_avg - boid.vx)*matching_factor)
                    //
                    //        boid.vy = (boid.vy + 
                    //                   (ypos_avg - boid.y)*centering_factor + 
                    //                   (yvel_avg - boid.vy)*matching_factor)
                    //
                    //    # Add the avoidance contribution to velocity
                    //    boid.vx = boid.vx + (close_dx*avoidfactor)
                    //    boid.vy = boid.vy + (close_dy*avoidfactor)
                    }
                }
            }
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


            // Calculate the boid's speed
            // Slow step! Lookup the "alpha max plus beta min" algorithm

            _Accum speed = sqrt(x_vel[i]*x_vel[i] + y_vel[i]*y_vel[i]);

            //Enforce min and max speeds
            if (speed < min_speed) {
                x_vel[i] = (x_vel[i]/speed)*min_speed;
                y_vel[i] = (y_vel[i]/speed)*min_speed;
            }
            if (speed > max_speed){ 
                x_vel[i] = (x_vel[i]/speed)*max_speed;
                y_vel[i] = (y_vel[i]/speed)*max_speed;
            }

            //Update boid's position
            x_pos[i] = x_pos[i] + x_vel[i];
            y_pos[i] = y_pos[i] + y_vel[i];
        
        
        //  draw disk
        tft_fillCircle(Accum2int(x_pos[i]), Accum2int(y_pos[i]), 4, ILI9340_WHITE); //x, y, radius, color
      }
        // NEVER exit while
         
      } // END WHILE(1)
  PT_END(pt);
} // animation thread

// === Python serial thread ====================================================
// you should not need to change this thread UNLESS you add new control types
static PT_THREAD (protothread_serial(struct pt *pt))
{
    PT_BEGIN(pt);
    static char junk;
    //   
    //
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
   
//    // control CS for DAC
//    mPORTBSetPinsDigitalOut(BIT_4);
//    mPORTBSetBits(BIT_4);
//    // SCK2 is pin 26 
//    // SDO2 (MOSI) is in PPS output group 2, could be connected to RB5 which is pin 14
//    PPSOutput(2, RPB5, SDO2);
//    // 16 bit transfer CKP=1 CKE=1
//    // possibles SPI_OPEN_CKP_HIGH;   SPI_OPEN_SMP_END;  SPI_OPEN_CKE_REV
//    // For any given peripherial, you will need to match these
//    SpiChnOpen(SPI_CHANNEL2, SPI_OPEN_ON | SPI_OPEN_MODE16 | SPI_OPEN_MSTEN | SPI_OPEN_CKE_REV , 2);
//  // === end DAC setup =========
    
  
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
  
  _Accum random;
  //randomize boid arrays
  int i;
  srand(5);
  for (i = 0; i < boids; i++){
      random = int2Accum(rand() % 320);
      
    
      x_pos[i] = random;
      printf("This is xpos[0] %d\n", Accum2int(x_pos[0]));
      random = int2Accum(rand() % 240);
      
      y_pos[i] = random;
      printf("This is ypos[0] %d\n", Accum2int(y_pos[0]));
      
      //Velocity algo checks for max speed
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







