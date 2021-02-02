# Arduino-T001-LaserTankPowerSupply

### Laser Tank Power Supply Tutorial
This is tutorial 1 for my 20W Laser Tank.  It's primary focus is to monitor the 11.1v LiPo Battery.  Normal use of the battery causes it to degrade over time, to prevent further degradation we don't want the battery to discharge to low.  So I set the Low Voltage Cutoff (LVC) to be 10V. 10V is well above the danger zone of 3.0v per cell and allows for some variance of the three cells that make up the battery.  There are several guides to understanding Lipo batteries [Lipo Guide](http://learningrc.com/lipo-battery/) so I won't go into a lot of detail.

The tutorial also covers setting up a software interrupt, Interrupt Service Routine (ISR) and background processing as part of the battery monitoring.  These will also help manage and control other parts of the Laser Tank. 

> #### Power Supply Requirements 
> - +10vdc to +11vdc for the 20W Laser
> - +9vdc for the left and right Drive Motors
> - +5vdc for the Arduino(s) and other Control and logic Boards and components
> - 0 to +3.3vdc for the input to the MKR1010 A6 pin, so we can calculate current and average battery voltage

![Power Supply Installed](/Images/Power_Supply_Installed.png)

![Power Supply Schematic](/Images/Power_Supply_Schematic.png)

The Laser is feed directly from the battery, the Drive Motors and Control Boards get their supply voltage from two different voltage regulators.  Their respective circuits are pretty straight forward, therefore I'll move on to the voltage monitor section of the schematic.

The voltage monitor section has both hardware and software related areas.

- Hardware:

![Voltage Mointor](/Images/Voltage_Monitor.png)

- Software
```sh

#ifndef LowVoltagePin
    // A2/D17 - Red LED
    #define LowVoltagePin  17
#endif

// Battery
short raw_read;
byte BatteryAverageCount = 0;
float BatteryVoltage = 0;
float BatteryAverageBuild = 0;
float BatteryAverageFinal = 0;

// ISR vars
volatile int ISR_BatteryVoltage = 0;

void sendBatteryStatus()
{ 
    // read the Battery Voltage
    raw_read = analogRead(A6);
    BatteryVoltage = (((float)raw_read) * 11.1 / 1023);
    BatteryAverageBuild += BatteryVoltage;

    // make the average
    // average is the last 10 samples
    BatteryAverageCount++;
    if(BatteryAverageCount == 10) {
        BatteryAverageFinal = (BatteryAverageBuild / 10);
        BatteryAverageBuild = BatteryAverageCount = 0;
      
        // send it to the client
        Serial.println("S:C = " + String(BatteryVoltage) + "v, A = " + String(BatteryAverageFinal) + "v, Raw = " + String(raw_read));

        // send an error message if the battery is below the error threshold
        if(BatteryAverageFinal < 10.1) {
            // turn on Low Battery Light
            digitalWrite(LowVoltagePin, HIGH);            
            Serial.println("E:LOW BATTERY, Please Shout Down Now!");
        } else {
            // turn off Low Battery Light
            digitalWrite(LowVoltagePin, LOW);  
        }
    }
}

// Interrupt Service Routine (ISR)
void TC4_Handler()
{
  if (TC4->COUNT16.INTFLAG.bit.OVF && TC4->COUNT16.INTENSET.bit.OVF)
  {
      // see if it is time to send the battery status - every 3 seconds
      // if so tell the background to do it
      if(ISR_BatteryVoltage < 300000) {
        ISR_BatteryVoltage++;
      }
  }
}

void setup()
{
    // setup the low voltage pin
    pinMode(LowVoltagePin, OUTPUT);
    digitalWrite(LowVoltagePin, LOW);

    // get the initial battery status so we can preset the battery average until we have one (every 30 seconds)
    while(BatteryAverageFinal < 10.1) {
      sendBatteryStatus();

      // check to make sure we got enough battery to continue
      if(BatteryVoltage < 10.1) {
        // turn on Low Battery Light and do nothing else
        digitalWrite(LowVoltagePin, HIGH);
      } else {
          BatteryAverageFinal = BatteryVoltage;
          digitalWrite(LowVoltagePin, LOW);         
      }
    }  
}

void loop()
{
  // Background Process 1
  // see if it's time to send the battery status
  // we get battery status every 3 seconds
  // the sendBatteryStatus() function will send a status update every 10 times we call it so we send the status every 30 seconds (3sec x 10 = 30 seconds)
  if(ISR_BatteryVoltage == 300000) {
    ISR_BatteryVoltage = 0;
    sendBatteryStatus();
  }
}
```

