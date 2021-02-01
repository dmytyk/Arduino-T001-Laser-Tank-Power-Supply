# Arduino-T001-LaserTankPowerSupply

### Laser Tank Power Supply Tutorial
This is tutorial 001 for my 20W Laser Tank.  It's primary focus is to monitor the 11.1v LiPo Battery.  Normal use of the battery causes it to degrade over time, to prevent degradation we don't want the battery to discharge to low.  So I set the Low Voltage Cutoff (LVC) to be 10V. 10V is well above the danger zone of 3.0v per cell and allows for some variance of the three cells that make up the battery.  There are several guides to understanding Lipo batteries [Lipo Guide](http://learningrc.com/lipo-battery/) so I won't go into a lot of detail.

The tutorial also covers setting up a software interrupt, Interrupt Service Routine (ISR) and background processing as part of the battery monitoring.  These will also help manage and control other parts of the Laser Tank. 

![Power Supply Mounted](/Images/Power_Supply_Mounted.JPG)