import py_midicsv as pm
import pandas as pd
import os
import numpy as np


curr_path = os.path.dirname(os.path.abspath(__file__))
midiFile = os.path.join(curr_path, "test4.mid")
outFile = os.path.join(curr_path, "output.csv")
pre_processed = os.path.join(curr_path, "pre_processed.csv")
processed_data_csv = os.path.join(curr_path, "processed_data.csv")
markov_note_file = os.path.join(curr_path, "MarkovNote.txt")
markov_duration_file = os.path.join(curr_path, "MarkovDuration.txt")

markov_note = np.full((13,13,13,13),0)
markov_duration = np.full((8,8,8,8),0)

directory = os.path.join(curr_path, 'training-data')
for filename in os.listdir(directory):
    if filename.endswith(".mid") or filename.endswith(".midi"):
        midiFile = os.path.join(directory, filename)
        print(filename)
        # Load the MIDI file and parse it into CSV format
        csv_string = pm.midi_to_csv(midiFile)

        with open(outFile, "w") as f:
            f.write("a, b, c, d, e, f\n")
            f.writelines(csv_string)
            f.close()



        endMidi = pd.read_csv(outFile,error_bad_lines=False)

        temp = endMidi.loc[[0]].to_numpy()

        #(Note_off - Note_on)/ ticks_per_beat = note type (1/8th etc)
        ticks_per_beat = temp[0,5]
        if (type(ticks_per_beat) == str):
            ticks_per_beat.strip()
        ticks_per_beat = float(ticks_per_beat)


        endMidi.drop(endMidi.columns[[0,3]], axis=1,inplace = True)
        for row_index in range(endMidi.shape[0]):

            if (endMidi.iloc[row_index, 1]).strip() == 'Note_on_c':
                endMidi = endMidi.iloc[row_index:]
                break

        for row_index in range(endMidi.shape[0] - 1, -1, -1):
            if ((endMidi.iloc[row_index, 1].strip() == 'Note_on_c') or (endMidi.iloc[row_index, 1].strip() == 'Note_off_c')):
                endMidi = endMidi.iloc[:row_index]
                break
            

        endMidi.to_csv(pre_processed, index = False)


        endMidi = endMidi.to_numpy()


        processed_data = []

        #1 loop through frequencies in csv (column 3)
        #2 identify unique frequencies, insert them in 2D array and note start time (assuming first hit of unique freq is always turn on)
        #3 as soon as we identify a repeated frequency, we note end time, subtract to get duration (assuming second hit of freq is always turn off)

        for index in range(endMidi.shape[0]):
            on_or_off = endMidi[index, 1] #on_or_off is a string
            if (type(on_or_off) == str):
                on_or_off = on_or_off.strip()

            if ((on_or_off != 'Note_on_c' and on_or_off != 'Note_off_c') or int(endMidi[index][2]) < 48 + 24 or int(endMidi[index][2]) > 60 + 24):
                continue
            
            velocity = endMidi[index, 3]
            if (type(velocity) == str):
                velocity = velocity.strip()
                velocity = int(velocity)
            # print(on_or_off)
            # print(velocity )
            # print((velocity == 0) and (on_or_off == 'Note_on_c'))
            if (on_or_off ==  'Note_on_c') and (velocity != 0):
                processed_data.append([endMidi[index, 2], endMidi[index, 0], None, None])
            elif ((velocity == 0) and (on_or_off == 'Note_on_c')) or (on_or_off ==  'Note_off_c'):
                for index2 in range(len(processed_data) - 1, -1, -1): #ind_array = individual array in processed data
                    if (endMidi[index, 2] == processed_data[index2][0]):
                        processed_data[index2][2] = endMidi[index, 0]
                        processed_data[index2][3] = (processed_data[index2][2] - processed_data[index2][1]) / ticks_per_beat #note type (quarter note, half note, full note etc )
                        processed_data[index2][3] *= 8
                        processed_data[index2][3] = round(processed_data[index2][3])
                        if (processed_data[index2][3] == 0):
                            processed_data[index2][3] = 1
                        if (processed_data[index2][3] > 8):
                            processed_data[index2][3] = 8
                        processed_data[index2][3] /= 8
                        break

        processed_data_df = pd.DataFrame(processed_data)
        processed_data_df.to_csv(processed_data_csv, index = False)
        # savetxt(processed_data_csv, processed_data, delimiter=',')


        curr_note = None
        prev_note = None
        prev_x2_note = None
        prev_x3_note = None

        curr_duration = None
        prev_duration = None
        prev_x2_duration = None
        prev_x3_duration = None

        
        for note_array_index in range(len(processed_data)):
            

            curr_start_time = int(processed_data[note_array_index][1])
            #Note Parallel Algo
            curr_note = []
            curr_duration = []
            for note_array_index_2 in range(note_array_index,len(processed_data)-note_array_index):
                if(processed_data[note_array_index_2][1] != curr_start_time):
                    break
                try:
                    curr_note.append(int(processed_data[note_array_index_2][0]))
                    curr_duration.append(float(processed_data[note_array_index_2][3]))
                except:
                    continue


            ###This loop is 4D because it loops through the past 4 states of our chain which goes through the entire song. This makes concurrently
            # played notes get counted properly in our giant accumulator###
            if (prev_x3_note != None):
                for curr_note_index in range(len(curr_note)):
                    for prev_note_index in range(len(prev_note)):
                        for prev_x2_note_index in range(len(prev_x2_note)):
                            for prev_x3_note_index in range(len(prev_x3_note)):
                                try:
                                    curr_markov_index_n = int(curr_note[curr_note_index] - (48 + 24))
                                    prev_markov_index_n = int(prev_note[prev_note_index] - (48 + 24))
                                    prev_x2_markov_index_n = int(prev_x2_note[prev_x2_note_index] - (48 + 24))
                                    prev_x3_markov_index_n = int(prev_x3_note[prev_x3_note_index] - (48 + 24))
                                    

                                    curr_markov_index_d = int(curr_duration[curr_note_index] / 0.125) - 1 #shortest note is 0.125 but lowest index is 0; longest note is 1 but largest index is 7
                                    prev_markov_index_d = int(prev_duration[prev_note_index] / 0.125) - 1
                                    prev_x2_markov_index_d = int(prev_x2_duration[prev_x2_note_index] / 0.125) - 1
                                    prev_x3_markov_index_d = int(prev_x3_duration[prev_x3_note_index] / 0.125) - 1


                                    markov_note[prev_markov_index_n, prev_x2_markov_index_n, prev_x3_markov_index_n, curr_markov_index_n] += 1
                                    markov_duration[prev_markov_index_d][prev_x2_markov_index_d][prev_x3_markov_index_d][curr_markov_index_d] += 1
                                except:
                                    continue

            
            #notes
            prev_x3_note = prev_x2_note
            prev_x2_note = prev_note
            prev_note = curr_note

            #duration
            prev_x3_duration = prev_x2_duration
            prev_x2_duration = prev_duration
            prev_duration = curr_duration

    else:
        continue

markov_note_dim = 13
for i in range(markov_note_dim):
    for j in range(markov_note_dim):
        for k in range(markov_note_dim):
            accumulator = 0
            for l in range(markov_note_dim):
                accumulator += markov_note[i, j, k, l]
            if (accumulator != 0):
                print((i,j,k))
                for t in range(markov_note_dim):
                    markov_note[i, j, k, t] = 255 * markov_note[i, j, k, t]
                    markov_note[i, j, k, t] = int(markov_note[i, j, k, t] / accumulator)

markov_duration_dim = 8
for i in range(markov_duration_dim):
    for j in range(markov_duration_dim):
        for k in range(markov_duration_dim):
            accumulator = 0
            for l in range(markov_duration_dim):
                accumulator += markov_duration[i, j, k, l]
            if (accumulator != 0):
                print('hi')
                print((i,j,k))
                for t in range(markov_duration_dim):
                    markov_duration[i, j, k, t] = 255 * markov_duration[i, j, k, t]
                    markov_duration[i, j, k, t] = int(markov_duration[i, j, k, t] / accumulator)
                

f= open(markov_note_file,"w+")

coolString = np.array2string(markov_note,threshold = np.sys.maxsize,separator=',')
x = coolString.replace('[','{')
y = x.replace(']','}')
f.write(y)

f.close()

f= open(markov_duration_file,"w+")

coolString = np.array2string(markov_duration,threshold = np.sys.maxsize,separator=',')
x = coolString.replace('[','{')
y = x.replace(']','}')
f.write(y)

f.close()
    


processed_data_df = pd.DataFrame(processed_data)

processed_data_df.to_csv(outFile,index = False)





