/* Temperature control in greenhouses using a microprocessor 
 *     and electric motor to open and close the window
 * *********************************************************
 *                Jeriel Shylo Rubarajan
 * 
 * Last update: 15th April 2021
 * 
 * OneWire Bus Pin - 2
 * i2c Pins - SDA, SCL
 * OLED i2c address - 0x3C
 * 
 * Stepper Motor Pins - 8, 9 ,10, 11
 * Stepper Homing Limit Switch Pins - 6 (Closed), 7 (Open)
 * Push button Pins - 3 (Menu), 4 (Up), 5 (Down)
*/

#include <SPI.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define ScreenWidth   128     // OLED display width, in pixels
#define ScreenHeight  64      // OLED display height, in pixels
#define ScreenAddress 0x3C    // OLED display address
#define OLEDReset     -1      // Reset pin #-1 as its sharing the Arduino reset pin

#define OneWireBus        2   // OneWire Bus pin number for temperature sensors
#define DS18B20Resolution 12  // Set the resolution of the temperature sensor to 12 bits

//INPUTS
#define MenuButton        3   // Menu Button
#define UpButton          4   // ++ Button
#define DownButton        5   // -- Button
#define StepperHomeLimitClosed 6 // Stepper Motor Home Limit Switch CLOSED
#define StepperHomeLimitOpen   7 // Stepper Motor Home Limit Switch OPEN

#define StepperDelay  7500  //Delay between StepperMotor Steps - Smaller values produce more speed and less torque

int MenuState = 2;  //Sets and declares the Menu state to 2 upon initialisation of the system
int HysteresisControlEnabled = 1; //Sets the HysteresisControl on upon initialisation of the system
int MenuButtonVal, UpButtonVal, DownButtonVal, HysteresisOutput, LastHO, CurrentHO, Sensors, PanelControl;

float Temperature, TempSensor0, TempSensor1, TempSensor2, TempSensor3, LowTargetTemperature, HysteresisThreshold;
float HysteresisMargin = 0.50;  //Margin of Hysteresis

Adafruit_SSD1306 display(ScreenWidth, ScreenHeight, &Wire, OLEDReset);
OneWire oneWire(OneWireBus);  //Initiliase the OneWire Protocol on the desired pin in reference to "OneWireBus"
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(9600);
   
//  pinMode(MenuButton, INPUT_PULLUP);  //Menu Button
//  pinMode(UpButton, INPUT_PULLUP);  //++ Button
//  pinMode(DownButton, INPUT_PULLUP);  //-- Button
//  pinMode(StepperHomeLimitOpen, INPUT_PULLUP); //STEPPER HOME LIMIT SWITCH
//  pinMode(StepperHomeLimitClosed, INPUT_PULLUP); //STEPPER HOME LIMIT SWITCH

  DDRD = DDRD | B00000000; //don't change pins 0 & 1 as they are RX & TX
  PORTD = B00011111;  //Set Ports 3 to 7 as input pull-up
  DDRB = B001111;  //Data Direction Register for Port-B to set pins 8 to 11 as outputs

  display.begin(SSD1306_SWITCHCAPVCC, ScreenAddress); //Initialise the OLED display
  display.clearDisplay(); //Clear the display

// Temperature Sensor Setup
  sensors.begin();  //Initialise the DS18B20 sensors
  Sensors = sensors.getDeviceCount(); //Detect number of sensors in system
  sensors.setResolution(DS18B20Resolution); //Set the resolution of the temperature sensors
  sensors.requestTemperatures();
  TempSensor0 = sensors.getTempCByIndex(0);
  TempSensor1 = sensors.getTempCByIndex(1);
  TempSensor2 = sensors.getTempCByIndex(2);
  TempSensor3 = sensors.getTempCByIndex(3);
  Temperature = (TempSensor0 + TempSensor1 + TempSensor2 + TempSensor3)/Sensors; 
  HysteresisThreshold = round(Temperature)+1; //Round the average temperature to the nearest whole number + 1

// Home the stepper motor
  for (int i = 0; i < 75; i++) {
    if (digitalRead(StepperHomeLimitOpen) == LOW) {
      break;
    }
    StepperCCW();
  }
  if (digitalRead(StepperHomeLimitOpen) == HIGH) {
    while(1);
  }
  
// Return to the closed position
  for (int i = 0; i < 75; i++) {
    if (digitalRead(StepperHomeLimitClosed) == LOW) {
      break;
    }
    StepperCW();
  }
  if (digitalRead(StepperHomeLimitClosed) == HIGH) {
    while(1);
  }
}

void loop() {
  MeasureTemperature();
  if (HysteresisControlEnabled == 1) {
    Hysteresis();
    Buttons();
    Menu();
  } else if (HysteresisControlEnabled == 0) { 
    // No Hysteresis Control 
    Buttons();
    Menu();
  }
}

void MeasureTemperature () {
  sensors.requestTemperatures();
  
       if (Sensors == 0) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.print("No sensors are connected");
  display.display();
} else if (Sensors == 1) {
  TempSensor0 = sensors.getTempCByIndex(0);
  Temperature = TempSensor0;
} else if (Sensors == 2) {
  TempSensor0 = sensors.getTempCByIndex(0);
  TempSensor1 = sensors.getTempCByIndex(1);
  Temperature = (TempSensor0 + TempSensor1)/Sensors;
} else if (Sensors == 3) {
  TempSensor0 = sensors.getTempCByIndex(0);
  TempSensor1 = sensors.getTempCByIndex(1);
  TempSensor2 = sensors.getTempCByIndex(2);
  Temperature = (TempSensor0 + TempSensor1 + TempSensor2)/Sensors;
} else if (Sensors == 4) {
  TempSensor0 = sensors.getTempCByIndex(0);
  TempSensor1 = sensors.getTempCByIndex(1);
  TempSensor2 = sensors.getTempCByIndex(2);
  TempSensor3 = sensors.getTempCByIndex(3);
  Temperature = (TempSensor0 + TempSensor1 + TempSensor2 + TempSensor3)/Sensors;
}
//  Serial Monitor Output
//  Serial.print(F("Average Temperature: "));
//  Serial.println(Temperature);
//  Serial.print(F("TempSensor 0 "));
//  Serial.println(TempSensor0);
}

void Hysteresis() {
  //ABOVE Target
  if (Temperature >= (HysteresisThreshold + HysteresisMargin)) {
    // Serial.println(F("Temperature is over target"));
    HysteresisOutput = HIGH;
  }
  //BELOW Target
  if (Temperature <= (HysteresisThreshold - HysteresisMargin)) {
    // Serial.println(F("Temperature is below target");
    HysteresisOutput = LOW;
  }
  LastHO = CurrentHO;
  CurrentHO = HysteresisOutput;
  if (LastHO == HIGH && CurrentHO == LOW) {
    while (digitalRead(StepperHomeLimitClosed) == HIGH) {
     StepperCW();
    }
  }
  if (LastHO == LOW && CurrentHO == HIGH) {
    while (digitalRead(StepperHomeLimitOpen) == HIGH) {
     StepperCCW();
    }
  }
  if (LastHO == LOW && CurrentHO == LOW || LastHO == HIGH && CurrentHO == HIGH) {
    PORTB = B001001;
  }
}

void Buttons() {
  MenuButtonVal = digitalRead(MenuButton);
  if (MenuButtonVal == LOW) {
    MenuState++;  //If the MenuButton is pressed, the MenuState is incremented by 1.
  }
  if (MenuState > 4) {
    MenuState = 0;  //If the MenuState goes beyond 4, it is reset to 0, as there are only that many states.
  }
}

void Menu() {
  //Display Settings
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("Average Temperature");
  display.setTextSize(2);
  display.print(Temperature);
  display.print((char)247); // Degree symbol
  display.println("C");
  display.setTextSize(1);

  //Menu
  switch (MenuState) {
    case 0:         //SET TARGET TEMP
      display.setTextColor(BLACK, WHITE);
      display.println("HysteresisThreshold: ");
      display.print(HysteresisThreshold);
      display.print((char)247); // Degree symbol
      display.println("C");
      display.setTextColor(WHITE);
      display.println("Hysteresis Margin: ");
      display.print(HysteresisMargin);
      display.print((char)247); // Degree symbol
      display.println("C");
      UpButtonVal = digitalRead(UpButton);
      if (UpButtonVal == LOW) {
        HysteresisThreshold += 0.1; //Increments the HysteresisThreshold value by 0.1
      }
      DownButtonVal = digitalRead(DownButton);
      if (DownButtonVal == LOW) {
        HysteresisThreshold -= 0.1; //Decrements the HysteresisThreshold value by 0.1
      }
      display.print("Target Temp: ");
      display.print(LowTargetTemperature);
      display.print((char)247);
      display.println("C");
      break;
    case 1:         //SET TEMP MARGIN
      display.println("HysteresisThreshold: ");
      display.print(HysteresisThreshold);
      display.print((char)247); // Degree symbol
      display.println("C");
      display.setTextColor(BLACK, WHITE);
      display.println("Hysteresis Margin: ");
      display.print(HysteresisMargin);
      display.print((char)247); // Degree symbol
      display.println("C");
      UpButtonVal = digitalRead(UpButton);
      if (UpButtonVal == LOW) {
        HysteresisMargin += 0.1;
      }
      DownButtonVal = digitalRead(DownButton);
      if (DownButtonVal == LOW) {
        HysteresisMargin -= 0.1;
      }
      if (HysteresisMargin <= 0.0) {
        HysteresisMargin = 0.0;
      }
      LowTargetTemperature = HysteresisThreshold - HysteresisMargin;
      display.setTextColor(WHITE);
      display.print("Target Temp: ");
      display.print(LowTargetTemperature);
      display.print((char)247);
      display.println("C");
      break;
    case 2:           //VIEW TEMP SENSORS
      display.println("Temperature Sensors");
            if (Sensors == 0) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.print("No sensors are connected");
        display.display();
      } else if (Sensors == 1) {
        display.print("TempSensor0: ");
        display.print(TempSensor0);
        display.print((char)247); // Degree symbol
        display.println("C");
      } else if (Sensors == 2) {
        display.print("TempSensor0: ");
        display.print(TempSensor0);
        display.print((char)247); // Degree symbol
        display.println("C");
        display.print("TempSensor1: ");
        display.print(TempSensor1);
        display.print((char)247); // Degree symbol
        display.println("C");
      } else if (Sensors == 3) {
        display.print("TempSensor0: ");
        display.print(TempSensor0);
        display.print((char)247); // Degree symbol
        display.println("C");
        display.print("TempSensor1: ");
        display.print(TempSensor1);
        display.print((char)247); // Degree symbol
        display.println("C");
        display.print("TempSensor2: ");
        display.print(TempSensor2);
        display.print((char)247); // Degree symbol
        display.println("C");
      } else if (Sensors == 4) {
        display.print("TempSensor0: ");
        display.print(TempSensor0);
        display.print((char)247); // Degree symbol
        display.println("C");
        display.print("TempSensor1: ");
        display.print(TempSensor1);
        display.print((char)247); // Degree symbol
        display.println("C");
        display.print("TempSensor2: ");
        display.print(TempSensor2);
        display.print((char)247); // Degree symbol
        display.println("C");
        display.print("TempSensor3: ");
        display.print(TempSensor3);
        display.print((char)247); // Degree symbol
        display.println("C");
      }
      break;
    case 3:           //Turn off System/Manual control
      display.setTextColor(BLACK, WHITE);
      display.println("Automatic Control");
      //if the option is off then disable to contorl???
      UpButtonVal = digitalRead(UpButton);
      if (UpButtonVal == LOW) {
        HysteresisControlEnabled++;
      }
      DownButtonVal = digitalRead(DownButton);
      if (DownButtonVal == LOW) {
        HysteresisControlEnabled--;
      }
      if (HysteresisControlEnabled < 0) {
        HysteresisControlEnabled = 0;
      } else if (HysteresisControlEnabled > 1) {
        HysteresisControlEnabled = 1;
      }
      switch (HysteresisControlEnabled) {
        case 0:
        display.print("OFF");
        display.setTextColor(WHITE);
        display.print("/");
        display.println("ON");
        break;
        case 1:
        display.setTextColor(WHITE);
        display.print("OFF");
        display.print("/");
        display.setTextColor(BLACK, WHITE);
        display.println("ON");
        break;
      }
      display.setTextColor(WHITE);
      display.println("Panel");
      if (PanelControl == 0) {
        display.setTextColor(BLACK, WHITE);
        display.print("CLOSED");
        display.setTextColor(WHITE);
        display.print("/");
        display.println("OPEN");
      } else if (PanelControl == 1) {
        display.print("CLOSED");
        display.print("/");
        display.setTextColor(BLACK, WHITE);
        display.println("OPEN");
      }
      break;
    case 4:
      display.println("Automatic Control");
      if (HysteresisControlEnabled == 0) {
        display.setTextColor(BLACK, WHITE);
        display.print("OFF");
        display.setTextColor(WHITE);
        display.print("/");
        display.println("ON");
      } else if (HysteresisControlEnabled == 1) {
        display.print("OFF");
        display.print("/");
        display.setTextColor(BLACK, WHITE);
        display.println("ON");
      }
      display.setTextColor(BLACK, WHITE);
      display.println("Panel");
      UpButtonVal = digitalRead(UpButton);
      if (UpButtonVal == LOW) {
        PanelControl++;
      }
      DownButtonVal = digitalRead(DownButton);
      if (DownButtonVal == LOW) {
        PanelControl--;
      }
      if (PanelControl < 0) {
        PanelControl = 0;
      } else if (PanelControl > 1) {
        PanelControl = 1;
      }
      switch (PanelControl) {
        case 0:
        display.print("CLOSED");
        display.setTextColor(WHITE);
        display.print("/");
        display.print("OPEN");
        if (HysteresisControlEnabled == 0 || StepperHomeLimitClosed == LOW) {
          while (digitalRead(StepperHomeLimitClosed) == HIGH) {
            StepperCW();
          }
        }
        break;
        case 1:
        display.setTextColor(WHITE);
        display.print("CLOSED");
        display.print("/");
        display.setTextColor(BLACK, WHITE);
        display.print("OPEN");
        if  (HysteresisControlEnabled == 0 || StepperHomeLimitOpen == LOW) {
          while (digitalRead(StepperHomeLimitOpen) == HIGH) { 
            StepperCCW();
          }
        }
        break;
      }
    break;
  }
  display.display();
  display.clearDisplay();
}

void StepperCCW() {
      PORTB = B000001;
      delayMicroseconds (StepperDelay);
      PORTB = B000101;
      delayMicroseconds (StepperDelay);
      PORTB = B000100;
      delayMicroseconds (StepperDelay);
      PORTB = B000110;
      delayMicroseconds (StepperDelay);
      PORTB = B000010;
      delayMicroseconds (StepperDelay);
      PORTB = B001010;
      delayMicroseconds (StepperDelay);
      PORTB = B001000;
      delayMicroseconds (StepperDelay);
      PORTB = B001001;
      delayMicroseconds (StepperDelay);
}

void StepperCW() {
      PORTB = B001001;
      delayMicroseconds (StepperDelay);
      PORTB = B001000;
      delayMicroseconds (StepperDelay);
      PORTB = B001010;
      delayMicroseconds (StepperDelay);
      PORTB = B000010;
      delayMicroseconds (StepperDelay);
      PORTB = B000110;
      delayMicroseconds (StepperDelay);
      PORTB = B000100;
      delayMicroseconds (StepperDelay);
      PORTB = B000101;
      delayMicroseconds (StepperDelay);
      PORTB = B000001;
      delayMicroseconds (StepperDelay);
}
