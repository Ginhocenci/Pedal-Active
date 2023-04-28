#include <ezButton.h>

 //limit switch 2 5v & pin1 (Min Pedal Travel)
 //limit switch 3 5v & pin2 (Max Pedal Travel)

ezButton limitSwitchMin(2);  // create ezButton object that attach to pin 2;
ezButton limitSwitchMax(3);  // create ezButton object that attach to pin 3;

//Stepper Motor Information
#include "FastAccelStepper.h"

    // Stepper Wiring
    #define dirPinStepper    7 
    #define stepPinStepper   9  // step pin must be pin 9

//no clue what this does
FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper = NULL;



/*ADS1256 Pinouts
5V - 5V
GND - GND
SCLK - pin 13 (SCK)
DIN - pin 11 (MOSI)
DOUT - pin 12 (MISO)
DRDY - pin 6
CS - pin 10
POWN - 5V
*/

#include <ADS1256.h>
#include <SPI.h>
#include <movingAvgFloat.h> 

float clockMHZ = 7.68; // crystal frequency used on ADS1256
float vRef = 2.5; // voltage reference
// Initialize ADS1256 object
ADS1256 adc(clockMHZ,vRef,false); // RESETPIN is permanently tied to 3.3v


float sensor1;
movingAvgFloat Force_Current_MA(40);      //Sets the number of data points to use when calculating the moving average 

float conversion = 4000; //conversion factor
float offset = 0;     //offset value

//Variable for calculation pedal movement & Stepper Parameters
  int Force_Min = 0.25;    //Min Force in lb to activate Movement
  int Force_Max = 50;     //Max Force in lb = Max Travel Position
  float Force_Current = 0;
  
  int Position_Min = 0;       //Pedal starting Postion. In the future this could be found using a switch
  int Position_Max = 90;     //Pedal maxium distance to travel
  int Position_Current = 0;   //Current position of pedal
  int Position_Next = 0;      //Future pedal postion based on current load cell data
  int Position_Deadzone = 5;  //Number of steps required before the pedal will move. Added in to prevent oscillation caused by varaying load cell readings
  
  float steps_per_rev = 1600;
  float rpm = 2500;
  float speed = (rpm/60*steps_per_rev);   //needs to be in us per step || 1 sec = 1000000 us
  float accel = 1000000;

void setup()
{
  Serial.begin(2000000);

  Force_Current_MA.begin();     //Start force moving average
  
  Serial.println("Starting ADC");

  // start the ADS1256 with data rate of 15 SPS
  // other data rates: 
  // ADS1256_DRATE_30000SPS
  // ADS1256_DRATE_15000SPS
  // ADS1256_DRATE_7500SPS
  // ADS1256_DRATE_3750SPS
  // ADS1256_DRATE_2000SPS
  // ADS1256_DRATE_1000SPS
  // ADS1256_DRATE_500SPS
  // ADS1256_DRATE_100SPS
  // ADS1256_DRATE_60SPS
  // ADS1256_DRATE_50SPS
  // ADS1256_DRATE_30SPS
  // ADS1256_DRATE_25SPS
  // ADS1256_DRATE_15SPS
  // ADS1256_DRATE_10SPS
  // ADS1256_DRATE_5SPS
  // ADS1256_DRATE_2_5SPS
  // 
  // NOTE : Data Rate vary depending on crystal frequency. Data rates listed below assumes the crystal frequency is 7.68Mhz
  //        for other frequency consult the datasheet.
  //Posible Gains 
  //ADS1256_GAIN_1 
  //ADS1256_GAIN_2 
  //ADS1256_GAIN_4 
  //ADS1256_GAIN_8 
  //ADS1256_GAIN_16 
  //ADS1256_GAIN_32 
  //ADS1256_GAIN_64 
  adc.begin(ADS1256_DRATE_500SPS,ADS1256_GAIN_64,false); 

  Serial.println("ADC Started");
  
   /// Set MUX Register to AINO and AIN1 so it start doing the ADC conversion 
  adc.setChannel(0,1); // switch back to MUX AIN0 and AIN1

  adc.waitDRDY(); // wait for DRDY to go low before changing multiplexer register
    adc.setChannel(0,1);   // Set the MUX for differential between ch2 and 3 
    sensor1 = (adc.readCurrentChannel()*conversion); // DOUT arriving here are from MUX AIN0 and AIN1

 //Load Cell Offset
  int i;
  float sval = 0;
  float ival = 0;
  int samples = 1000;
  
  for (i = 0; i < samples; i++){
    adc.setChannel(0,1);   // Set the MUX for differential between ch2 and 3 
    sensor1 = (adc.readCurrentChannel()*conversion); // DOUT arriving here are from MUX AIN0 and 
    ival = sensor1;
    Serial.println(sensor1,10);
    sval = sval + ival;
    
  }

  offset = sval/samples;
  conversion = -0.30/offset*conversion;  //ADS1256 doesn't always have a consitent conversion factor so I had to add this in 

    Serial.print("Offset ");
    Serial.println(offset,10);
    delay(1000);

    
  //FastAccelStepper setup
  engine.init();
  stepper = engine.stepperConnectToPin(stepPinStepper);
  if (stepper) {
    stepper->setDirectionPin(dirPinStepper);
    //stepper->setEnablePin(enablePinStepper);
    stepper->setAutoEnable(true);
    
  //Stepper Parameters
    stepper->setSpeedInHz(speed);   // steps/s
    stepper->setAcceleration(accel);  // 100 steps/s²

  }

// Setting Min and Max Travel Position
 
limitSwitchMin.setDebounceTime(50); // set debounce time to 50 milliseconds
limitSwitchMax.setDebounceTime(50); // set debounce time to 50 milliseconds

 limitSwitchMin.loop();  // MUST call the loop() function first
 limitSwitchMax.loop();  // MUST call the loop() function first

  //int stateMin = limitSwitchMin.getState();
  //int stateMax = limitSwitchMax.getState();
  

     int set = 0;                                                 //Setting Max Limit Switch Position
      while(limitSwitchMax.getState() == HIGH){
      stepper->setSpeedInHz(speed);   // steps/s      
      stepper->moveTo(set);       //Move motor 10 steps back
        
        while (stepper->getCurrentPosition() != stepper->targetPos()){}
        

      set = set + 100;
      Serial.println(set);
          
    limitSwitchMax.loop();  // MUST call the loop() function first

 
      if(limitSwitchMax.getState() == LOW){
          Serial.println("The limit switch: Max On");
          Serial.print("Max Position is "); 
          Serial.println(stepper->getCurrentPosition()-1000);

        Position_Max = (stepper->getCurrentPosition()-1000);
    }
  }    

      while(limitSwitchMin.getState() == HIGH){                    //Setting Min Limit Switch Position
        stepper->setSpeedInHz(speed);   // steps/s
        stepper->moveTo(set);       //Move motor 10 steps back

          while (stepper->getCurrentPosition() != stepper->targetPos()){}

      set = set - 100;
      Serial.println(set);

  limitSwitchMin.loop();  // MUST call the loop() function first

    if(limitSwitchMin.getState() == LOW){
    Serial.println("The limit switch: Min On");
    Serial.print("Min Position is ");
    Serial.println(stepper->getCurrentPosition()+6000);
    
    Position_Min = (stepper->getCurrentPosition()+6000);
     
    }
  }   

stepper->moveTo(Position_Min);       
      while (stepper->getCurrentPosition() != stepper->targetPos()){
      delay(10);
      }

stepper->setSpeedInHz(speed);

}

void loop()
{ 

adc.waitDRDY(); // wait for DRDY to go low before changing multiplexer register
  adc.setChannel(0,1);   // Set the MUX for differential between ch2 and 3 
  sensor1 = adc.readCurrentChannel()*conversion; // DOUT arriving here are from MUX AIN0 and AIN1
  Force_Current = Force_Current_MA.reading(sensor1);

 
  if (Force_Current > Force_Max)  {       //If current force is over the max force it will just read the max force
    Force_Current = Force_Max;
  }

  if (Force_Current < Force_Min)  {       //If current force is below the min force it will just read 0
    Force_Current = Force_Min;
  }
  
  Position_Next = ((Position_Max-Position_Min)/(Force_Max-Force_Min))*(Force_Current-Force_Min)+Position_Min ;        //Calculates new position using linear function
  
  Serial.print(Force_Current);
  Serial.print(" lbs || ");
  Serial.print(Position_Next - Position_Current);
  Serial.print(" Travel Distance ");
  Serial.print(Position_Next);
  Serial.print(" senosr lbs ");
  Serial.println(sensor1,4);

  if(abs(Position_Next-Position_Current)>Position_Deadzone){       //Checks to see if the new position move is greater than the deadzone limit set to prevent oscillation
  
  stepper->moveTo(Position_Next);

  Position_Current = Position_Next;      //Sets the new current position after the stepper motors moves !!!This has to be after the stepper have moved
  }

 }
