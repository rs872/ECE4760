import py_midicsv as pm
import os


curr_path = os.path.dirname(os.path.abspath(__file__))
midiFile = os.path.join(curr_path, "")


# Load the MIDI file and parse it into CSV format
csv_string = pm.midi_to_csv("hipHopSample.mid")

with open("example_converted.csv", "w") as f:
    f.writelines(csv_string)
