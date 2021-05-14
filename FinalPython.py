import PySimpleGUI as sg
# on linux:
# sudo pip3 install pyserial
# /home/bruce/.local/lib/python3.5
# sudo python3.5 pic_target_3.py
#
# you can find out more about PySimleGUI at
# https://pysimplegui.readthedocs.io/en/latest/
#
# --event format to PIC--
# Four sharacters for each non-string event:
# pushbutton event 'b' + 2 digit button number + value (1,0)
# toggle sw event 't' + 2 digit button number + value (1,0)
# slider event 's' + 1-digit slider number + n digit value
# listbox event 'l' + + 1-digit listbox number + 1 digit selection number
# radio button 'r' + 1 digit group number + 1 digit selection numbr
# -- string --
# strings typed in the input line are sent in their entirety.
# -- reset --
# RESET has NO code on PIC!
# serial reset event sends a rs-232 BREAK which is connected
# through a filter to MCLR pin
#
# Python_TX_pin--(100ohm)--(+Schottky Diode-)------>(target MCLR pin)
#                                             |
#                                     (10uf)------(1kohm)
#                                        |           |
#                                        -------------
#                                             |
#                                         (PIC gnd)
#
import time
import serial
# open microcontroller serial port
# For windows the device will be 'COMx'
ser = serial.Serial('COM4', 115200, timeout=0.001)  # open serial port 38400

#sg.theme('DarkAmber')   # Add a touch of color
# All the stuff inside your window.
# This a heirachical list of items to be displayd in the window
# First list is first row controls, etc
# Buttons:
#   Realtime buttons respond to push-events
#   After the window is defined below, release events may be bound to each realtime button
#   The 'key' for each button must be of the form 'pushbutNN', 
#   where 'NN' are digits 0-9 defining the button number
# Toggles:
#   Toggle switches are actually checkboxes
#   The 'key' for each checkbox must be of the form 'toggleNN', 
#   where 'NN' are digits 0-9 defining the checkbox number
# Sliders
#   The 'key' for each slider must be of the form 'sliderN', 
#   where 'N' is a digit 0-9 defining the slider number
#   Sliders can have any integer range which is handy for the application
# Text
#   The text input field acts like the one-line Arduino serial send box.
#   The multiline output box receives serial from the PIC. text typed here is ignored.
# Listbox
#   The 'key' for each listbox must be of the form 'listN', 
#   where 'N' is a digit 0-9 defining the listbox number
#   Listbox as implemented can have only one selected value 
font_spec = 'Courier 24 bold'
heading_color = '#4b188f'
layout = [  [sg.Text('Markov Song Generator',  background_color=heading_color)],
            [sg.RealtimeButton('Play A Song', key='pushbut01', font='Helvetica 12'),
			 sg.RealtimeButton('Stop', key='pushbut02', font='Helvetica 12')],
			 
            #
            [sg.Text('Serial data to PIC', background_color=heading_color)],
            [sg.InputText('', size=(40,10), key='pic_input', do_not_clear=False,
                enable_events=False, focus=True),
             sg.Button('Send', key='pic_send', font='Helvetica 12')],
            #
            [sg.Text('Serial data from PIC', background_color=heading_color)],
            [sg.Multiline('', size=(50,10), key='console',
               autoscroll=True, enable_events=False)],
            #
            [sg.Text('System Controls', background_color=heading_color)],
            [sg.Button('Exit', font='Helvetica 12')],
            [ sg.Checkbox('reset_enable', key='r_en', 
                        font='Helvetica 8', enable_events=True),
             sg.Button('RESET PIC', key='rtg', font='Helvetica 8')
            ],
			[sg.Radio('Pluck String', "radio1", default=True, key='radio1_1', enable_events=True),
             sg.Radio('Low string', "radio1", key='radio1_2', enable_events=True),
			 sg.Radio('Bass', "radio1", key='radio1_3', enable_events=True)],
			 [sg.Radio('Mallot', "radio1", key='radio1_5', enable_events=True),	
			 sg.Radio('Chime', "radio1", key='radio1_7', enable_events=True),
             sg.Radio('Synth', "radio1", key='radio1_8', enable_events=True)				 
			 ]			
         ]

# change the colors in any way you like.
sg.SetOptions(background_color='#692c00',
       text_element_background_color='#692c00',
       element_background_color='#475841',#'#9FB8AD',
       scrollbar_color=None,
       input_elements_background_color='#692c00',#'#F7F3EC',
       progress_meter_color = ('green', 'blue'),
       button_color=('white','#475841'),
       )

# Create the Window
window = sg.Window('Markov Song Generator', layout, location=(0,0), 
                    return_keyboard_events=True, use_default_focus=True,
                    element_justification='c', finalize=True)

# Bind the realtime button release events <ButtonRelease-1>
# https://github.com/PySimpleGUI/PySimpleGUI/issues/2020
window['pushbut01'].bind('<ButtonRelease-1>', 'r')
window['pushbut02'].bind('<ButtonRelease-1>', 'r')
#window['pushbut03'].bind('<ButtonRelease-1>', 'r')

# Event Loop to process "events" 
# event is set by window.read
event = 0
#
#  button state machine variables
button_on = 0
button_which = '0'
#
#
while True:
    
    # time out paramenter makes the system non-blocking
    # If there is no event the call returns event  '__TIMEOUT__'
    event, values = window.read(timeout=20) # timeout=10
    #
    #print(event)  # for debugging
    # if user closes window using windows 'x' or clicks 'Exit' button  
    if event == sg.WIN_CLOSED or event == 'Exit': # 
        break
    #
    # pushbutton events state machine
    if event[0:3]  == 'pus' and button_on == 0 :
       # 'b' for button, two numeral characters, a '1' for pushed, and a terminator
       ser.write(('b' + event[7:9] + '1' + '\r').encode()) 
       button_on = 1
       button_which = event[7:9]
    # releaase event signalled by the 'r'
    elif (button_on == 1 and event[7:10] == button_which +'r') :
       ser.write(('b'  + button_which + '0' + '\r').encode()) 
       button_on = 0
       button_which = ' '
    #
    # listbox
    if event[0:3]  == 'lis'  : 
       # get the list box index#
       listbox_value = window.Element(event).GetIndexes()
       ser.write(('l0' + event[4] + str(listbox_value[0]) + '\r').encode()) 
    #
    # radio button
    if event[0:3]  == 'rad'  : 
       #print(event)
       # get the radio group ID and group-member ID radio1_2
       ser.write(('r0' + event[5] + event[7] + '\r').encode()) 

    # toggle switches
    if event[0:3]  == 'tog'  : 
       # read out the toggle switches
       switch_state = window.Element(event).get()
       ser.write(('t' + event[6:8] + str(switch_state) + '\r').encode())  
    #
    # silder events
    if event[0:3]  == 'sli'  :
       ser.write(('s ' + event[6] + " {:f}".format((values[event])) + '\r').encode()) 
    #
    # reset events
    switch_state = window.Element('r_en').get()
    if event[0:3] == 'rtg' and switch_state == 1 :
       # drops the data line for 100 mSec
       ser.send_break() #optional duration; duration=0.01
    #
    # The one-line text input button event
    if event == 'pic_send':
       # The text from the one-line input field
       input_state = window.Element('pic_input').get()
       # add <cr> for PIC
       input_state = '$' + input_state + '\r'
       # zero the input field
       window['pic_input'].update('')
       # send to PIC protothreads
       ser.write((input_state).encode())
       #
    # character loopback from PIC
    while ser.in_waiting > 0:
       #serial_chars = (ser.read().decode('utf-8'));
       #window['console'].update(serial_chars+'\n', append=True)
       pic_char = chr(ser.read(size=1)[0]) 
       if (pic_char) == '\r' :
          window['console'].update('\n', append=True)
       else :
          window['console'].update((pic_char), append=True)
     
# close port and Bail out
ser.close()             
window.close()





