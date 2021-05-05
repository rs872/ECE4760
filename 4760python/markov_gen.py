import numpy as np
import os


curr_path = os.path.dirname(os.path.abspath(__file__))

print(curr_path)

markovNoteFile = os.path.join(curr_path, "MarkovNote.txt")
markovDurationFile = os.path.join(curr_path, "MarkovDuration.txt")


markov_note = np.full((13,13,13,13),255)
markov_duration = np.full((8,8,8,8),255)

f= open(markovNoteFile,"w+")

coolString = np.array2string(markov_note,threshold = np.sys.maxsize,separator=',')
x = coolString.replace('[','{')
y = x.replace(']','}')
f.write(y)

f.close()

# f= open(markovDurationFile,"w+")

# coolstring = np.array2string(markov_duration,threshold = np.sys.maxsize,separator=',')
# x = coolString.replace('[','{')
# y = x.replace(']','}')

# f.write(y)

# f.close()


# //Populate Markov Chains

# for i in range(0,8):
#     for j 

# int i;
# for (i = 0; i < 8; i++){
#     markov_duration[i][0] = (unsigned char)rand() % 32;
#     printf("ROW # %d: [%d, ", i, markov_duration[i][0]);
#     int j;
#     for (j = 1; j < 7; j++){
#         markov_duration[i][j] = markov_duration[i][j-1] + (unsigned char)rand() % 32;
#         printf("%d, ", markov_duration[i][j]);

#     }
#     markov_duration[i][7] = 255;
#     printf("%d ", markov_duration[i][7]);
#     printf("]\n");
# }


# for (i = 0; i < 56; i++){
#     int j;
#     for (j = 0; j < 56; j++){
#         int k;
#         for(k=0;k < 56; k++){
#             markov_notes[i][j][k] = (unsigned char)(rand() % 255);
#             printf("%d, ", markov_notes[i][j][k]);
#         }

#     }
#     printf("]\n");
# }
