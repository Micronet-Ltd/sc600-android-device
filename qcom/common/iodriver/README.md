# IO Driver

The IO driver is resposible for managing communication between the MCU and the
Android or another Linux system.


## Components

### USB Communication

The USB communication is presented to the Linux system as a /dev/ttyACM device. These
devices do not have a real baud rate.

#### Main communication channel

The main communication channel communicates the majority of the messages, however not
the majaority of the communication will be on this channel.

#### Accelerometer channel

The accelerometer data has it's own ACM device. The data consists of a 64bit big endian
timestamp (unsigned?) in miliseconds, and a 64bit unsigned little endian XYZ data.
This data is encapsulated inside a frame. This results in a payload of 16 bytes, any
additional data is ignored and reserved for future use (such as gyroscope data?). The
Gryoscope data will probably be its own channel. Data is sent on this channel by the
MCU only after it has been commanded to do so (possibly only by intial communication
with the iodriver process, _not explicitly_.)

The format of the XYZ data is the same format the the accelerometer.

The timestamp is a monotonic timestamp.



