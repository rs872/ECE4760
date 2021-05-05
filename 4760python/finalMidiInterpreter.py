import py_midicsv as pm
import pandas as pd
import os


curr_path = os.path.dirname(os.path.abspath(__file__))
midiFile = os.path.join(curr_path, "test2.mid")
outFile = os.path.join(curr_path, "output.csv")


# Load the MIDI file and parse it into CSV format
csv_string = pm.midi_to_csv(midiFile)

with open(outFile, "w") as f:
    f.writelines(csv_string)
    f.close()



endMidi = pd.read_csv(outFile,error_bad_lines=False)

beatspertick = endMidi.iloc[1,5]
print(beatspertick)

#endMidi.drop(endMidi.columns[[0,3,5]], axis=1,inplace = True)

#endMidi.to_csv(outFile,index = False)
