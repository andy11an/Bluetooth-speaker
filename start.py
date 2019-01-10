
import time
import os
time.sleep(5)
os.system("sudo service bluetooth restart")
time.sleep(4)
os.system("sudo hciconfig hci0 piscan")
os.system("sudo python re_slavenew.py&")
os.system("sudo python whileandroid.py")