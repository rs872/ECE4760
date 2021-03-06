/*
 * File:        lab-3
 * Author:      Rishi Singhal, William Salcedo, Raghav Kumar with code adapted from Bruce Land
 * 
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
#include <math.h>
////////////////////////////////////


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


//used for Python GUI
char new_string = 0;
char new_slider = 0;

// current slider
int slider_id;
float slider_value ; // value could be large


// current string
char receive_string[64];
// === the fixed point macros ========================================
#define float2Accum(a) ((_Accum)(a))
#define Accum2float(a) ((float)(a))
#define int2Accum(a) ((_Accum)(a))
#define Accum2int(a) ((int)(a))
#define abs(x) ((x > 0) ? x: -x)
// === input arrays ==================================================
#define nSamp 512
#define nPixels 256 //slightly higher than height of the screen

//=== sample rate ===========================
int sample_rate = 11000;
int timer_match;



short int v_in[nSamp] ;

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_fft ;

// string buffer
char buffer[60];

// system 1 second interval tick
int sys_time_seconds ;

//current x position
short int current_x = 50;

// === print line for TFT ============================================
// print a line on the TFT
// string buffer
char buffer[60];
void printLine(int line_number, char* print_buffer, short text_color, short back_color){
    // line number 0 to 31 
    /// !!! assumes tft_setRotation(0);
    // print_buffer is the string to print
    int v_pos;
    v_pos = line_number * 10 ;
    // erase the pixels
    tft_fillRoundRect(0, v_pos, 239, 8, 1, back_color);// x,y,w,h,radius,color
    tft_setTextColor(text_color); 
    tft_setCursor(0, v_pos);
    tft_setTextSize(1);
    tft_writeString(print_buffer);
}

// === print line for TFT, larger font ============================================
void printLine2(float line_number, char* print_buffer, short text_color, short back_color){
    // line number 0 to 31 
    /// !!! assumes tft_setRotation(0);
    // print_buffer is the string to print
    //modified to accept floats to allow for line numbers in 0.5 increments
    int v_pos;
    v_pos = line_number * 20 ;
    // erase the pixels
    tft_fillRoundRect(0, v_pos, 40, 16, 1, back_color);// x,y,w,h,radius,color
    tft_setTextColor(text_color); 
    tft_setCursor(0, v_pos);
    tft_setTextSize(2);
    tft_writeString(print_buffer);
}

void printLineXY(int x, float line_number, char* print_buffer, short text_color, short back_color){
    // line number 0 to 31 
    /// !!! assumes tft_setRotation(0);
    // print_buffer is the string to print
    int v_pos;
    v_pos = line_number * 20 ;
    // erase the pixels
    tft_fillRoundRect(x, v_pos, 239, 16, 1, back_color);// x,y,w,h,radius,color
    tft_setTextColor(text_color); 
    tft_setCursor(x, v_pos);
    tft_setTextSize(2);
    tft_writeString(print_buffer);
}

//=== FFT ==============================================================
// FFT
#define N_WAVE          512    /* size of FFT 512 */
#define LOG2_N_WAVE     9     /* log2(N_WAVE) 0 */
#define begin {
#define end }

_Accum Sinewave[N_WAVE]; // a table of sines for the FFT
_Accum window[N_WAVE]; // a table of window values for the FFT
_Accum fr[N_WAVE], fi[N_WAVE];
int pixels[nPixels] ;

inline unsigned short getColor(_Accum magnitude) { //convert to colors
    if(magnitude < int2Accum(1)){
        return 0x0000;
    }
    else if(magnitude < int2Accum(2)){
        return 0x2945;
    }
    else if(magnitude < int2Accum(4)){
        return 0x4a49;
    }
    else if(magnitude < int2Accum(8)){
        return 0x738e;
    }
    else if(magnitude < int2Accum(16)){
        return 0x85cl;
    }
    else if(magnitude < int2Accum(32)){
        return 0xad55;
    }
    else if(magnitude < int2Accum(64)){
        return 0xc638;
    }
    else {
        return 0xFFFF; //if magnitude is greater than or equal to 64
    }
    
   
}

void FFTaccum(_Accum fr[], _Accum fi[], int m)
//Adapted from code by:
//Tom Roberts 11/8/89 and Malcolm Slaney 12/15/94 malcolm@interval.com
//fr[n],fi[n] are real,imaginary arrays, INPUT AND RESULT.
//size of data = 2**m
// This routine does foward transform only
begin
    int mr,nn,i,j,L,k,istep, n;
    _Accum qr,qi,tr,ti,wr,wi;

    mr = 0;
    n = 1<<m;   //number of points
    nn = n - 1;

    /* decimation in time - re-order data */
    for(m=1; m<=nn; ++m)
    begin
        L = n;
        do L >>= 1; while(mr+L > nn);
        mr = (mr & (L-1)) + L;
        if(mr <= m) continue;
        tr = fr[m];
        fr[m] = fr[mr];
        fr[mr] = tr;

    end

    L = 1;
    k = LOG2_N_WAVE-1;
    while(L < n)
    begin
        istep = L << 1;
        for(m=0; m<L; ++m)
        begin
            j = m << k;
            wr =  Sinewave[j+N_WAVE/4];
            wi = -Sinewave[j];

            for(i=m; i<n; i+=istep)
            begin
                j = i + L;
                tr = (wr * fr[j]) - (wi * fi[j]);
                ti = (wr * fi[j]) + (wi * fr[j]);
                qr = fr[i] >> 1;
                qi = fi[i] >> 1;
                fr[j] = qr - tr;
                fi[j] = qi - ti;
                fr[i] = qr + tr;
                fi[i] = qi + ti;
            end
        end
        --k;
        L = istep;
    end
end

// === FFT Thread =============================================
    
// DMA channel busy flag
#define CHN_BUSY 0x80
#define log_min 0x10   
static PT_THREAD (protothread_fft(struct pt *pt))
{
    PT_BEGIN(pt);
    
    static int sample_number ;
    static _Accum zero_point_4 = float2Accum(0.4) ;
    // approx log calc ;
    static int sx, y, ly, temp ;
    while(1) {
        // yield time 1 second
        PT_YIELD_TIME_msec(30);
        
        // enable ADC DMA channel and get
        // 512 samples from ADC
        DmaChnEnable(1); //enable channel 1
        // yield until DMA done: while((DCH0CON & Chn_busy) ){};
        PT_WAIT_WHILE(pt, DCH1CON & CHN_BUSY);
        
        // compute and display fft
        // load input array
        for (sample_number=0; sample_number<nSamp-1; sample_number++){
            // window the input and perhaps scale it
            fr[sample_number] = (int2Accum(v_in[sample_number]) * window[sample_number]); 
            fi[sample_number] = 0 ;
        }
        
        // do FFT
        FFTaccum(fr, fi, LOG2_N_WAVE);
        // get magnitude and log
        // The magnitude of the FFT is approximated as: 
        //   |amplitude|=max(|Re|,|Im|)+0.4*min(|Re|,|Im|). 
        // This approximation is accurate within about 4% rms.
        // https://en.wikipedia.org/wiki/Alpha_max_plus_beta_min_algorithm
        for (sample_number = 0; sample_number < nPixels; sample_number++) {  
            // get the approx magnitude
            fr[sample_number] = abs(fr[sample_number]); //>>9
            fi[sample_number] = abs(fi[sample_number]);
            // reuse fr to hold magnitude
            fr[sample_number] = max(fr[sample_number], fi[sample_number]) + 
                    (min(fr[sample_number], fi[sample_number]) * zero_point_4); 
        }
            
        int sample_incr = sample_rate / 8; //amount for each tick on the axis for frequency (sample_rate/2)/4
        
        // display scale for the frequency axis
        sprintf(buffer, "%d", sample_incr * 4 );
        printLine2(0.5, buffer, ILI9340_WHITE, ILI9340_BLACK);
        
        sprintf(buffer, "%d", sample_incr * 3 );
        printLine2(3.5, buffer, ILI9340_WHITE, ILI9340_BLACK);
        
        sprintf(buffer, "%d", sample_incr * 2 );
        printLine2(6.5, buffer, ILI9340_WHITE, ILI9340_BLACK);
        
        sprintf(buffer, "%d", sample_incr);
        printLine2(9.5, buffer, ILI9340_WHITE, ILI9340_BLACK);

        // Display on TFT
        // erase, then draw
        tft_fillRect(current_x,0,10,240,ILI9340_BLACK); //fill first 10 pixels ahead of the pointer with black (mimic the scrolling of an oscilloscope)
        int largest_bin = 6; //define largest magnitude bin (random value, actual value to be calculated later)
        for (sample_number = 1; sample_number < 240; sample_number++) { //start at bin 1 to skip the DC noise bin
            tft_drawPixel(current_x, 240 - sample_number, getColor(fr[sample_number]) ); //plot each frequency bin at a different color based on its magnitude
            if(fr[sample_number] > fr[largest_bin] && sample_number > 5){ //find the bin with the largest magnitude (skip the low frequency bins due to dc noise)
                largest_bin = sample_number;
            }
            // reuse fr to hold magnitude 
        }  

        //frequency range is divided among 512 bins, use ratio multiplied by the sample rate to get max frequency
        sprintf(buffer, "Max Freq: %d", sample_rate * largest_bin / 512); 
        printLineXY(50, 0, buffer, ILI9340_WHITE, ILI9340_BLACK);

        current_x++;        
        if(current_x == 320){ // keep plotting within the margins, refresh if at the end of the screen
            current_x = 50;
        }
        // NEVER exit while
        
      } // END WHILE(1)
  PT_END(pt);
} // FFT thread

// === Slider Thread =============================================
// 
static PT_THREAD (protothread_sliders(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1){
        PT_YIELD_UNTIL(pt, new_slider==1);
        // clear flag
        new_slider = 0; 
        if (slider_id == 1){ // turn factor slider
            sample_rate = slider_value * 2; //sample rate is 2 times the frequency based on nyquist criteria
            timer_match = 40000000/sample_rate; // set timer to 40MHz divided by sample rate
            CloseTimer3();
            OpenTimer3(T3_ON | T3_SOURCE_INT | T3_PS_1_1, timer_match); //initialize timer3 set to the desired frequency we want
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

    ANSELA = 0; //analaog select A and B
    ANSELB = 0;
    // === config threads ========================
    // turns OFF UART support and debugger pin, unless defines are set
    PT_setup();
    // === setup system wide interrupts  ========
    INTEnableSystemMultiVectoredInt();

    // the ADC ///////////////////////////////////////
    
    // timer 3 setup for adc trigger  ==============================================
    // Set up timer3 on,  no interrupts, internal clock, prescalar 1, compare-value
    timer_match = 40000000/sample_rate;
    OpenTimer3(T3_ON | T3_SOURCE_INT | T3_PS_1_1, timer_match); 
    
    //=== DMA Channel 1 transfer ADC data to array v_in ================================
    // Open DMA Chan1 for later use sending video to TV
    #define DMAchan1 1
	DmaChnOpen(DMAchan1, 0, DMA_OPEN_DEFAULT);
    DmaChnSetTxfer(DMAchan1, (void*)&ADC1BUF0, (void*)v_in, 2, nSamp*2, 2); //512 16-bit integers
    DmaChnSetEventControl(DMAchan1, DMA_EV_START_IRQ(28)); // 28 is ADC done
    // ==============================================================================
    
    // configure and enable the ADC
    CloseADC10(); // ensure the ADC is off before setting the configuration

    // define setup parameters for OpenADC10
    // Turn module on | ouput in integer | trigger mode auto | enable autosample
    // ADC_AUTO_SAMPLING_ON -- Sampling begins immediately after last conversion completes; SAMP bit is automatically set
    #define PARAM1  ADC_FORMAT_INTG16 | ADC_CLK_TMR | ADC_AUTO_SAMPLING_ON //

    // define setup parameters for OpenADC10
    // ADC ref external  | disable offset test | disable scan mode | do 1 sample | use single buf | alternate mode off
    #define PARAM2  ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_OFF | ADC_SAMPLES_PER_INT_1 | ADC_ALT_BUF_OFF | ADC_ALT_INPUT_OFF
    //
    // Define setup parameters for OpenADC10
    // use peripherial bus clock | set sample time | set ADC clock divider
    // ADC_SAMPLE_TIME_6 seems to work with a source resistance < 1kohm
    #define PARAM3 ADC_CONV_CLK_PB | ADC_SAMPLE_TIME_6 | ADC_CONV_CLK_Tcy //ADC_SAMPLE_TIME_5| ADC_CONV_CLK_Tcy2

    // define setup parameters for OpenADC10
    // set AN11 and  as analog inputs
    #define PARAM4	ENABLE_AN11_ANA // pin 24

    // define setup parameters for OpenADC10
    // do not assign channels to scan
    #define PARAM5	SKIP_SCAN_ALL

    // use ground as neg ref for A | use AN11 for input A     
    // configure to sample AN11 
    SetChanADC10(ADC_CH0_NEG_SAMPLEA_NVREF | ADC_CH0_POS_SAMPLEA_AN11); // configure to sample AN4 
    OpenADC10(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5); // configure ADC using the parameters defined above

    EnableADC10(); // Enable the ADC
    ///////////////////////////////////////////////////////

    // === init FFT data =====================================
    // one cycle sine table
    //  required for FFT
    int ii;
    for (ii = 0; ii < N_WAVE; ii++) {
        Sinewave[ii] = float2Accum(sin(6.283 * ((float) ii) / N_WAVE)*0.5);
        window[ii] = float2Accum(1.0 * (1.0 - cos(6.283 * ((float) ii) / (N_WAVE - 1))));
    }

    // === TFT setup ============================
    // init the display in main since more than one thread uses it.
    // NOTE that this init assumes SPI channel 1 connections
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);
    //240x320 vertical display
    tft_setRotation(1); // Use tft_setRotation(1) for 320x240

    // Add in ticks and axis for the spectrogram, permanently drawn
    tft_fillRect(48,0,1,240,ILI9340_RED); //vertical line
    tft_fillRect(30,240-2,18,2,ILI9340_RED); //bottom tick
    tft_fillRect(30,240-62,18,2,ILI9340_RED); //2nd tick
    tft_fillRect(30,240-122,18,2,ILI9340_RED); //3rd tick
    tft_fillRect(30,240-182,18,2,ILI9340_RED); //4th tick
    tft_fillRect(30,0,18,2,ILI9340_RED); //top tick

    
    // === identify the threads to the scheduler =====
    // add the thread function pointers to be scheduled
    // --- Two parameters: function_name and rate. ---
    // rate=0 fastest, rate=1 half, rate=2 quarter, rate=3 eighth, rate=4 sixteenth,
    // rate=5 or greater DISABLE thread!

    pt_add(protothread_serial, 0);
    pt_add(protothread_fft,0);
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
