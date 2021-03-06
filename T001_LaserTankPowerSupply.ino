
#ifndef VoltageMonitorPin
    // A6 / D21
    #define VoltageMonitorPin  A6
#endif

#ifndef LowVoltagePin
    // D7 - Red LED
    #define LowVoltagePin  7
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

void sendBatteryStatus()
{ 
    // read the Battery Voltage
    raw_read = analogRead(VoltageMonitorPin);
    BatteryVoltage = (((float)raw_read) * 12.6 / 4095);
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

// Start MKR1010 software interrupt functions **********
void setup_timer4(uint16_t clk_div_, uint8_t count_)
{
   // Set up the generic clock (GCLK4) used to clock timers
  REG_GCLK_GENDIV = GCLK_GENDIV_DIV(1) |          // Divide the 48MHz clock source by divisor 1: 48MHz/1=48MHz
                    GCLK_GENDIV_ID(4);            // Select Generic Clock (GCLK) 4
  while (GCLK->STATUS.bit.SYNCBUSY);              // Wait for synchronization

  REG_GCLK_GENCTRL = GCLK_GENCTRL_IDC |           // Set the duty cycle to 50/50 HIGH/LOW
                     GCLK_GENCTRL_GENEN |         // Enable GCLK4
                     GCLK_GENCTRL_SRC_DFLL48M |   // Set the 48MHz clock source
                     GCLK_GENCTRL_ID(4);          // Select GCLK4
  while (GCLK->STATUS.bit.SYNCBUSY);              // Wait for synchronization

  // Feed GCLK4 to TC4 and TC5
  REG_GCLK_CLKCTRL = GCLK_CLKCTRL_CLKEN |         // Enable GCLK4 to TC4 and TC5
                     GCLK_CLKCTRL_GEN_GCLK4 |     // Select GCLK4
                     GCLK_CLKCTRL_ID_TC4_TC5;     // Feed the GCLK4 to TC4 and TC5
  while (GCLK->STATUS.bit.SYNCBUSY);              // Wait for synchronization

  REG_TC4_CTRLA |= TC_CTRLA_MODE_COUNT8;          // Set the counter to 8-bit mode
  while (TC4->COUNT8.STATUS.bit.SYNCBUSY);        // Wait for synchronization

  REG_TC4_COUNT8_CC0 = count_;                    // Set the TC4 CC0 register to some arbitary value
  while (TC4->COUNT8.STATUS.bit.SYNCBUSY);        // Wait for synchronization

  NVIC_SetPriority(TC4_IRQn, 0);                  // Set the Nested Vector Interrupt Controller (NVIC) priority for TC4 to 0 (highest)
  NVIC_EnableIRQ(TC4_IRQn);                       // Connect TC4 to Nested Vector Interrupt Controller (NVIC)

  REG_TC4_INTFLAG |= TC_INTFLAG_OVF;              // Clear the interrupt flags
  REG_TC4_INTENSET = TC_INTENSET_OVF;             // Enable TC4 interrupts

  uint16_t prescale=0;
  switch(clk_div_)
  {
    case 1:    prescale=TC_CTRLA_PRESCALER(0); break;
    case 2:    prescale=TC_CTRLA_PRESCALER(1); break;
    case 4:    prescale=TC_CTRLA_PRESCALER(2); break;
    case 8:    prescale=TC_CTRLA_PRESCALER(3); break;
    case 16:   prescale=TC_CTRLA_PRESCALER(4); break;
    case 64:   prescale=TC_CTRLA_PRESCALER(5); break;
    case 256:  prescale=TC_CTRLA_PRESCALER(6); break;
    case 1024: prescale=TC_CTRLA_PRESCALER(7); break;
  }
  REG_TC4_CTRLA |= prescale | TC_CTRLA_WAVEGEN_MFRQ | TC_CTRLA_ENABLE;    // Enable TC4
  while (TC4->COUNT8.STATUS.bit.SYNCBUSY);        // Wait for synchronization
}
//----

uint16_t next_pow2(uint16_t v_)
{
  // the next power-of-2 of the value (if v_ is pow-of-2 returns v_)
  --v_;
  v_|=v_>>1;
  v_|=v_>>2;
  v_|=v_>>4;
  v_|=v_>>8;
  return v_+1;
}
//----

uint16_t get_clk_div(uint32_t freq_)
{
  float ideal_clk_div=48000000.0f/(256.0f*float(freq_));
  uint16_t clk_div=next_pow2(uint16_t(ceil(ideal_clk_div)));
  switch(clk_div)
  {
    case 32: clk_div=64; break;
    case 128: clk_div=256; break;
    case 512: clk_div=1024; break;
  }
  return clk_div;
}
//----

void setup_timer4(uint32_t freq_)
{
  uint16_t clk_div=get_clk_div(freq_);
  uint8_t clk_cnt=(48000000/clk_div)/freq_;
  setup_timer4(clk_div, clk_cnt);
}
// End MKR1010 software interrupt functions **********

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

      // clear interrupt
      REG_TC4_INTFLAG = TC_INTFLAG_OVF;
  }
}

void setup()
{
    // setup the low voltage pin
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
      // delay 10 second, remember we have to call sendBatteryStatus() 10 times to get the average so 1000 msec * 10 = 10 seconds
      delay(1000);
    }

    // call ISR - TC4_Handler 100000 times per second
    // examples:
    // an interrupt is called every 10 microseconds so
    // 1 interrupt = 10us
    // 5 interrupts = 50us
    // 100 interrupts = 1ms
    // 500 interrupts = 5ms
    // 1000 interrupts = 10ms
    // 50000 interrupts = 500ms
    // 100000 interrupts = 1s
    // 300000 interrupts = 3s
    // 500000 interrupts = 5s
    setup_timer4(100000);
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
