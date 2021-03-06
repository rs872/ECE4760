/*********************************************************************
 *
 *  FM synth to SPI to  MCP4822 dual channel 12-bit DAC
 *
 *********************************************************************
 * Bruce Land Cornell University
 * Sept 2018
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config_1_3_2.h"
// threading library
#include "pt_cornell_1_3_2_python.h"
// for sine
#include <math.h>

////////////////////////////////////
// graphics libraries
// SPI channel 1 connections to TFT
#include "tft_master.h"
#include "tft_gfx.h"
// need for rand function
#include <stdlib.h>
// need for sin function
#include <math.h>
#include "Notes.h"
#include "note_markov.h"
#include "duration_markov.h"
#include "octave_markov.h"
#include "markov_seeds.h"
////////////////////////////////////

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_cmd, pt_tick;
// uart control threads
static struct pt pt_input, pt_output, pt_DMA_output ;

// system 1 second interval tick
int sys_time_seconds ;

// === GCC s16.15 format ===============================================
#define float2Accum(a) ((_Accum)(a))
#define Accum2float(a) ((float)(a))
#define int2Accum(a) ((_Accum)(a))
#define Accum2int(a) ((int)(a))
// the native type is _Accum but that is ugly
typedef _Accum fixAccum  ;
#define onefixAccum int2Accum(1)
//#define sustain_constant float2Accum(256.0/20000.0) ; // seconds per decay update

// === GCC 0.16 format ===============================================
#define float2Fract(a) ((_Fract)(a))
#define Fract2float(a) ((float)(a))

// the native type is _Accum but that is ugly
typedef _Fract fixFract  ;
fixFract onefixFract = float2Fract(0.9999);
// ???
///#define sustain_constant float2Accum(256.0/20000.0) ; // seconds per decay update

/* ====== MCP4822 control word =========================================
bit 15 A/B: DACA or DACB Selection bit
1 = Write to DACB
0 = Write to DACA
bit 14 ? Don?t Care
bit 13 GA: Output Gain Selection bit
1 = 1x (VOUT = VREF * D/4096)
0 = 2x (VOUT = 2 * VREF * D/4096), where internal VREF = 2.048V.
bit 12 SHDN: Output Shutdown Control bit
1 = Active mode operation. VOUT is available. ?
0 = Shutdown the selected DAC channel. Analog output is not available at the channel that was shut down.
VOUT pin is connected to 500 k???typical)?
bit 11-0 D11:D0: DAC Input Data bits. Bit x is ignored.
*/
// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000
#define DAC_config_chan_B 0b1011000000000000
#define Fs 20000.0
#define two32 4294967296.0 // 2^32 

//== Timer 2 interrupt handler ===========================================
// actual scaled DAC 
volatile unsigned int DAC_data;
// SPI
volatile SpiChannel spiChn = SPI_CHANNEL2 ;	// the SPI channel to use
volatile int spiClkDiv = 2 ; // 20 MHz max speed for this DAC

// the DDS units: 1=FM, 2=main frequency
volatile unsigned int phase_accum_fm, phase_incr_fm ;// 
volatile unsigned int phase_accum_main, phase_incr_main ;//
// DDS sine table
#define sine_table_size 256
volatile fixAccum sine_table[sine_table_size];
// envelope: FM and main frequency
volatile fixAccum env_fm, wave_fm, dk_state_fm, attack_state_fm;
volatile fixAccum env_main, wave_main,  dk_state_main, attack_state_main;



volatile unsigned int phase_accum_fma, phase_incr_fma ;// 
volatile unsigned int phase_accum_maina, phase_incr_maina ;//

volatile fixAccum env_fma, wave_fma, dk_state_fma, attack_state_fma;
volatile fixAccum env_maina, wave_maina,  dk_state_maina, attack_state_maina;


// define the envelopes and tonal quality of the instruments
#define n_synth 8 
// 0 plucked string-like 
// 1 slow rise 
// 2 string-like lower pitch
// 3 low drum
// 4 medium drum
// 5 snare
// 6 chime 
// 7 low, harsh string
//nc - below are various parameters that are used to generate instrument like sounds. They are used in
//in the ISR where sound is produced via the ADC. 
//                                          0     1      2      3      4      5      6      7
volatile fixAccum  attack_main[n_synth] = {0.001, 0.9,  0.001, 0.001, 0.001, 0.001, 0.001, .005};
volatile fixAccum   decay_main[n_synth] = {0.98,  0.97, 0.98,  0.98,  0.98,  0.80,  0.98, 0.98};
//
volatile fixAccum     depth_fm[n_synth] = {2.00,  2.5,  2.0,   3.0,   1.5,   10.0,  1.0,  2.0};
volatile fixAccum    attack_fm[n_synth] = {0.001, 0.9,  0.001, 0.001, 0.001, 0.001, 0.001, 0.005};
volatile fixAccum     decay_fm[n_synth] = {0.80,  0.8,  0.80,  0.90,  0.90,  0.80,  0.98,  0.98};
//nc - Generating sound is same as in lab 1, however this time we modulate the main frequency (freq_main)
//with another frequency (freq_fm). This is known as frequency modulation.
//                          0    1    2    3     4    5     6     7
float freq_main[n_synth] = {1.0, 1.0, 0.5, 0.25, 0.5, 1.00, 1.0,  0.25};
float   freq_fm[n_synth] = {3.0, 1.1, 1.5, 0.4,  0.8, 1.00, 1.34, 0.37};
// the current setting for instrument (index into above arrays)
//nc - The current instrument is set to a plucked string.
int current_v1_synth=0, current_v2_synth=0 ;

//20 counts / 1 ms * 1000 msec / 1sec * 60 sec /1 min * min/beats= ( 1.2*10^6)/bpm
#define BPM_SCALER 1200000
#define BPM 120

//nc - When indexing note array - subtract from MIDI number of the first note of the array! 
//For ex if C4 is first then subtract by 60. This is done, so that the code can index into the note
//array just on the basis of a midi number.

//nc - tempo is how long the note lasts. The ADC fires at the same rate always, but temp decides how many 
//times the ADC produces sounds.(250ms is 5000 ISR fires. Everytime the iSR fires, it produces sound.)
// note transition rate in ticks of the ISR
// rate is 20/mSec (timer overflow rate for the ISR). So 250 mS is 5000 counts
//20 counts (ISR fires) per ms
// 8/sec, 4/sec, 2/sec, 1/sec (4th of sec = 250ms = 5000 counts)

//1 in the sub_divs is a quarter-note
//This array is analagous to note duration
//16th, 8th, dotted_8th, quarter, dotted_quarter, half, dotted half, whole
#define note_dimension 12
_Accum sub_divs[8] = {0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4};
int tempo_v1_flag, tempo_v2_flag;
int current_v1_tempo=1, current_v2_tempo=2;
int tempo_v1_count, tempo_v2_count;
unsigned int cycles_per_beat = BPM_SCALER/BPM;


//MARKOV CHAIN - DURATION
//matrix of probabilities, each row needs to sum up to 1
//can store markov_duration in RAM since (8^4 = 4096 bytes) and RAM is 32K bytes

//Flash memory is 128K bytes but approximating ~28K bytes for the code, we have ~100K bytes for the 
//note markov chain. Hence, we could have 17^4 = 83.5K bytes
#define MARKOVDIM 12 


//seeds for the markov chains
volatile int prev_prev_note = 11;//rightmost
volatile int prev_note = 11; //center
volatile int curr_note = 3;//leftmost
volatile int next_note = 0;

volatile int prev_prev_note_duration = 0; //rightmost
volatile int prev_note_duration = 0; //middle of tuple
volatile int curr_note_duration = 7; //leftmost of tuple
volatile int next_note_duration = 1; // next note predicted by algo

volatile int prev_octave = 0; 
volatile int curr_octave = 0; 
volatile int next_octave =0; 


//for additive synthesis (chords)
volatile int curr_notea = 3;
volatile int next_notea = 0;

volatile int curr_octavea = 2;
volatile int next_octavea =0;

volatile char seed = 0;

char markov_trigger = 1; //trigger markov thread
// beat/rest patterns
#define n_beats 11
int beat[n_beats] = {
		0b0101010101010101, // 1-on 1-off phase 2
		0b1111111011111110, // 7-on 1-off
		0b1110111011101110, // 3-on 1-off
		0b1100110011001100, // 2-on 2-off phase 1
		0b1010101010101010, // 1-on 1-off phase 1
		0b1111000011110000, // 4-on 4-off phase 1
		0b1100000011000000, // 2-on 6-off 
		0b0011001100110011, // 2-on 2-off phase 2
		0b1110110011101100, // 3-on 1-off 2-on 2-off 3-on 1-off 2-on 2-off 
		0b0000111100001111, // 4-on 4-off phase 2
		0b1111111111111111  // on
	} ;
// max-beats <= 16 the length of the beat vector in bits
#define max_beats 16
int current_v1_beat=1, current_v2_beat=2;
int beat_v1_count, beat_v2_count ;
//
// random number
int rand_raw ;

// time scaling for decay calculation
volatile int dk_interval; // wait some samples between decay calcs
// play flag
//When this flag is a 1, the ADC plays the note. If the user inputs cs, then the cmd thread
//will flip this flag to a 1
volatile int play_trigger;

volatile fixAccum sustain_state, sustain_interval=0;
// profiling of ISR
volatile int isr_time, isr_start_time, isr_count=0;
volatile int flag_attack_done = 0;
volatile int cycles_attack = 0;

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
char play_music = 0;
char note_count = 0; // max 255
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
//=============================
void __ISR(_TIMER_2_VECTOR, ipl2) Timer2Handler(void)
{
    // time to get into ISR
    isr_start_time = ReadTimer2(); 
    tempo_v1_count++ ;
    // update flag if duration specified was hit 
    if (tempo_v1_count>=cycles_per_beat*sub_divs[curr_note_duration]) {
        tempo_v1_flag = 1;
        tempo_v1_count = 0;
    }
    
    mT2ClearIntFlag();
    
    //Everytime the timer interrupts, the phase_accum variable for both the main_freq and freq_mod
    //vector increments. This is irrespective of sound being played or not.
    // FM phase
    phase_accum_fm += phase_incr_fm ; 
    // main phase
    phase_accum_main += phase_incr_main + (Accum2int(sine_table[phase_accum_fm>>24] * env_fm)<<16) ;
    
    phase_accum_fma += phase_incr_fma ; 
    // main phase
    phase_accum_maina += phase_incr_maina + (Accum2int(sine_table[phase_accum_fma>>24] * env_fma)<<16) ;
    
     
     // init the exponential decays
     // by adding energy to the exponential filters 
    if (play_trigger) {
        //nc - current_v1_synth contains the index of the instrument to be played.
        //hence all appropriate variables required to produce that instrument's sound are
        //extracted below. 
        dk_state_fm = depth_fm[current_v1_synth]; 
        dk_state_main = onefixAccum; 
        
        dk_state_fma = depth_fm[current_v1_synth]; 
        dk_state_maina = onefixAccum; 
        
        attack_state_fm = depth_fm[current_v1_synth]; 
        attack_state_main = onefixAccum; 
        
        attack_state_fma = depth_fm[current_v1_synth]; 
        attack_state_maina = onefixAccum; 
                
        play_trigger = 0; 
        phase_accum_fm = 0;
        phase_accum_main = 0;
        
        phase_accum_fma = 0;
        phase_accum_maina = 0;
        
        dk_interval = 0;
        sustain_state = 0;
        flag_attack_done = 0;
    }
    
    //Bread and butter of the algorithm
    //To note, the following code block runs, even if play_trigger is not set.
    //Main frequency and modulating frequency are BOTH present
    //pure tone with the modulator decaying
    //in guitar string the second harmonic decays faster than the fundamental, and third even faster
    // envelope calculations are 256 times slower than sample rate
    // computes 4 exponential decays and builds the product envelopes
    if ((dk_interval++ & 0xff) == 0){
            // approximate the first order FM decay  ODE
            dk_state_fm = dk_state_fm * decay_fm[current_v1_synth] ;
            //  approximate the first order main waveform decay  ODE
            dk_state_main = dk_state_main * decay_main[current_v1_synth] ;
            
            dk_state_fma = dk_state_fma * decay_fm[current_v1_synth] ;
            //  approximate the first order main waveform decay  ODE
            dk_state_maina = dk_state_maina * decay_main[current_v1_synth] ;


        //}
        // approximate the ODE for the exponential rise FM/main waveform
        attack_state_fm = attack_state_fm * attack_fm[current_v1_synth];
        attack_state_main = attack_state_main * attack_main[current_v1_synth];
        
        attack_state_fma = attack_state_fma * attack_fm[current_v1_synth];
        attack_state_maina = attack_state_maina * attack_main[current_v1_synth];

        // product of rise and fall is the FM envelope
        // fm_depth is the current value of the function
        env_fm = (depth_fm[current_v1_synth] - attack_state_fm) * dk_state_fm ;
        
        env_fma = (depth_fm[current_v1_synth] - attack_state_fma) * dk_state_fma ;
        // product of rise and fall is the main envelope
        env_main = (onefixAccum - attack_state_main) * dk_state_main ;
        
        env_maina = (onefixAccum - attack_state_maina) * dk_state_maina ;
        
    }
    //send a value to the DAC irrespective of play trigger being set or not.
    // === Channel A =============
    // CS low to start transaction
     mPORTBClearBits(BIT_4); // start transaction
     // write to spi2
     WriteSPI2(DAC_data); 
    //update sinewave values to be sent to the DAC    
    wave_main = sine_table[phase_accum_main>>24] * env_main;
    
    //second sinewave->to layer as a chord
    wave_maina = sine_table[phase_accum_maina>>24] * env_maina;
    
    // truncate to 12 bits, read table, convert to int and add offset
    DAC_data = DAC_config_chan_A | ((int)(Accum2int(wave_main)*0.33) + (int)(Accum2int(wave_maina)*0.33) + 2048) ; //change to 0.25 when 4 notes
    // test for done
    while (SPI2STATbits.SPIBUSY); // wait for end of transaction
     // CS high
     mPORTBSetBits(BIT_4); // end transaction
     
     // time to get into ISR is the same as the time to get out so add it again
     isr_time = max(isr_time, ReadTimer2()+isr_start_time) ; // - isr_time;
} // end ISR TIMER2

// === Serial Thread ======================================================
// revised commands:
// cs note (00-99)  -- change seed value (single digit numbers must start with 0, ex. 08)
// t -- print current instrument parameters
// 

//nc - This is the main control thread of the program
static PT_THREAD (protothread_cmd(struct pt *pt))
{
    // The serial interface
    static int value = 0;
    static float f_fm, f_main;
    PT_BEGIN(pt);
    // clear port used by thread 4
    mPORTBClearBits(BIT_0);
    while(1) {
            rand_raw = rand();
            //Yield until a new string is received. 
            PT_YIELD_UNTIL(pt, new_string==1);
            new_string = 0;
            //nc - value is note index in our own note array in the header
            value = (int) (receive_string[2] - '0') * 10 + (receive_string[3] - '0');
            switch(receive_string[0]){
                 case 'c': // value is seed index
                     if (receive_string[1]=='t') {
                     // value is tempo index 0-3
                        current_v1_tempo = (int)(value);
                     }
                     //change the seed of the markov chain 
                     if (receive_string[1]=='s') { //seed
                         seed = (int)(value);
                         prev_prev_note = note_seeds[seed][2];//rightmost   Change index of first thing to seed variable
                         prev_note = note_seeds[seed][1]; //center
                         curr_note = note_seeds[seed][0];//leftmost
                    }    
                     break;
             }
            // never exit while
      } // END WHILE(1)
  PT_END(pt);
} // thread 3


// === Thread 5 ======================================================
//Markov thread
static PT_THREAD (protothread_markov(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1) {
            // yield until triggered by music thread
            PT_YIELD_UNTIL(pt, markov_trigger == 1) ;
            markov_trigger = 0;
            unsigned long int random_num = rand() % 255; //max 255 random number
            unsigned long int cum_prob = 0;

            //duration markov thread-set cumulative probabilities
            int i;
            for (i=0; i<8; i++){
                cum_prob += (unsigned long int)
                            markov_duration[curr_note][curr_note_duration][prev_note_duration][i];
                if (cum_prob >= random_num){ //next note duration based on the largest number lower than the random number
                    next_note_duration = i;
                    break;
                }
            }
            //reset cumulative probability
            cum_prob = 0;
            
            //note markov chain
            random_num = rand() % 255; //255 max number
            for (i=0; i<13; i++){
                cum_prob += (unsigned long int)
                markov_notes[curr_note][prev_note][prev_prev_note][i];
                if(cum_prob >= random_num){
                    next_note = i;
                    break;
                }
            }
            
            cum_prob = 0;
            
            //note markov chain - second note in the chord
            random_num = rand() % 255; //255 max
            for (i=0; i<13; i++){
                cum_prob += (unsigned long int)
                            markov_notes[curr_note][prev_note][prev_prev_note][i];
                if(cum_prob >= random_num){
                    next_notea = i;
                    break;
                }
            }
                     
            cum_prob = 0;

            //octave markov chain
            random_num = rand() % 255; //255 max
            for (i=0; i<4; i++){
                cum_prob += (unsigned long int)
                            markov_octave[curr_note][prev_note][curr_octave][prev_octave][i];
                if(cum_prob >= random_num){
                    next_octave = i;
                    break;
                }
            }
            
            cum_prob = 0;

            //octave markov chain, second note in the chord
            random_num = rand() % 255; //255 max
            for (i=0; i<4; i++){
                cum_prob += (unsigned long int)
                            markov_octave[curr_note][prev_note][curr_octave][prev_octave][i];
                if(cum_prob >= random_num){
                    next_octavea = i;
                    break;
                }
            }
            
            // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
} // thread 4

// ===  radio thread =========================================================
// process listbox from Python to set instrument to be played
static PT_THREAD (protothread_radio(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1){
        PT_YIELD_UNTIL(pt, new_radio==1);
        // clear flag
        new_radio = 0; 
        if (radio_group_id == 1){
            if (radio_member_id == 1){
                current_v1_synth = (int)(0);
            }
            if (radio_member_id == 2){
                current_v1_synth = (int)(1);
            }
            if (radio_member_id == 3){
                current_v1_synth = (int)(2);
            }
//skip instrument 4 and 6
            if (radio_member_id == 5){
                current_v1_synth = (int)(4);
            }
            if (radio_member_id == 7){
                current_v1_synth = (int)(6);
            }
            if (radio_member_id == 8){
                current_v1_synth = (int)(7);
            }
        }
    } // END WHILE(1)   
    PT_END(pt);  
} // thread radio


// === Music thread ==========================================================
// process buttons from Python to play music
static PT_THREAD (protothread_music(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1){
        PT_YIELD_UNTIL(pt,tempo_v1_flag==1); //yield until current duration has been met
        if (play_music && note_count < 100){ //100 note song
            
            //update values one step further into markov chain
            prev_prev_note_duration = prev_note_duration;
            prev_note_duration = curr_note_duration;
            curr_note_duration = next_note_duration;
            
            prev_prev_note =  prev_note;
            prev_note = curr_note;
            curr_note = next_note;             
            
            prev_octave =  curr_octave;
            curr_octave = next_octave;
            
            curr_notea = next_notea;
            curr_octavea = next_octavea;
            
            //generate phase increments based on notes and octaves found
            phase_incr_fm = freq_fm[current_v1_synth]*notesDEF[curr_note + 12 * curr_octave]*(float)two32/Fs; 
            phase_incr_main = freq_main[current_v1_synth]*notesDEF[curr_note + 12 * curr_octave]*(float)two32/Fs;
            
            phase_incr_fma = freq_fm[current_v1_synth]*notesDEF[curr_notea + 12 * curr_octavea]*(float)two32/Fs; 
            phase_incr_maina = freq_main[current_v1_synth]*notesDEF[curr_notea + 12 * curr_octavea]*(float)two32/Fs; 
            
            markov_trigger = 1;//trigger markov thread
            play_trigger = 1;//play note
            tempo_v1_flag = 0;
            note_count++;
        }
            
    } // END WHILE(1)   
    PT_END(pt);  
} // thread blink


// === Buttons thread ==========================================================
// process buttons from Python to play and stop music
static PT_THREAD (protothread_buttons(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1){
        PT_YIELD_UNTIL(pt, new_button==1);
        // clear flag
        new_button = 0;   
        // Button one -- Play
        if (button_id==1 && button_value==1) {
            play_music = 1;
            tempo_v1_count = 0;
            tempo_v1_flag = 0;
            note_count = 0;
           break;
        }
        // Button 2 -- Stop
        if (button_id==2 && button_value==1) {
            play_music = 0; //stop
        }
    } // END WHILE(1)   
    PT_END(pt);  
} // thread blink

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
        // pushbutton
        if (PT_term_buffer[0]=='b'){
            // signal the button thread
            new_button = 1;
            // subtracting '0' converts ascii to binary for 1 character
            button_id = (PT_term_buffer[1] - '0')*10 + (PT_term_buffer[2] - '0');
            button_value = PT_term_buffer[3] - '0';

        }
        
//        // listbox
        if (PT_term_buffer[0]=='l'){
            new_list = 1;
            list_id = PT_term_buffer[2] - '0' ;
            list_value = PT_term_buffer[3] - '0';
        }
        
        // radio group
        if (PT_term_buffer[0]=='r'){
            new_radio = 1;
            radio_group_id = PT_term_buffer[2] - '0' ;
            radio_member_id = PT_term_buffer[3] - '0';
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
// set up UART, timer2, threads
// then schedule them as fast as possible

int main(void)
{
  
  // === config the uart, DMA, vref, timer5 ISR =============
  PT_setup();
  
   // === setup system wide interrupts  ====================
  INTEnableSystemMultiVectoredInt();
    
    // 400 is 100 ksamples/sec
    // 1000 is 40 ksamples/sec
    // 2000 is 20 ksamp/sec
    // 1000 is 40 ksample/sec
    // 2000 is 20 ks/sec
    OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, 2000);
    // set up the timer interrupt with a priority of 2
    ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
    mT2ClearIntFlag(); // and clear the interrupt flag

    // SCK2 is pin 26 
    // SDO2 (MOSI) is in PPS output group 2, could be connected to RB5 which is pin 14
    PPSOutput(2, RPB5, SDO2);

    // control CS for DAC
    mPORTBSetPinsDigitalOut(BIT_4);
    mPORTBSetBits(BIT_4);

    // divide Fpb by 2, configure the I/O ports. Not using SS in this example
    // 16 bit transfer CKP=1 CKE=1
    // possibles SPI_OPEN_CKP_HIGH;   SPI_OPEN_SMP_END;  SPI_OPEN_CKE_REV
    // For any given peripherial, you will need to match these
    SpiChnOpen(spiChn, SPI_OPEN_ON | SPI_OPEN_MODE16 | SPI_OPEN_MSTEN | SPI_OPEN_CKE_REV , spiClkDiv);
  // === now the threads ====================
    

    // init the display
  tft_init_hw();
  tft_begin();
  tft_fillScreen(ILI9340_BLACK);
  //240x320 vertical display
  tft_setRotation(1); // Use tft_setRotation(1) for 320x240
  
   // === identify the threads to the scheduler =====
  // add the thread function pointers to be scheduled
  // --- Two parameters: function_name and rate. ---
  // rate=0 fastest, rate=1 half, rate=2 quarter, rate=3 eighth, rate=4 sixteenth,
  // rate=5 or greater DISABLE thread!
  prev_prev_note = note_seeds[seed][2];//rightmost   Change index of first thing to seed variable
  prev_note = note_seeds[seed][1]; //center
  curr_note = note_seeds[seed][0];//leftmost
  
  prev_prev_note_duration = duration_seeds[seed][2]; //rightmost
  prev_note_duration = duration_seeds[seed][1]; //middle of tuple
  curr_note_duration = duration_seeds[seed][0]; //leftmost of tuple
  
  prev_octave = octave_seeds[seed][1]; 
  curr_octave = octave_seeds[seed][0];
  
  pt_add(protothread_cmd, 0);
  pt_add(protothread_serial, 0);
  pt_add(protothread_markov, 0);
  pt_add(protothread_buttons, 0);
  pt_add(protothread_music,0);
  pt_add(protothread_radio,0);
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


//   // init the threads
//   PT_INIT(&pt_cmd);
//   PT_INIT(&pt_tick);

  // turn off the sustain until triggered
  sustain_state = float2Accum(100.0);
  
  // build the sine lookup table
   // scaled to produce values between 0 and 4095
   int k;
   for (k = 0; k < sine_table_size; k++){
         sine_table[k] =  float2Accum(2047.0*sin((float)k*6.283/(float)sine_table_size));
    }

      // === scheduler thread =======================
  // scheduler never exits
  PT_SCHEDULE(protothread_sched(&pt_sched));
  // ===========================================
} // main
