
import time
import os
while True:
   os.system("sudo python Androidbt.py")
   os.system('sudo pkill -f "python re_slavenew.py"')
   os.system("sudo killall -9 Slave/Slave/slavenew0817")
   os.system("sudo python re_slavenew.py&")