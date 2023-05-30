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
-RX capabilites + decoding + CRC checking
-Python TX/RX script

~== Tips and Tricks ==~
-Make sure you're not over/under-driving the output of the transmit circuit.
 Over/under modulation can cause a reciever to fail to properly decode packets.
 You should test this with a second radio and the python demodulation script included here
-Also check reciever volume into your PC for the same reasons
 The demodulator is more resistant to distortion than noise, but it can't hurt

~== Current Bugs (TOFIX) ==~
-AGC on the reciever looks like it may require a longer start/stop flag to lock on
	-Will need to test further though

~== Acknowlegements ==~
-Cal EE123 (packet formatting info): https://inst.eecs.berkeley.edu/~ee123/sp15/lab/lab6/Lab6_Part_B-APRS.html
-Matthew Tran (verification code): https://matthewtran.dev/
-UCR CS120B for LCD code
-Mark Adler on StackOverflow for CRC function code
