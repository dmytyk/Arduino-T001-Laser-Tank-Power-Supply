# Arduino-T001-LaserTankPowerSupply

### Laser Tank Power Supply Tutorial
This is tutorial 1 for my 5.5w Laser Tank (LT).  It's primary focus is to monitor the +11.1v LiPo Battery.  The Lipo battery says it is +11.1v however during recharging the actual charge voltage can be above +11.1v (could be as high as +12.6v for my particular battery).
Normal use of the battery causes it to degrade over time, to prevent further degradation we don't want the battery to discharge to low.  So I set the Low Voltage Cutoff (LVC) to be 10.5v. 10.5v is well above the danger zone of 3.0v per cell and allows for some variance of the three cells that make up the battery.  There are several guides to understanding Lipo batteries [Lipo Guide](http://learningrc.com/lipo-battery/) so I won't go into a lot of detail regarding their usage.

The tutorial shows code using a Software Interrupt with an  Interrupt Service Routine (ISR), Background Processing and Battery Monitoring, we are only going to detail the Battery Monitoring code.  I will cover the other areas in future tutorials (these will help manage and control other parts to the LT). I will be adding multiple sensors and devices to the LT, so I wanted to ensure various processes including the Battery Monitoring are only using CPU time only as needed.

> #### Power Supply Requirements 
> - +10.5vdc to +12vdc for the 5.5w Laser
> - +9vdc for the left and right Drive Motors
> - +5vdc for the Arduino(s) and other Control and logic Boards & Components
> - 0 to +3.3vdc for the input to the MKR1010 A6 pin, used to calculate current and average battery voltage

![Power Supply Installed](/Images/Power_Supply_Installed.png)

![Power Supply Schematic](/Images/Power_Supply_Schematic.png)

The 5.5w Laser is feed directly from the battery, the Drive Motors and Control Boards get their supply voltage from two different voltage regulators.  Their respective circuits are pretty straight forward, therefore I'll move on to the voltage monitor section of the schematic.

The voltage monitor section has hardware and software related components.

### Voltage Monitor Hardware

![Voltage Mointor](/Images/Voltage_Monitor.png)

The hardware component is composed of a voltage divider circuit using R1 and R2. R2 is a 10K potentiometer that I adjust to get a starting voltage of +3.3v (maximum input for and MKR1010 A6 pin). Aligning the A6 input max of +3.3v with the supply voltage of +12.6v allows us to use one of the analog to digital converters on the MKR1010. Due to resistor tolerances you will have to adjust the R2 value ([Making Adjustments](https://www.youtube.com/watch?v=C3UCRNXDfCQ)) to achieve proper alignment.

### Voltage Monitor Software

The voltage monitor software takes a reading of the battery voltage ever 3 seconds (driven by the ISR and processed in the background).  An average of the last 10 readings is created and is use as the value to make the LVC decision.  If the LVC is reached a warning message is printed (in this tutorial for demonstration purposes, the working LT sends the message to the user via websockets, more on that in a future tutorial) and the LowVoltage Led is lit.

- Declare the variables we need, they include:
  - The Voltage Monitor pin - output voltage from voltage divider
  - The Low Voltage LED output pin
  - The Low Voltage Cutoff (LVC) value
  - Vars for reading, storing and calculating the battery voltage
  - ISR var used to tell the background it's time to do a status check 
```sh
#ifndef VoltageMonitorPin
    // A6 / D21
    #define VoltageMonitorPin  A6
#endif

#ifndef LowVoltagePin
    // D6 - Red LED
    #define LowVoltagePin  6
#endif

#ifndef LowVoltageCutoff
    // the value we want keep the system from starting and/or when to shutdown
    #define LowVoltageCutoff  10.5
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
- sendBatteryStatus() - function to read and process the battery voltage
  - Read the Battery Voltage - We defined our working voltage range to be +10.5v (LVC) to +12.6v (MAX). Using our voltage divider circuit the actual input to the VoltageMonitorPin will be in the range of: (approximately +x.xv) to +3.3v.  The analogRead() function is using a resolution of 12 bits, therefore the raw_read value could have a theoretical range of 0 t0 4095 (we will never get to close to 0 because of our LVC limit).  Since we know the MAX supply voltage is +12.6v, multiple the raw_read value by (float)12.6 and then divide by 4095 to get BatteryVoltage (the current reading).  Next we add the BatteryVoltage to the BatteryAverageBuild to keep a running total for this 30 second average.
  - Make the Average - once we have 10 samples we divide the BatteryAverageBuild by 10 to get the BatteryAverageFinal value for this 30-second period. The sample is reported, then checked to see if we are at or below the LVC if so report that and lite the Low Voltage LED.
```sh
void sendBatteryStatus()
{ 
    // Read the Battery Voltage
    raw_read = analogRead(VoltageMonitorPin);
    BatteryVoltage = (((float)raw_read) * 12.6 / 4095);
    BatteryAverageBuild += BatteryVoltage;

    // Make the Average
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
- Interrupt Service Routine, tells the background to take a sample of the battery voltage every 3 seconds
  - The software interrupt is configured to call the ISR once every 10 micro-seconds (10 * 300000 = 3,000,000 micro-seconds or 3 seconds).  Once we reach 3 seconds the ISR is done until the background takes the sample and resets the ISR counter, so it can start over.  Why a 10 micro second interrupt? Why not a 1-second interrupt and skip all the counting? I'll explain that in a future tutorial for the 5.5w Laser. Hack Shack Tip (HST) I like to share really cool/useful websites so here is one that does conversions for you [Unit Juggler](https://www.unitjuggler.com/convert-frequency-from-%C2%B5s(p)-to-s(p).html?val=3000000).
```sh
// Interrupt Service Routine
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
- setup(), required Arduino code block
  - Set up the Low Voltage Pin - configure the Low Voltage warning LED
  - Get the Initial Battery Status - since we are dealing with a small voltage range (+10.5v to +12.6v) I increased the analogReadResolution() to 12 bits from the default of 10 bits (this will increase the accuracy of the samples), next take a sample of the start up battery voltage, if it is too low lite the Low Voltage warning LED and wait.  If it is good (above the LVC) ensure the Low Voltage warning LED is off and set the initial BatteryAverageFinal to be equal to the current BatteryVoltage, so we have a value until a full 30-second sample is available.
  - delay 10 seconds - add a delay, so we don't flood console with error messages
```sh
void setup()
{
    // Setup the Low Voltage Pin
    pinMode(LowVoltagePin, OUTPUT);
    digitalWrite(LowVoltagePin, LOW);

    // Get the Initial Battery Status so we can preset the battery average until we have one (every 30 seconds)
    // also check and make sure we are good to go else set the Low Voltage LED and wait
    analogReadResolution(12);
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
      
      // delay 10 seconds, remember we have to call sendBatteryStatus() 10 times to get the average so 1000 msec * 10 = 10 seconds
      delay(1000);
    }  
}
```
- loop(), required Arduino code block
  - Background Process 1 - Wait for the ISR_BatteryVoltage to reach 300000 (3 seconds), when it does it we reset ISR_BatteryVoltage counter, so the ISR will start counting again, then call the function to check the battery status
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

## Next Up
> - Tutorials on Software Interrupts, Background Processing, Board to Board communication, Pulse Simulation, Website using Base64 to save code space and other Fun Stuff

Follow me on [YouTube](https://www.youtube.com/channel/UClwcP7ByE6Ia9DmKfP0C-UQ) to catch the "Next Up" stuff and thanks for Hanging Out at the Shack!
