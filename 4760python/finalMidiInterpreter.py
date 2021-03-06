import py_midicsv as pm
import pandas as pd
import os
import numpy as np

#Pathnames for files to be loaded from or saved to
curr_path = os.path.dirname(os.path.abspath(__file__)) #current path
midiFile = os.path.join(curr_path, "test4.mid") #midiFile to be parsed
outFile = os.path.join(curr_path, "output.csv") #Raw CSV from midi file
no_headers_csv = os.path.join(curr_path, "no_headers.csv") #CSV after removing the headers and footers
processed_data_csv = os.path.join(curr_path, "processed_data.csv") #Contains: Note, start time, end time, duration (end time-start time)
markov_note_file = os.path.join(curr_path, "MarkovNote.txt") #Markov chains generated for notes-to be added to header file in PIC32
markov_duration_file = os.path.join(curr_path, "MarkovDuration.txt") #Markov chains generated for duration-to be added to header file in PIC32
markov_octave_file = os.path.join(curr_path, "MarkovOctave.txt") #Markov chains generated for ocatve-to be added to header file in PIC32
seeds_file = os.path.join(curr_path, "Seeds.txt") #Seeds that determine the notes, durations and octaves to start the algorithm from

#create numpy array that is 13x13x13x13 for notes and 8x8x8x8 for duration. All populated by 0s
markov_note = np.full((12,12,12,12),0) 
markov_octave = np.full((12,12,4,4,4),0) #n^6 and we have n = 4 octaves (Current octave depends on 3 notes and 2 past octaves)
markov_duration = np.full((12,8,8,8),0)

#Parse midi file and convert to CSV
directory = os.path.join(curr_path, 'training-data')
for filename in os.listdir(directory):
    if filename.endswith(".mid") or filename.endswith(".midi"):
        midiFile = os.path.join(directory, filename)
        # Load the MIDI file and parse it into CSV format
        csv_string = pm.midi_to_csv(midiFile)
        with open(outFile, "w") as f: #write to output.csv file
            f.write("a, b, c, d, e, f\n")
            f.writelines(csv_string)
            f.close()


        endMidi = pd.read_csv(outFile,error_bad_lines=False)

        #access the first row and convert to numpy array, to get the ticks per beat
        firstRow = endMidi.loc[[0]].to_numpy()  
        #(Note_off - Note_on)/ ticks_per_beat = note type (1/8th etc)
        ticks_per_beat = firstRow[0,5]
        if (type(ticks_per_beat) == str):
            ticks_per_beat.strip() #remove whitespaces from the string, only store the number. To be converted to float
        ticks_per_beat = float(ticks_per_beat)


        endMidi.drop(endMidi.columns[[0,3]], axis=1,inplace = True) #remove columns 0 to 3. Modify inplace, not a copy

        # .shape returns the dimensions of the dataframe
        # Check every row for the first instance of "Note_on_C" to show the start of a note being played. Delete all prior rows
        for row_index in range(endMidi.shape[0]): 
            if (endMidi.iloc[row_index, 1]).strip() == 'Note_on_c':
                endMidi = endMidi.iloc[row_index:] #Delete every row before this first instance of note_on_c
                break 

        #check every row from the bottom for the first instance of "Note_off_C" or "Note_on_C" 
        #Delete every row after it
        for row_index in range(endMidi.shape[0] - 1, -1, -1):
            if ((endMidi.iloc[row_index, 1].strip() == 'Note_on_c') or (endMidi.iloc[row_index, 1].strip() == 'Note_off_c')):
                endMidi = endMidi.iloc[:row_index]
                break
            
        #convert this dataframe with removed headers and footers to CSV 
        endMidi.to_csv(no_headers_csv, index = False)

        endMidi = endMidi.to_numpy() #convert to numpy array

        processed_data = []

        #1 loop through the notes in csv (column 3)
        #2 identify unique notes, insert them in 2D array and store the start time (assuming first hit of unique note is always turn on)
        #3 as soon as we identify a repeated note, we get the end time, subtract to get duration (assuming second hit of note is always turn off)
        # 2D array contains: (notes, start time, end time, duration)
        for index in range(endMidi.shape[0]):
            on_or_off = endMidi[index, 1] #store on_or_off string value
            if (type(on_or_off) == str):
                on_or_off = on_or_off.strip() #remove whitespace

            #skips over rows that don't contain note_on_c or note_off_c or have notes out of range from our implementation
            if ((on_or_off != 'Note_on_c' and on_or_off != 'Note_off_c') or int(endMidi[index][2]) < 24 or int(endMidi[index][2]) > 71):
                continue
            
            #get velocity, having a note on with velocity = 0 is equivalent to note_off
            velocity = endMidi[index, 3]
            if (type(velocity) == str):
                velocity = velocity.strip()
                velocity = int(velocity)

            #if the note is on, add its note value and start tick to 2D array
            if (on_or_off ==  'Note_on_c') and (velocity != 0):
                processed_data.append([endMidi[index, 2], endMidi[index, 0], None, None])
            
            elif ((velocity == 0) and (on_or_off == 'Note_on_c')) or (on_or_off ==  'Note_off_c'):
                #elif the note is off, start iterating from the bottom of the processed_data (not endMidi)
                for index2 in range(len(processed_data) - 1, -1, -1): 
                    #if the endMidi off note (lets say it's 50) is same as the most recently added note (of the same kind, so 50) in processed_data, 
                    #we can be assured that that is an on note
                    if (endMidi[index, 2] == processed_data[index2][0]): 
                        #hence, save as end time on index2 note of processed_data, the time of the index note in endMidi
                        processed_data[index2][2] = endMidi[index, 0] 
                        #store duration: note type (quarter note, half note, full note etc )
                        processed_data[index2][3] = (processed_data[index2][2] - processed_data[index2][1]) / ticks_per_beat
                        #multiply by 8 because our smallest duration is 0.125 and we want to normalize to 1. Since there are a lot of decimals durations,
                        #the code below is a way to round them all off to multiples of 0.125.
                        #so first multiply by 8
                        processed_data[index2][3] *= 8
                        #now round to a decimal
                        processed_data[index2][3] = round(processed_data[index2][3]) 
                        if (processed_data[index2][3] == 0): #if the note type is below an eighth, make it an eighth.
                            processed_data[index2][3] = 1
                        if (processed_data[index2][3] > 8): #if the note type is longer than a full note, reduce it to a note.
                            processed_data[index2][3] = 8
                        processed_data[index2][3] /= 8 #Divide by 8 to bring back to decimal values.
                        break

        processed_data_df = pd.DataFrame(processed_data)
        processed_data_df.to_csv(processed_data_csv, index = False)
        # savetxt(processed_data_csv, processed_data, delimiter=',')

    
        #instantiate the note, duration and octave pipelines. 
        
        curr_note = None
        prev_note = None
        prev_x2_note = None
        prev_x3_note = None

        curr_duration = None
        prev_duration = None
        prev_x2_duration = None
        prev_x3_duration = None

        curr_octave = None
        prev_octave = None
        prev_x2_octave = None
        
        #go through each row of the processed_data array
        for note_array_index in range(len(processed_data)):
            
            #note down the current time to figure out which notes need to be looked at together
            curr_start_time = int(processed_data[note_array_index][1])
            #Note Parallel Algo
            curr_note = []
            curr_duration = []
            curr_octave = []
            #from the current index in the processed data, iterate down the processed_data array to add notes, 
            #durations and octaves that all start at the same time
            for note_array_index_2 in range(note_array_index,len(processed_data)-note_array_index):
                #check if the start time of the note at note_array_index_2 is equal to the start time of the note at note_array_index.
                #if not, break.
                if(processed_data[note_array_index_2][1] != curr_start_time):
                    break
                #if it is, add note, duration and octave to their respective arrays.
                #the octave is essentially a value between 0 and 3 inclusive
                #using try-catch block since we saw some errors attributed to 'bad' csv lines.
                try:
                    temp_note = int(processed_data[note_array_index_2][0])
                    curr_note.append(temp_note)
                    curr_duration.append(float(processed_data[note_array_index_2][3]))
                    #this gives the octave of the current note relative to our lowest note
                    curr_octave.append(int((temp_note - 48)/ 12))

      
                    
                except:
                    print('in except')
                    continue
            
            
            #just propogate the chain. This will update the notes, durations and octaves at every time step. 
            #the loops below use this information to update the markov transition matrices.
            
            #notes
            prev_x3_note = prev_x2_note
            prev_x2_note = prev_note
            prev_note = curr_note

            #duration
            prev_x3_duration = prev_x2_duration
            prev_x2_duration = prev_duration
            prev_duration = curr_duration

            #octaves
            prev_x2_octave = prev_octave
            prev_octave = curr_octave
            
            ### THIS LOOP UPDATES THE TRANSITION MATRICES FOR ALL MARKOV CHAINS ###

            #if the pipeline of notes is not filled, skip updating the matrices
            if (prev_x3_note != None):
                #at this point, all the current arrays contain atleast a single element, and potentially more than one element. 
                #the goal for this note 4D loop is to find all permutations of the past three notes with the current note and update the 
                #12x12x12x12 matrix for each of the combinations.
                for prev_note_index in range(len(prev_note)):
                    for prev_x2_note_index in range(len(prev_x2_note)):
                        for prev_x3_note_index in range(len(prev_x3_note)):
                            for curr_note_index in range(len(curr_note)):
                                try:
                                    #We subtract by 48 in order to normalize for the lowest note Midi number which is 48. After subtracting from 48 and 
                                    #modding with 12, we essentially get an index value for each note. Thus each note is able to correspond to a particular index 
                                    #value that we are able to increment.......
                                    curr_markov_index_n = (int(curr_note[curr_note_index] - 48)) % 12
                                    prev_markov_index_n = (int(prev_note[prev_note_index] - 48)) % 12
                                    prev_x2_markov_index_n = (int(prev_x2_note[prev_x2_note_index] - 48)) % 12
                                    prev_x3_markov_index_n = (int(prev_x3_note[prev_x3_note_index] - 48)) % 12
                                    
                                    #.....here!
                                    markov_note[prev_markov_index_n, prev_x2_markov_index_n, prev_x3_markov_index_n, curr_markov_index_n] += 1
                                    
      
                                        
                                except:
                                    continue
                        
                        #since the octaves depend on the past two octaves and the past two notes, we do the same.
                        #take note that we make sure that the current note and prev_x3_note calculation is done before we udpate the octave 
                        #markov because the octave markov only depends on the prev 2 notes, not prev 3. 
                        for curr_octave_index in range(len(curr_octave)):
                            for prev_octave_index in range(len(prev_octave)):
                                for prev_x2_octave_index in range(len(prev_x2_octave)):
                                    
                                    #these two lines are essentially the same as the ones we wrote when updating the note markov chain above.
                                    prev_markov_index_n = (int(prev_note[prev_note_index] - 48)) % 12
                                    prev_x2_markov_index_n = (int(prev_x2_note[prev_x2_note_index] - 48)) % 12

                                    curr_octave_n =  curr_octave[curr_octave_index]
                                    prev_octave_n =  prev_octave[prev_octave_index]
                                    prev_x2_octave_n = prev_x2_octave[prev_x2_octave_index]            

                                    markov_octave[prev_markov_index_n, prev_x2_markov_index_n, prev_octave_n, prev_x2_octave_n, curr_octave_n] += 1 
                                   
            #do the same for duration markov as well..
            if (prev_x2_duration != None):
                for prev_duration_index in range(len(prev_duration)):
                    for prev_x2_duration_index in range(len(prev_x2_duration)):
                        for curr_duration_index in range(len(curr_duration)):
                            for prev_note_index_2 in range(len(prev_note)):
                                #We haven't been too smart with variable naming here, so excuse that :)
                                #We subtract by 1 because after dividing by 0.125, if the result is 1, then it should actually index into value 0
                                #and if the value is a 8 (0.125 * 8 = 1), then the increment should be made at index 8.
                                curr_markov_index_d = int(curr_duration[curr_duration_index] / 0.125) - 1 
                                prev_markov_index_d = int(prev_duration[prev_duration_index] / 0.125) - 1
                                prev_x2_markov_index_d = int(prev_x2_duration[prev_x2_duration_index] / 0.125) - 1
                                prev_markov_index_n_2 = (int(prev_note[prev_note_index_2] - 48)) % 12
                                markov_duration[prev_markov_index_n_2][prev_markov_index_d][prev_x2_markov_index_d][curr_markov_index_d] += 1
          
    else:
        continue


#here we loop through the transition matrices to normalize the values to 255
#we normalize to 255 because numbers upto 255 can be expressed as char that are a single byte vs the four bytes for an int.
#this saves space on the PIC.

note_seeds = []
markov_note_dim = 12
for i in range(markov_note_dim):
    for j in range(markov_note_dim):
        for k in range(markov_note_dim):
            #the accumulator variable for normalization
            accumulator = 0
            for l in range(markov_note_dim):
                accumulator += markov_note[i, j, k, l]
            if (accumulator != 0):
                print((i,j,k))
                #appending the seeds -- the non zero probability points to start our music generation from on the PIC.
                note_seeds.append([i,j,k])
                for t in range(markov_note_dim):
                    #once we have the accumulator value, loop through all the elements of the last row and normalize to 255.
                    markov_note[i, j, k, t] = 255 * markov_note[i, j, k, t]
                    markov_note[i, j, k, t] = int(markov_note[i, j, k, t] / accumulator)

duration_seeds = []
markov_duration_dim = 8
for i in range(markov_note_dim):
    for k in range(markov_duration_dim):
        for l in range(markov_duration_dim):
            accumulator = 0
            for m in range(markov_duration_dim):
                accumulator += markov_duration[i, k, l, m]
            if (accumulator != 0):
                #print('hi')
                #print((i,k,l))
                duration_seeds.append([k,l])
                for t in range(markov_duration_dim):
                    markov_duration[i, k, l, t] = 255 * markov_duration[i, k, l, t]
                    markov_duration[i, k, l, t] = int(markov_duration[i, k, l, t] / accumulator)

octave_seeds = []
markov_octave_dim = 4
for i in range(markov_note_dim):
    for j in range(markov_note_dim):
        for k in range(markov_octave_dim):
            for l in range(markov_octave_dim):
                accumulator = 0
                for m in range(markov_octave_dim):
                    accumulator += markov_octave[i, j, k, l, m]
                if (accumulator != 0):
                    octave_seeds.append([k,l])
                    for t in range(markov_octave_dim):
                        markov_octave[i, j, k, l, t] = 255 * markov_octave[i, j, k, l, t]
                        markov_octave[i, j, k, l, t] = int(markov_octave[i, j, k, l, t] / accumulator)
                


f= open(markov_note_file,"w+")

coolString = np.array2string(markov_note,threshold = np.sys.maxsize,separator=',')
x = coolString.replace('[','{')
y = x.replace(']','}')
f.write(y+';')

f.close()

f= open(markov_duration_file,"w+")

coolString = np.array2string(markov_duration,threshold = np.sys.maxsize,separator=',')
x = coolString.replace('[','{')
y = x.replace(']','}')
f.write(y+';')

f.close()

f= open(markov_octave_file,"w+")

coolString = np.array2string(markov_octave,threshold = np.sys.maxsize,separator=',')
x = coolString.replace('[','{')
y = x.replace(']','}')
f.write(y+';')

f.close()

f= open(seeds_file,"w+")

coolString = np.array2string(np.array(note_seeds),threshold = np.sys.maxsize,separator=',')
x = coolString.replace('[','{')
y = x.replace(']','}')
f.write('const unsigned char note_seeds [' + str(len(note_seeds)) + '][3] = \n')
f.write(y+';')

coolString = np.array2string(np.array(duration_seeds),threshold = np.sys.maxsize,separator=',')
x = coolString.replace('[','{')
y = x.replace(']','}')
f.write('\nconst unsigned char duration_seeds [' + str(len(duration_seeds)) + '][2] = \n')
f.write(y+';')

coolString = np.array2string(np.array(octave_seeds),threshold = np.sys.maxsize,separator=',')
x = coolString.replace('[','{')
y = x.replace(']','}')
f.write('\nconst unsigned char octave_seeds [' + str(len(octave_seeds)) + '][2]= \n')
f.write(y+';')


f.close()
    

processed_data_df = pd.DataFrame(processed_data)

processed_data_df.to_csv(outFile,index = False)
