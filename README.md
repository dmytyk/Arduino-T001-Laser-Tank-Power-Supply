# Arduino-T001-LaserTankPowerSupply

### Laser Tank Power Supply Tutorial
This is tutorial 1 for my 5.5w Laser Tank (LT).  It's primary focus is to monitor the 11.1v LiPo Battery.  Normal use of the battery causes it to degrade over time, to prevent further degradation we don't want the battery to discharge to low.  So I set the Low Voltage Cutoff (LVC) to be 10V. 10V is well above the danger zone of 3.0v per cell and allows for some variance of the three cells that make up the battery.  There are several guides to understanding Lipo batteries [Lipo Guide](http://learningrc.com/lipo-battery/) so I won't go into a lot of detail regarding their usage.

The tutorial shows code using a Software Interrupt, an Interrupt Service Routine (ISR) and Background Processing as part of the battery monitoring.  I will cover these in future tutorials, so I will not explain their operation at time (these will help manage and control other parts to the LT). I will be adding multiple sensors and devices to the LT, so I wanted to ensure various processes are only using CPU time only as needed.

> #### Power Supply Requirements 
> - +10vdc to +12vdc for the 5.5w Laser
> - +9vdc for the left and right Drive Motors
> - +5vdc for the Arduino(s) and other Control and logic Boards and components
> - 0 to +3.3vdc for the input to the MKR1010 A6 pin, so we can calculate current and average battery voltage

![Power Supply Installed](/Images/Power_Supply_Installed.png)

![Power Supply Schematic](/Images/Power_Supply_Schematic.png)

The 5.5w Laser is feed directly from the battery, the Drive Motors and Control Boards get their supply voltage from two different voltage regulators.  Their respective circuits are pretty straight forward, therefore I'll move on to the voltage monitor section of the schematic.

The voltage monitor section has hardware and software related components.

### Voltage Monitor Hardware

![Voltage Mointor](/Images/Voltage_Monitor.png)

The hardware component is composed of a voltage divider circuit using R1 and R2. R2 is a 10K potentiometer that I adjust to get a starting voltage of +3.3v (maximum input for and MKR1010 A6 pin). Due to resistor tolerances you may have to adjust yours ([Making Adjustments](https://www.youtube.com/channel/UClwcP7ByE6Ia9DmKfP0C-UQ)).  D1 is a 3.3v zener diode is used to ensure we do not overload the MKR1010's input. Why do I need D1? The Lipo battery says it is 11.1v however during recharging the actual charge voltage can be above 11.1 (could be as high as 12.6v for my particular battery). If you are willing to monitor the recharging process and stop it at 11.1v or adjust R2 every time to ensure you do not exceed +3.3v D1 is not needed, however that is not practical and poses an unnecessary risk to the MKR1010.

### Voltage Monitor Software

The voltage monitor software allows the LT to make a reading of the battery voltage ever 3 seconds (driven by the ISR and processed in the background).  An average of the last 10 readings is created and is use as the value to make the LVC decision.  If the LVC is reached a warning message is printer (in this tutorial, the actual LT sends the message to the user via websockets, more on that in a future tutorial) and LowVoltage Led is lit.

- Declare the variables we need, these include:
  - The Voltage Monitor pin - output voltage from voltage divider
  - The Low Voltage LED output pin
  - The Low Voltage Cutoff (LVC)  
  - Vars for reading, storing and calculating the battery voltage
  - ISR var used to tell the background it's time to do a status check 
```sh
#ifndef VoltageMonitorPin
    // A6 / D21
    #define VoltageMonitorPin  A6
#endif

#ifndef LowVoltagePin
    // D8 - Red LED
    #define LowVoltagePin  8
#endif

#ifndef LowVoltageCutoff
    // the value we want keep the system from starting and/or when to shutdown
    #define LowVoltageCutoff  10
#endif

// Battery
short raw_read;
byte BatteryAverageCount = 0;
float BatteryVoltage = 0;
float BatteryAverageBuild = 0;
float BatteryAverageFinal = 0;

// ISR vars
volatile int ISR_BatteryVoltage = 0;
```
- Function to read and process the battery voltage
  - Read the Battery Voltage - the input to the VoltageMonitorPin should be in the range of: 0 - +3.3v [ our working range should be : +x.xv (+10v or LVC) and +3.3v (+11.1v or MAX) ].  The analogRead is using the default resolution of 10 bits, therefore the raw_read value should be between 0 and 1023.  Since we know the target supply voltage is 11.1, multiple the raw_read value by (float)11.1 and then divide by 1023 to get BatteryVoltage (the current reading).  Next we add the BatteryVoltage to the BatteryAverageBuild to keep a running total for this 30 second average.
  - Make the Average - once we get to 10 samples and then divide the BatteryAverageBuild by 10 to get the BatteryAverageFinal for this 30-second period.  The sample is reported, and then checked to see if we are +10v or below and if so report that too.  Finally, if below +10v lite the Low Voltage LED.
```sh
void sendBatteryStatus()
{ 
    // read the Battery Voltage
    raw_read = analogRead(VoltageMonitorPin);
    BatteryVoltage = (((float)raw_read) * 11.1 / 1023);
    BatteryAverageBuild += BatteryVoltage;

    // make the Average
    // average is the last 10 samples
    BatteryAverageCount++;
    if(BatteryAverageCount == 10) {
        BatteryAverageFinal = (BatteryAverageBuild / 10);
        BatteryAverageBuild = BatteryAverageCount = 0;
      
        // send it to the client
        Serial.println("S:C = " + String(BatteryVoltage) + "v, A = " + String(BatteryAverageFinal) + "v, Raw = " + String(raw_read));

        // send an error message if the battery is below the error threshold
        if(BatteryAverageFinal <= LowVoltageCutoff) {
            // turn on Low Battery Light
            digitalWrite(LowVoltagePin, HIGH);            
            Serial.println("E:LOW BATTERY, Please Shout Down Now!");
        } else {
            // turn off Low Battery Light
            digitalWrite(LowVoltagePin, LOW);  
        }
    }
}
```
- ISR, tells the background to take a sample of the battery voltage every 3 seconds
  - The software interrupt is configured to call the ISR once every 10 micro-seconds (10 * 300000 = 3,000,000 or 3 seconds).  Once we reach 3 seconds the ISR is done until the background takes the sample and resets the ISR counter, so it can start over.  Why so fast, I'll explain in a future tutorial for the 5.5w Laser.  I like to share really cool/useful websites so here is one that does conversions for you [Unit Juggler](https://www.unitjuggler.com/convert-frequency-from-%C2%B5s(p)-to-s(p).html?val=3000000).
```sh
// interrupt service routine
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
```
- setup, required Arduino code block
  - Setup the Low Voltage Pin - configure the Low Voltage warning LED
  - Get the Initial Battery Status - take a sample of the start up battery voltage, if it is too low lite the warning LED and wait.  If it is good ensure the warning LED is off and set the initial BatteryAverageFinal to be equal to the current BatteryVoltage, so we have a value until a full 30-second sample is available
```sh
void setup()
{
    // Setup the Low Voltage Pin
    pinMode(LowVoltagePin, OUTPUT);
    digitalWrite(LowVoltagePin, LOW);

    // Get the Initial Battery Status so we can preset the battery average until we have one (every 30 seconds)
    // also check and make sure we are good to go else set the Low Voltage LED and wait
    while(BatteryAverageFinal <= LowVoltageCutoff) {
      sendBatteryStatus();

      // check to make sure we got enough battery to continue
      if(BatteryVoltage <= LowVoltageCutoff) {
        // turn on Low Battery Light and do nothing else
        digitalWrite(LowVoltagePin, HIGH);
      } else {
          BatteryAverageFinal = BatteryVoltage;
          digitalWrite(LowVoltagePin, LOW);         
      }
    }  
}
```
- loop, required Arduino code block
  - Background Process 1 - The code segment waits for the ISR_BatteryVoltage to reach 300000 (3 seconds), when it does it calls  the function to check the battery
```sh
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