This is Kevin Guo's (hopefully) helpful end-of-week writeup on how far he got on the Captester!
Plus a little mini-guide of sorts. 

Status + Features:
There's a Nordic Semiconductor dev board, and a Raspberry Pi. The dev board's serial output should be plugged into the bottom USB port in the Pi.
The Nordic Semiconductor board directly controls the capacitors, and checks for LED errors with light sensors (which Emanuele made). This runs the captester.c code and generates test cases + tests for erroneous output. 

This captester features semi-random testing sequences, with the range of generation determined by the a whole bunch of defines in the code at the top of the captester.c file. 

You can change the capacitor on-time and off-time generation, how many repetitions we go through for each generated set of numbers, the delay between rounds of testing (where we check for errors in output), as well as a "randomness" factor that adds/subtracts time from on/off times in repetitions within a single round. All the ranges for these generations are defined, and you can change them.

In addition, at the beginning, when you wave your hand in front of the hygiene sensor to start, it uses this time delay to generate a random seed. If the program finds an error and you want to try duplicating it, you can find the saved random seed from whatever sequence of testing you want to duplicate, and then write it in the set_seed define, which is set to 0 by default for random generation. 

The dev board generates serial output, which is fed to the Raspberry Pi to write into a log file in the SD card. The Raspberry Pi requires no input or external display. As soon as you plug the Pi into power, it will boot up, automatically log in, and start the logging process. Every time it starts a new file, it will name it with a timestamp. You can press the reset/reboot button to restart the current testing sequence, and close/save the current file. 

I didn't get radio stuff working :(. But for more information, you can go look at my comments!
