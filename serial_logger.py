import serial
import sys
import datetime
import os

def timeGetter():
    "calculates the date and time, and mushes it nicely into a string"
    d = datetime.datetime.now()    # get it!
    currYear = (d.year) % 100      # prettify it!
    currMonth = "%02d" % (d.month)
    currDay = "%02d" % (d.day)
    currHour = "%02d" % (d.hour)
    currMins = "%02d" % (d.minute)
    totalDate = str(currMonth) + "-" + str(currDay) + "-" + str(currYear) + "_" + str(currHour) + ":" + str(currMins)   # mush it!
    return totalDate;

ser = serial.Serial('/dev/ttyUSB0',115200) # open up our serial port at USB0 with that baud rate

sys.stdout.write("Serial-logging system starting. Wave your hand over the sensor to start. You may need to press the reset/reboot button first.\n")
sys.stdout.write("To close/save the current file and restart testing, press the reset/reboot button\n")
x = 'k'     # make x equal to something random so it doesn't complain

while(True):
    while (x != '$'):           # don't write to file until we start successfully. '$' character will appear
        x = ser.read()

    # first calculate the date for the filename
    sys.stdout.write("\nCreating and opening log file:  \n")
    timeGot = timeGetter();
    filename = "captest_log_" + timeGot + ".txt"

    # open the file
    sys.stdout.write("System starting. \n")
    with open(filename, 'w') as log:
        print log
        while (x != '*'):        # we recieve '*' character when we want to stop recording. otherwise, keep looping
            x = ser.read()       # read from serial
            if(x == '^'):    # we receive '^' character when we hit an error. print out a debugging time.
                timeGot = timeGetter();
                sys.stdout.write("\nThe time is: \n")
                sys.stdout.write(timeGot)
            elif(x == '&'):
                timeGot = timeGetter();
                sys.stdout.write("\nTIME: ")
                sys.stdout.write(timeGot)
            else:
                log.write(x)        # write to screen + write to log file
                sys.stdout.write(x)
                sys.stdout.flush()
                log.flush()         # save the stuff in the file!
                os.fsync(log.fileno())
    # if we recieve the special character and exit the writing loop, inform the user + print the time
    sys.stdout.write("System restart. Log file saved and closed. Wave your hand over the sensor to start another file log \n")
    timeGot = timeGetter();
    sys.stdout.write("The time is: ")
    sys.stdout.write(timeGot)
    log.close()    # close the log, wait for a restart.
