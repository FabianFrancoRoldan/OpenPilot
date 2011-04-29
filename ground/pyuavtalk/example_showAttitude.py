import logging
import serial
import traceback

from openpilot.uavtalk.uavobject import *
from openpilot.uavtalk.uavtalk import *
from openpilot.uavtalk.objectManager import *
from openpilot.uavtalk.connectionManager import *
    
    
def _hex02(value):
    return "%02X" % value
    
class UavtalkDemo():
    
    UAVOBJDEF_PATH = "D:\\Projects\\Fred\\OpenPilot\\git\\build\\uavobject-synthetics\\python"
    PORT = (30-1)
    
    METHOD_OBSERVER = 1
    METHOD_WAIT = 2
    METHOD_GET = 3
    
    METHOD = METHOD_WAIT
    
    def __init__(self):
        try:
            self.nbUpdates = 0
            
            print "Opening Port"
            serPort = serial.Serial(UavtalkDemo.PORT, 57600, timeout=.5)
            if not serPort.isOpen():
                raise IOError("Failed to open serial port")
            
            print "Creating UavTalk"
            self.uavTalk = UavTalk(serPort)
            
            print "Starting ObjectManager"
            self.objMan = ObjManager(self.uavTalk, UavtalkDemo.UAVOBJDEF_PATH)
            
            print "Starting UavTalk"
            self.uavTalk.start()
            
            print "Starting ConnectionManager"
            self.connMan = ConnectionManager(self.uavTalk, self.objMan)
            
            print "Connecting...",
            self.connMan.connect()
            print "Connected"
            
            print "Getting all Data"
            self.objMan.requestAllObjUpdate()
            
            print "SN:",
            sn = self.objMan.FirmwareIAPObj.CPUSerial.value
            print "".join(map(_hex02, sn))
            
            print "Current updatePeriod for AttitudeActual is",
            print self.objMan.AttitudeActual.metadata.telemetryUpdatePeriod.value
            
            if UavtalkDemo.METHOD == UavtalkDemo.METHOD_OBSERVER or UavtalkDemo.METHOD == UavtalkDemo.METHOD_WAIT:            
                print "Request fast periodic updates for AttitudeActual"
                self.objMan.AttitudeActual.metadata.telemetryUpdateMode.value = UAVMetaDataObject.UpdateMode.PERIODIC
                self.objMan.AttitudeActual.metadata.telemetryUpdatePeriod.value = 50
                self.objMan.AttitudeActual.metadata.updated()
              
              
            if UavtalkDemo.METHOD == UavtalkDemo.METHOD_OBSERVER:
                print "Install Observer for AttitudeActual updates\n"
                self.objMan.regObjectObserver(self.objMan.AttitudeActual, self, "_onAttitudeUpdate")
                # Spin until we get interrupted
                while True:
                    time.sleep(1)
                    
            elif UavtalkDemo.METHOD == UavtalkDemo.METHOD_WAIT:
                while True:
                    self.objMan.AttitudeActual.waitUpdate()
                    self._onAttitudeUpdate(self.objMan.AttitudeActual)
            
            elif UavtalkDemo.METHOD == UavtalkDemo.METHOD_GET:
                while True:
                    self.objMan.AttitudeActual.getUpdate()
                    self._onAttitudeUpdate(self.objMan.AttitudeActual)
                
                
        except KeyboardInterrupt:
            pass
        except Exception,e:
            print
            print "An error occured: ", e
            print
            traceback.print_exc()
        
        print "Stopping UavTalk"
        self.uavTalk.stop()
        raw_input("Press ENTER, the application will close")

        
    def _onAttitudeUpdate(self, args):      
        print "."*self.nbUpdates+" "*(10-self.nbUpdates),
        self.nbUpdates += 1
        if self.nbUpdates > 10:
            self.nbUpdates = 0
            
        roll = self.objMan.AttitudeActual.Roll.value
        print "Roll: %-4d " % roll,
        i = roll/90
        if i<-1: i=-1
        if i>1: i= 1
        i = int((i+1)*15)
        print "-"*i+"*"+"-"*(30-i)+" \r",
        

if __name__ == '__main__':
    
    # Log everything, and send it to stderr.
    logging.basicConfig(level=logging.INFO)
    
    UavtalkDemo()