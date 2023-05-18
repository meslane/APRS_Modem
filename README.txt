  _       _     _ _ _     _      _     _    _     _ _ _
/\_\    /\_\  /\_\_\_\  /\_\ _ /\_\  /\_\ /\_\  /\_\_\_\
\/\_\   \ \_\ \/\_\/_/_ \/\_\_\\_\_\ \/\_\__\_\ \/\_\/\_\
 \/\_\  _\ \_\ \/\_\_\_\ \/\_\//\_\_\ \/_/_\_\/_ \/\_\_\_\
  \/\_\/\_\_\_\ \/\_\/\_\ \/\_\\/_/\_\  /\ \_/\ \ \/\_\/_/
   \/\_\_\_\_\_\ \/\_\_\_\ \/\_\  \/\_\ \/\_\\/\_\ \/\_\ 
    \/_/_/_/_/_/  \/_/_/_/  \/_/   \/_/  \/_/ \/_/  \/_/
    
 ##   ###   ###    ###    #   #   ##   ###   ####  #   #  #
#  #  #  #  #  #  #       ## ##  #  #  #  #  #     ## ##  #
####  ###   ###    ##     # # #  #  #  #  #  ####  # # #  #
#  #  #     # #      #    #   #  #  #  #  #  #     #   #  
#  #  #     #  #  ###     #   #   ##   ###   ####  #   #  #

~== Current Functionality ==~
-Encoding and transmission of arbitrary packets over resistor DAC
-Automatic radio push-to-talk activation
-GPS module support
-LCD + rotart encoder user interface
-Persistent settings
-TX jitter setting
-RX capabilites

~== Planned Functionality ==~
-RX Decoding
-PCB
-3D-printed enclosure

~== Current Bugs (TOFIX) ==~
-Occasional false positives happen where a start flag is detected and the RX routine hangs until message buffer overflow
-Start flags are getting appended to the packet

~== Fixed Bugs ==~
-Decoding would fail unless a print statement was in the dsp isr
	-Fixed by slightly lengthening the sampling period
-Whether or not a packet is decoded by multimon seems to be a function of its contents
	-Was caused by bit misalignment in end flag, fixed by refactoring packet code
-First two APRS transmissions are not properly formatted (transmits nothing, then transmits twice)
    -Fixed by initializing relevant variables differently

~== Acknowlegements ==~
-Cal EE123 (packet formatting info): https://inst.eecs.berkeley.edu/~ee123/sp15/lab/lab6/Lab6_Part_B-APRS.html
-Matthew Tran (verification code): https://matthewtran.dev/
-UCR CS120B for LCD code
-Mark Adler on StackOverflow for CRC function code
