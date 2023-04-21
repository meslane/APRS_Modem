#   #   ###  #  #  #  #  ###
#   #  #     ## #  #  #  #  #
# # #  ###   # ##   ##   ###
# # #  #  #  #  #  #  #  #
 # #    ###  #  #  #  #  # 

 ##   ###   ###    ###    #   #   ##   ###   ####  #   #  #
#  #  #  #  #  #  #       ## ##  #  #  #  #  #     ## ##  #
####  ###   ###    ##     # # #  #  #  #  #  ####  # # #  #
#  #  #     # #      #    #   #  #  #  #  #  #     #   #  
#  #  #     #  #  ###     #   #   ##   ###   ####  #   #  #

~== Current Functionality ==~
-Encoding and transmission of arbitrary packets over resistor DAC
-Automatic radio push-to-talk activation
-GPS module support

~== Planned Functionality ==~
-RX capabilities

~== Current Bugs (TOFIX) ==~
-First two APRS transmissions are not properly formatted (transmits nothing, then transmits twice)
    -Probably an init issue

~== Fixed Bugs ==~
-Decoding would fail unless a print statement was in the dsp isr
	-Fixed by slightly lengthening the sampling period
-Whether or not a packet is decoded by multimon seems to be a function of its contents
	-Was caused by bit misalignment in end flag, fixed by refactoring packet code

~== Acknowlegements ==~
-Cal EE123 (packet formatting info): https://inst.eecs.berkeley.edu/~ee123/sp15/lab/lab6/Lab6_Part_B-APRS.html
-Matthew Tran (verification code): https://matthewtran.dev/
