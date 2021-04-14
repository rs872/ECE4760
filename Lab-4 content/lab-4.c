
/*
 * File:        Bird Chirp Synthesizer
 *             
 * Author:      Ragav Kumar, Rishi Singhal, and William Salcedo
 * For use with Sean Carroll's Big Board
 * http://people.ece.cornell.edu/land/courses/ece4760/PIC32/target_board.html
 * Target PIC:  PIC32MX250F128B
 * 
 * This template instantiates threads to communicate events from a Python
 * control interface. The python GUI supports push buttons, 
 * toggle switches (checkbox), Sliders and general text input/putput
 * 
 * Start the python script or this program in either order
 * (The python text is included as a comment at the end of this file)
 * Clicking on the LED button turns on the on-board LED
 * Clicking on the Clear LCD button clears the TFT
 * Clicking the Dot Color checkbox modifies a graphic red/green dot
 * The slider sets a cursor position on the TFT
 * The DDS checkbox turns DDS ON/OFF
 * Scrollig and clicking a Listbox entry will set the DDS waveform if DDS-ON
 * Typing anything in the Text input line causes the PIC to echo it into the receive window.
 *   Typing a command of the form "f 400" will result in a 400 Hz sinewave at the DACA output if DDS-ON
 *   Typing a command of the form "v 1.25" will set that voltage at the DACA output if DDS-OFF
 *   Typing a command of the form "h" will echo back the form of the other commands
 * Checking the reset_enable, then clicking RESET PIC does the expected IF the circuit is connected
 */
// =============================================
// NOTE!! -- to use serial spawned functions
// you MUST EDIT config_1_3_2 to
// (1) uncomment the line -- #define use_uart_serial
// (2) SET the baud rate to match the PC terminal
// =============================================
////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config_1_3_2.h"
// threading library
#include "pt_cornell_1_3_2_python.h"
#include <math.h>

////////////////////////////////////
// graphics libraries
// SPI channel 1 connections to TFT
#include "tft_master.h"
#include "tft_gfx.h"

// current string
char receive_string[64];

//used for Python GUI
char new_string = 0;
char new_slider = 0;

// current slider
int slider_id;
float slider_value ; // value could be large

int return_pos_pan = 60000;
int return_pos_tilt = 60000;

char new_button = 0;
char new_toggle = 0;

char record_pos = 1;

// curent button
char button_id, button_value ;
// current toggle switch/ check box
char toggle_id, toggle_value ;

#define SERVO_MIN_CYCLES 60000
// === Slider Thread =============================================
// 
static PT_THREAD (protothread_sliders(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1){
        PT_YIELD_UNTIL(pt, new_slider==1 && record_pos ==1);
        // clear flag
        new_slider = 0; 
        if (slider_id == 1){ // turn factor slider
            
            OC3RS = slider_value;
            return_pos_pan = slider_value;

        }
        else if (slider_id == 2){
            OC4RS = slider_value;
            return_pos_tilt = slider_value;
            
        }
    }
    PT_END(pt);
}

// === Buttons thread ==========================================================
// process buttons from Python for clear LCD and blink the on-board LED
static PT_THREAD (protothread_buttons(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1){
        PT_YIELD_UNTIL(pt, new_button==1);
        // clear flag
        new_button = 0;   
        // Button one -- control the LED on the big board
        if (button_id==1 && button_value==0){
            OC3RS = return_pos_pan;
            OC4RS = return_pos_tilt; 
        }

         
    } // END WHILE(1)   
    PT_END(pt);  
} // thread blink

// === Toggle thread ==========================================================
// process toggle from Python to change a dot color on the LCD
static PT_THREAD (protothread_toggles(struct pt *pt))
{
    PT_BEGIN(pt);
    //
    while(1){
        // this threaqd does a periodic redraw in case the dot is erased
        PT_YIELD_UNTIL(pt, new_toggle==1);
        //update dot color if toggle changed
        printf("hello Yo");
        if (new_toggle == 1){
            // clear toggle flag
            new_toggle = 0;   
            // Toggle one -- put a  green makrer on screen
            if (toggle_id==1 && toggle_value==1){
                record_pos = 1;
                
            }
            // toggle 0 -- put a red dot on the screen
            else if (toggle_id==1 && toggle_value==0){
                record_pos = 0;
            }
           
        } // end new toggle
    } // END WHILE(1)   
    PT_END(pt);  
} // thread toggles

//======================RANDOM WALKING THREAD=======================
static PT_THREAD (protothread_randomwalk(struct pt *pt))
{
    PT_BEGIN(pt);
    //
    while(1){
        // this threaqd does a periodic redraw in case the dot is erased
        PT_YIELD_TIME_msec(1000);
        //update dot color if toggle changed
        if (record_pos == 0){
            int tempReg = OC3RS + 100;
            printf("tempReg: %d",tempReg);
            if(tempReg < 60000) 
            {
                OC3RS = 60000;
            }
            else if(tempReg > 80000) 
            {
                OC3RS = 800000;
            }
            else 
            {
                OC3RS = tempReg;
            }
            tempReg = OC4RS + 100;
            printf("tempReg: %d",tempReg);
            if(tempReg < 60000) 
            {
                OC4RS = 60000;
            }
            else if(tempReg > 80000) 
            {
                OC4RS = 800000;
            }
            else 
            {
                OC4RS = tempReg;
            }
            
        } // end new toggle
    } // END WHILE(1)   
    PT_END(pt);  
} // thread random walk


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
        if (PT_term_buffer[0]=='t'){
            // signal the button thread
            new_toggle = 1;

            // subtracting '0' converts ascii to binary for 1 character
            toggle_id = (PT_term_buffer[1] - '0')*10 + (PT_term_buffer[2] - '0');
            toggle_value = PT_term_buffer[3] - '0';
        }
        
        // pushbutton
        if (PT_term_buffer[0]=='b'){
            // signal the button thread
            new_button = 1;
            // subtracting '0' converts ascii to binary for 1 character
            button_id = (PT_term_buffer[1] - '0')*10 + (PT_term_buffer[2] - '0');
            button_value = PT_term_buffer[3] - '0';
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
    
    OpenTimer23(T2_ON | T2_PS_1_1 | T2_32BIT_MODE_ON, 800000); //800K (cycles)
    
    OpenOC3(OC_ON | OC_TIMER_MODE32 | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE , SERVO_MIN_CYCLES, SERVO_MIN_CYCLES) ;
    OpenOC4(OC_ON | OC_TIMER_MODE32 | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE , SERVO_MIN_CYCLES, SERVO_MIN_CYCLES) ;
    
    PPSOutput(4, RPA3, OC3) ;  // configure OC3 to RPA3
    PPSOutput(3, RPA2, OC4) ;  // configure OC4 to RPA2
    
    SetChanADC10( ADC_CH0_NEG_SAMPLEA_NVREF | ADC_CH0_POS_SAMPLEA_AN11 ); // configure to sample AN11
    
    
    
    
    
//  // === set up DAC on big board ========
//  // timer interrupt //////////////////////////
//    // Set up timer2 on,  interrupts, internal clock, prescalar 1, toggle rate
//    #define TIMEOUT (40000000/Fs) // clock rate / sample rate
//    // 2000 is 20 ksamp/sec
//    printf("in main");
//    OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, TIMEOUT);
//
//    // set up the timer interrupt with a priority of 2
//    ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
    mT3ClearIntFlag(); // and clear the interrupt flag
//
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
//    

// 
//    // === setup system wide interrupts  ========
  INTEnableSystemMultiVectoredInt();

  // === config threads ========================
  PT_setup();
//  
//  // === identify the threads to the scheduler =====
//  // add the thread function pointers to be scheduled
//  // --- Two parameters: function_name and rate. ---
//  // rate=0 fastest, rate=1 half, rate=2 quarter, rate=3 eighth, rate=4 sixteenth,
//  // rate=5 or greater DISABLE thread!
//  
//  pt_add(protothread_buttons, 0);
  pt_add(protothread_serial, 0);
  pt_add(protothread_sliders, 0);
  pt_add(protothread_buttons, 0);
  pt_add(protothread_toggles, 0);
  pt_add(protothread_randomwalk, 0);
  
//  pt_add(protothread_python_string, 0);

//  
  // === initalize the scheduler ====================
  PT_INIT(&pt_sched) ;
  // >>> CHOOSE the scheduler method: <<<
  // (1)
  // SCHED_ROUND_ROBIN just cycles thru all defined threads  
  // NOTE the controller must run in SCHED_ROUND_ROBIN mode
  // ALSO note that the scheduler is modified to cpy a char
  // from uart1 to uart2 for the controller
  
  pt_sched_method = SCHED_ROUND_ROBIN ;
  
  // === scheduler thread =======================
  // scheduler never exits
  PT_SCHEDULE(protothread_sched(&pt_sched));
  // ============================================
  
} // main

