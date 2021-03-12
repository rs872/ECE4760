/*
 * File:        Python control prototype
 *             
 * Author:      Bruce Land
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

//== Timer 2 interrupt handler ===========================================
// direct digital synthesis of sine wave
#define two32 4294967296.0 // 2^32 
#define Fs 44000
#define WAIT {}
// DAC ISR
// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000
// B-channel, 1x, active
#define DAC_config_chan_B 0b1011000000000000
//
volatile int DAC_data ;// output value
volatile SpiChannel spiChn = SPI_CHANNEL2 ;	// the SPI channel to use
volatile int spiClkDiv = 2 ; // 20 MHz max speed for DAC!!
volatile unsigned int phase_accum_main = 0;//N (main accumulator)
volatile float phase_incr_mult = two32/Fs ;//multiplier for DDS accum increment
volatile float swoop_factor = (M_PI/5720);//factor used inside sin function
volatile unsigned int chirp_freq = 2000;//The constant factor of the integrator

volatile char record_array[50]; //Array to store states when record toggle is pressed
volatile unsigned char record_index = 0; //State variable indicating what position the last entry of the record_array is
volatile unsigned char playback_index = 0; //State variable indicating how many of the recorded sounds the play thread has played in conjunction with the DDS
volatile unsigned char playback_flag = 1; //Flag indicating that the device is in playback mode
volatile unsigned char sound_flag = 1; //Flag indicating that a sound is being played. 



// DDS sine table
#define sine_table_size 256
volatile int sin_table[sine_table_size] ;
// the dds state controlled by python interface
volatile int dds_state = 0;

volatile int isr_counter = 5720; //Counter that indicates how much of a sound has been played

volatile _Accum curr_amplitude = 0;

float swoop(int x);
float chirp(int x);

//5720 interrupts for 130 ms at 44kHz
void __ISR(_TIMER_2_VECTOR, ipl2) Timer2Handler(void)
{
    // you MUST clear the ISR flag
    mT2ClearIntFlag();
    //Sound selector
    //Changes the frequency function based on which sound is requested with flag dds_state
    //dds_state = 0 is swoop, dds_state = 1 is chirp, dds_state = 2 is silence.
    if (dds_state == 0 && isr_counter < 5720){ 
        phase_accum_main += (int)(phase_incr_mult * swoop(isr_counter));
    }
    
    else if(dds_state == 1 && isr_counter < 5720){ 
        chirp_freq += chirp(isr_counter);
        phase_accum_main += (int)(chirp_freq * phase_incr_mult);
    }

    //  DDS phase and sine table lookup
    //Isr counter- how far in time i am into this sound, used to see the phase increment
    DAC_data = sin_table[phase_accum_main>>24]; //take the 32 bit accum, and use the top 8 bits to index the sine table
 
    // === DAC Channel A =============
    // wait for possible port expander transactions to complete
    
    // CS low to start transaction
    
     mPORTBClearBits(BIT_4); // start transaction
     
     //Envelope
     //changes amplitude as a piecewise function of isr_counter.
     if (isr_counter< 1000) {
         curr_amplitude = (float) isr_counter * 0.001;
     }
     else if (isr_counter < 5720) {
         curr_amplitude = 1;
     }
     else if (isr_counter > 5720) {
         curr_amplitude = 1 - ((float) isr_counter - 4720) * 0.001;
     }
     else {
         curr_amplitude = 0;
     }
     
     DAC_data = (int) (DAC_data * curr_amplitude);
    
    //ADC Sends. Iterates the isrcounter and sends numerical data to the DAC according to dds state. 
    //Also changes the sound flag back to 1 to allow for playback threads to iterate and play the next sound.
    if (isr_counter < 5720) { //Increment ISR
        isr_counter++;
        if (dds_state < 2){
            WriteSPI2( DAC_config_chan_A | ((DAC_data + 2048) & 0xfff));//12 bit DAC- can take values from 0 to 2^12 which is 4096.
        //At last moment before sending to DAC- go to range 0 to 4096 since DAC expects unsigned values not -2048 to +2048
        }
        else{
            WriteSPI2( DAC_config_chan_A | (2048 & 0xfff));
        }
    }
    else{ //No sound playing
        WriteSPI2( DAC_config_chan_A | (2048 & 0xfff));
        chirp_freq = 2000; //Reset condition for chirp (integrator)
        sound_flag = 1;
    }
     
    
    while (SPI2STATbits.SPIBUSY) WAIT; // wait for end of transaction
     // CS high
    mPORTBSetBits(BIT_4) ; // end transaction
   //    
}

////////////////////////////////////
// === print a line on TFT =====================================================
// print string buffer
char tft_str_buffer[60];
// SEE 
// http://people.ece.cornell.edu/land/courses/ece4760/PIC32/index_TFT_display.html
// for details
void tft_printLine(int line_number, int indent, char* print_buffer, short text_color, short back_color, short char_size){
    // print_buffer is the string to print
    int v_pos, h_pos;
    char_size = (char_size>0)? char_size : 1 ;
    //
    v_pos = line_number * 8 * char_size ;
    h_pos = indent * 6 * char_size ;
    // erase the pixels
    //tft_fillRoundRect(0, v_pos, 239, 8, 1, back_color);// x,y,w,h,radius,color
    tft_setTextColor2(text_color, back_color); 
    tft_setCursor(h_pos, v_pos);
    tft_setTextSize(char_size);
    tft_writeString(print_buffer);
}

// === outputs from python handler =============================================
// signals from the python handler thread to other threads
// These will be used with the prototreads PT_YIELD_UNTIL(pt, condition);
// to act as semaphores to the processing threads
char new_string = 0;
char new_button = 0;
char new_toggle = 0;
char new_slider = 0;
char new_list = 0 ;
char new_radio = 0 ;
// identifiers and values of controls
// curent button
char button_id, button_value ;
// current toggle switch/ check box
char toggle_id, toggle_value ;
// current radio-group ID and button-member ID
char radio_group_id, radio_member_id ;
// current slider
int slider_id;
float slider_value ; // value could be large
// current listbox
int list_id, list_value ; 
// current string
char receive_string[64];

// === string input thread =====================================================
// process text from python
static PT_THREAD (protothread_python_string(struct pt *pt))
{
    PT_BEGIN(pt);
    static int dds_freq;
    // 
    while(1){
        // wait for a new string from Python
        PT_YIELD_UNTIL(pt, new_string==1);
        new_string = 0;
        // parse frequency command
        if (receive_string[0] == 'f'){
            // dds frequency
            sscanf(receive_string+1, "%d", &dds_freq);
            //phase_incr_main = (float)dds_freq * two32/Fs ;
            printf("freq=%d\r", dds_freq);
        }
        //

        //
        else if (receive_string[0] == 'h'){
            // dds frequency
            printf("f number ...sets DDS freq integer range 0-10000\r");
            // DAC amplitude
            printf("v float ...sets DAC volt, if DDS is off range 0.0-2.0\r");
            // help
            printf("help ...list the avaliable commands\r");
            // default string
            printf("Any other string is just echoed back\r");
        }
        //
        else {
            tft_printLine(1,0, receive_string, ILI9340_GREEN, ILI9340_BLACK, 2);
            printf("received>%s", receive_string);        
        }
    } // END WHILE(1)   
    PT_END(pt);  
} // thread python_string

// === Buttons thread ==========================================================
// process buttons from Python for clear LCD and blink the on-board LED
static PT_THREAD (protothread_buttons(struct pt *pt))
{
    PT_BEGIN(pt);
    // set up LED port A0 to blink
    mPORTAClearBits(BIT_0 );	//Clear bits to ensure light is off.
    mPORTASetPinsDigitalOut(BIT_0);    //Set port as output
    while(1){
        PT_YIELD_UNTIL(pt, new_button==1);
        // clear flag
        new_button = 0;   
        // Button one -- control the LED on the big board
        if (button_id==1 && button_value==1) {
//            mPORTASetBits(BIT_0); 
            isr_counter = 0;
            dds_state = 0; //swoop
            if (toggle_value == 1 && record_index < 49){
                record_array[record_index] = dds_state;
                record_index++;
            }
        }
        // Button 2 -- clear TFT
        if (button_id==2 && button_value==1) {
            isr_counter = 0;
            dds_state = 1; //chirp
            if (toggle_value == 1 && record_index < 49){
                record_array[record_index] = dds_state;
                record_index++;
            }
//            tft_fillScreen(ILI9340_BLACK);
        }
        if (button_id == 3 && button_value == 1) {
            isr_counter = 0;
            dds_state = 2;
            if (toggle_value == 1 && record_index < 49){
                record_array[record_index] = dds_state;
                record_index++;
            }
        }
    } // END WHILE(1)   
    PT_END(pt);  
} // thread blink

// === Toggle thread ==========================================================
static PT_THREAD (protothread_toggles(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1){
        // this threaqd does a periodic redraw in case the dot is erased
        PT_YIELD_TIME_msec(100)
        //update dot color if toggle changed
        if (new_toggle == 1){
            // clear toggle flag
            new_toggle = 0;   
            if (toggle_id==1 && toggle_value==1){
                playback_flag = 0;
                playback_index = 0;
                record_index = 0;
            }
            else if (toggle_id==1 && toggle_value==0){
                playback_flag = 1;
            }
           
        } // end new toggle

    } // END WHILE(1)   
    PT_END(pt);  
} // thread toggles

// === Playback thread ==========================================================
static PT_THREAD (protothread_playback(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1){
        //Yields until the device is in playback mode and the playback index is != the record index.
        //This implies that the playback index is less than the record index and allows the thread to 
        //Iterate through the list and playback the respective sounds corresponding to the integer stored
        //in the list of chars.
        PT_YIELD_UNTIL(pt, playback_flag == 1 && playback_index != record_index);
        
        if (sound_flag == 1){//Statement verifies no sound is being played
            dds_state = record_array[playback_index]; //Change played sound
            isr_counter = 0; //Start sound
            playback_index++; //iterate playback_index
            sound_flag = 0; //signal sound is being played
            printf("%d\n", dds_state ); //Print dds_state to serial terminal
       
        }     
    } // END WHILE(1)   
    PT_END(pt);  
} // thread toggles

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
        
        // toggle switch
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
        
        // slider
        if (PT_term_buffer[0]=='s'){
            sscanf(PT_term_buffer, "%c %d %f", &junk, &slider_id, &slider_value);
            new_slider = 1;
        }
        
        // listbox
        if (PT_term_buffer[0]=='l'){
            new_list = 1;
            list_id = PT_term_buffer[2] - '0' ;
            list_value = PT_term_buffer[3] - '0';
            //printf("%d %d", list_id, list_value);
        }
        
        // radio group
        if (PT_term_buffer[0]=='r'){
            new_radio = 1;
            radio_group_id = PT_term_buffer[2] - '0' ;
            radio_member_id = PT_term_buffer[3] - '0';
            //printf("%d %d", radio_group_id, radio_member_id);
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
    
  // === set up DAC on big board ========
  // timer interrupt //////////////////////////
    // Set up timer2 on,  interrupts, internal clock, prescalar 1, toggle rate
    #define TIMEOUT (40000000/Fs) // clock rate / sample rate
    // 2000 is 20 ksamp/sec
    printf("in main");
    OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, TIMEOUT);

    // set up the timer interrupt with a priority of 2
    ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
    mT2ClearIntFlag(); // and clear the interrupt flag

    // control CS for DAC
    mPORTBSetPinsDigitalOut(BIT_4);
    mPORTBSetBits(BIT_4);
    // SCK2 is pin 26 
    // SDO2 (MOSI) is in PPS output group 2, could be connected to RB5 which is pin 14
    PPSOutput(2, RPB5, SDO2);
    // 16 bit transfer CKP=1 CKE=1
    // possibles SPI_OPEN_CKP_HIGH;   SPI_OPEN_SMP_END;  SPI_OPEN_CKE_REV
    // For any given peripherial, you will need to match these
    SpiChnOpen(SPI_CHANNEL2, SPI_OPEN_ON | SPI_OPEN_MODE16 | SPI_OPEN_MSTEN | SPI_OPEN_CKE_REV , 2);
  // === end DAC setup =========
    
  // === build the sine lookup table =======
   // scaled to produce values between 0 and 4096
   int ii;
   for (ii = 0; ii < sine_table_size; ii++){
         sin_table[ii] = (int)(2047*sin((float)ii*6.283/(float)sine_table_size)); //sine table
    }
 
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
  
  // === config threads ========================
  PT_setup();
  
  // === identify the threads to the scheduler =====
  // add the thread function pointers to be scheduled
  // --- Two parameters: function_name and rate. ---
  // rate=0 fastest, rate=1 half, rate=2 quarter, rate=3 eighth, rate=4 sixteenth,
  // rate=5 or greater DISABLE thread!
  
  pt_add(protothread_buttons, 0);
  pt_add(protothread_serial, 0);
  pt_add(protothread_python_string, 0);
  pt_add(protothread_toggles, 0);
  pt_add(protothread_playback, 0);
  
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


inline float swoop(int x){
    return 260 * sin(swoop_factor*x) + 1740;
}

inline float chirp(int x){
    return 3.06E-4 * x;
}
// === end  ======================================================

