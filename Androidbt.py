# file: rfcomm-server.py
# auth: Albert Huang <albert@csail.mit.edu>
# desc: simple demonstration of a server application that uses RFCOMM sockets
#
# $Id: rfcomm-server.py 518 2007-08-10 07:20:07Z albert $

from bluetooth import *


def socketfunc():
        server_sock=BluetoothSocket( RFCOMM )
        server_sock.bind(("",10))
        server_sock.listen(1)

        port = server_sock.getsockname()[1]

        uuid = "94f39d29-7d6d-437d-973b-fba39e49d4ee"
        advertise_service( server_sock, "SampleServer",
                   service_id = uuid,
                   service_classes = [ uuid, SERIAL_PORT_CLASS ],
                   profiles = [ SERIAL_PORT_PROFILE ],
#                   protocols = [ OBEX_UUID ]
                    )

        print "Waiting for connection on RFCOMM channel %d" % port

        client_sock, client_info = server_sock.accept()
        print "Accepted connection from ", client_info

        try:
                while True:

                        data = client_sock.recv(1024)
                        print "received [%s]" % data
                        

                        if(data == 'A'):
                                client_sock.send('Ap config mode')
                                file = open('status_memory.txt','w')
                                file.write(data)
                                print "file write..."
                                file.close()
                                print "file close"

                        if(data == 'B'):
                                client_sock.send('WiFi UPnP mode')
                                file = open('status_memory.txt','w')
                                file.write(data)
                                print "file write..."
                                file.close()
                                print "file close"
                        if(data == 'C'):
                               client_sock.send('WiFi Direct UPnP mode')
                               file = open('status_memory.txt','w')
                               file.write(data)
                               print "file write..."
                               file.close()
                               print "file close"
                        if(data == 'D'):
                               client_sock.send('InternetRadio mode')
                               file = open('status_memory.txt','w')
                               file.write(data)
                               print "file write..."
                               file.close()
                               print "file close"

                        if(data == 'E'):
                               client_sock.send('FM Radio mode')
                               file = open('status_memory.txt','w')
                               file.write(data)
                               print "file write..."
                               file.close()
                               print "file close"

                   

                        if(data[0] == 'S'):
                                file = open('channel.txt','w')
                                file.write(data)
                                print "file write..."
                                file.close()
                                print "file close"
                        if(data == 'Sl'):
                                print "left chanel"
								os.system('sudo pkill -f "python re_slavenew.py"')
                                os.system("sudo killall -9 Slave/Slave/slavenew0817")
                                os.system("amixer set Speaker 100,0")
                                #os.system("sudo /etc/init.d/networking restart")
                                os.system("sudo python re_slavenew.py&")
                        if(data == 'Sr'):
                                print "Right chanel"
                                os.system("amixer set Speaker 100,0")
								os.system('sudo pkill -f "python re_slavenew.py"')
								os.system("sudo killall -9 Slave/Slave/slavenew0817")
                                #os.system("sudo /etc/init.d/networking restart")
                                os.system("sudo python re_slavenew.py&")
                        if(data == 'Ss'):
                                print "chanel"
                                os.system("amixer set Speaker 100,100")
                                os.system('sudo pkill -f "python re_slavenew.py"')
								os.system("sudo killall -9 Slave/Slave/slavenew0817")
                                #os.system("sudo /etc/init.d/networking restart")
                                os.system("sudo python re_slavenew.py&")
                        if(data == 'Rr'):
                                os.system("sudo /etc/init.d/networking restart")

                        if len(data) == 0: break
                        if data == 'q': break


 			if(data[0] == 'R'):
                                file = open('InternetRadio_status.txt','w')
                                file.write(data)
                                print "file write..."
                                file.close()
                                print "file close"
                    

                        if(data[0] == 'V'):
                                file = open('volume.txt','w')
                                file.write(data)
                                file.close()
                                print "file close"
                         




        except IOError:
                pass

        print "disconnected"

        client_sock.close()
        server_sock.close()
        print "all done"

while True:
        socketfunc()

