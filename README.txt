 ##   ###   ###    ###
#  #  #  #  #  #  #
####  ###   ###    ##
#  #  #     # #      #
#  #  #     #  #  ###

Current Functionality:
-Encoding and transmission of arbitrary packets over resistor DAC

Planned Functionality:
-GPS module support
-RX capabilities

Current Bugs (TOFIX):
-Whether or not a packet is decoded by multimon seems to be a function of its contents
	-EX: a repeater list of "WIDE1-1,WIDE2-2" works pretty well, but "WIDE1-1,WIDE2-2,RELAY-0" is never decoded
	-Could be caused by bad flags? Packet contents match with the outputs of some prior art.

Fixed Bugs:
-Decoding would fail unless a print statement was in the dsp isr
	-Fixed by slightly lengthening the sampling period

Acknowlegements:
-Cal EE123: https://inst.eecs.berkeley.edu/~ee123/sp15/lab/lab6/Lab6_Part_B-APRS.html
-Matthew Tran: https://matthewtran.dev/