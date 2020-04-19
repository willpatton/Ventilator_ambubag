/*
 * Ventilator Ambubag
 * 
 * @author: Will Patton https://github.com/willpatton
 * @date: April 3, 2020
 * @license: MIT
 * 
 * BOARD
 * Select Arduino IDE board type: NODEMCU 1.0 (ESP12-E Module) 
 * 
 * Using Processor/Pinout as shown here:
 * ESP8266 12-E NodeMCU Kit
 * https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
 * 
 * GPIO16: pin is high at BOOT
 * GPIO0: boot failure if pulled LOW
 * GPIO2: pin is high on BOOT, boot failure if pulled LOW
 * GPIO15: boot failure if pulled HIGH
 * GPIO3: pin is high at BOOT
 * GPIO1: pin is high at BOOT, boot failure if pulled LOW
 * GPIO10: pin is high at BOOT
 * GPIO9: pin is high at BOOT
 * NOTE: GPIO4 and GPIO5 are the most safe to use GPIOs if you want to operate relays.
 * 
 * LED_BUILTIN
 * The official Nodemcu has the on-board LED connected to the GPIO16 pin (with inverse logic: LOW = ON, HIGH=OFF).
 * Does not go to physical pin.
 * 
 * //FYI PIN D4=GPIO2 fires BLUETOOTH LED, boot fails if pulled LOW
 * 
 * Sensors:
 * Limit Switch - Optical Beam Break
 * https://www.adafruit.com/product/2167
 * 
 * NXP Pressure Sensor:
 * MPXV5010G -  SINGLE 1.45 psi 
 *              0 to 10 kPa (0 to 1.45 psi) (0 to 1019.78 mm H2O) 0.2 to 4.7 V Output
 *              Wire to 5.0V
 *              
 * Flow Sensor:          
 * Sensirion             
 * SFM3200-250-AW, Digital Flow Meter for medical applications           
 * https://www.sensirion.com/en/download-center/mass-flow-meters/flow-sensor-sfm3200-for-a-superior-performance-at-low-flows/
 * 
 *
 * NOTES:
 * Options: 
 *    serial console/plotter
 *    pressure sensor, water gauge, beam break right, duty cycle potentiometer
 *    
 *    
 * TESTING:   
 * WIRE PIN D7 TO ARDUINO 2 (BREATH PIN) ATTACHE INTERRUPT ON FALLING EDGE
 * WIRE PIN D1 TO ARDUINO 3 (TEST RUNNING PIN)
 */

int debug = true;

//PINOUTS (NODEMCU ESP12-E Module)
#define POT   A0            //PIN A0 = ADCO   ANALOG INPUT - controls the BPM breaths per minute
#define TEST_RUNNING_PIN 5  //PIN D1 = GPIO5  goes to D1 pin. HIGH when test is running, LOW at the completion of TEST
#define RELAY 14            //PIN D5 = GPIO14 EXHALE RELAY           
#define BBL     12          //PIN D6 = GPIO12 goes to D6 pin
//#define BBR    13           //PIN D7 = GPIO13 goes to D7 pin
#define BREATH_PIN 13       //PIN D7 = GPIO13 goes to D7 pin     

//SENSORS
unsigned int pressureSensor = 0;
unsigned int flowSensor     = 0;
unsigned int beamBreakL     = 0;      //beam break left

//BREATH STATES
#define MAX_INHALE_MSEC 6000          //typically 6000   
#define MIN_INHALE_MSEC 700           //typically 700
#define NONE 0                        //breath states
#define BEGIN 1                       //  "     "
#define INHALE_INIT 2                 //  "     "
#define INHALE 3                      //  "     "
#define PLATEAU_INIT 4                //  "     "
#define PLATEAU 5                     //  "     "
#define EXHALE_INIT 6                 //  "     "
#define EXHALE 7                      //  "     "
int breathState  = BEGIN;             //breath state (on first run)

//BREATH TIMERS, COUNTERS
unsigned long inhaleMillis  = 1001;   //milliseconds  1000 = 1 sec
unsigned long plateauMillis = 1;      //  "       "     "
unsigned long exhaleMillis  = 1003;   //  "       "     "
unsigned long inhaleTimer   = 0;      //breath timers
unsigned long plateauTimer  = 0;      //  "       "     "
unsigned long exhaleTimer   = 0;      //  "       "     "
unsigned long bpmTimer      = 0;      //  "       "     "
float bpm = 0;                        //desired breaths per minute
unsigned long breath_counter = 0;     //num of breaths taken so far

//TESTING
#define TEST_CYCLES_MAX 5           //typically 5 to 100 loops for testing
bool testCompleteFlag = false;        //false to begin, then assert true when finished

//CONTROL FLAGS
bool displayFlag = false;             //show text to console
bool plotFlag = false;                //show data to plotter

//PERFORMANCE TIMERS, COUNTERS
int perfFlag = false;                 //enable peformance monitoring
unsigned long refresh_counter = 1;
unsigned long loop_counter = 1;
unsigned long loop_timer = 0;
unsigned long second_timer = 0;       //1-second timer


/**
 * setup
 */
void setup() {

  Serial.begin(115200);
  delay(1000);
  
  //LED
  pinMode(LED_BUILTIN, OUTPUT);  //on-board LED. Does not go to physical pin.

  //POT
  pinMode(POT, INPUT);          //analog in, wired to pot wiper. Controls breaths per minute (BPM)
  
  //RELAY
  pinMode(RELAY, OUTPUT);       //wired to relay signal pin. When asserted, it applies 24VDC to the actuator to compress the ambubag

  //LIMIT SWITH - BEAM BREAK
  pinMode(BBL, INPUT_PULLUP);   //wired as a limit switch

  //TESTING
  pinMode(BREATH_PIN, OUTPUT);           //wire to Arduino input pin 2
  digitalWrite(BREATH_PIN, HIGH);        //HIGH = exhale (DEFAULT), LOW = inhale.  OPPOSITE logic of RELAY
  pinMode(TEST_RUNNING_PIN, OUTPUT);     //wire to Arduino input pin 3
  digitalWrite(TEST_RUNNING_PIN, LOW);   //LOW = not running/complete. HIGH = test is RUNNING/ACTIVE.  
}


/*
 * loop
 */
void loop() {

  //POT
  unsigned int pot = analogRead(POT);
  //Serial.println(pot);
  inhaleMillis = exhaleMillis = map(pot, 0,1023, MAX_INHALE_MSEC, MIN_INHALE_MSEC);

  //LMIT SWITCH BB
  beamBreakL = digitalRead(BBL);

  //BREATH STATE
  if(breathState == NONE){
    //do nothing, inactive
  }

  //BREATH STATE
  if(breathState == BEGIN){
    breathState = INHALE_INIT;    //begin to breathe. begin cycles. 
  }

  //LIMIT SWITCH
  //if beam break LEFT broken, then goto PLATEAU_INIT
  if(breathState == INHALE && !digitalRead(BBL)){   
    Serial.println("BEAM BROKEN - LEFT LIMIT");
    breathState = PLATEAU_INIT;
    plateauTimer = millis();
  }


  //INHALE STATES
  if(breathState == INHALE_INIT){
    digitalWrite(LED_BUILTIN, LOW);         //assert LED ON = ACTIVE LOW
    digitalWrite(TEST_RUNNING_PIN, HIGH);   //assert HIGH that test is active/running, okay
    digitalWrite(BREATH_PIN, LOW);          //inhale
    digitalWrite(RELAY, HIGH);              //assert INHALE. FILL LUNGS HERE!!!
    if(debug){Serial.println("INHALE_INIT ");}
    breathState = INHALE;                 //go to next state
    inhaleTimer = millis();
  } 
  if((breathState == INHALE) && (millis() - inhaleTimer < inhaleMillis)){
     if(debug){
      Serial.print("INHALE "); Serial.print(millis() - inhaleTimer); 
      Serial.print(" < ");Serial.println(inhaleMillis);
     }
  } 
  if((breathState == INHALE) && (millis() - inhaleTimer >= inhaleMillis)){
     breathState = PLATEAU_INIT;      //go to next state
     plateauTimer = millis();
  }


  //PLATEAU STATES
  if(breathState == PLATEAU_INIT){
    if(debug){Serial.println("PLATEAU_INIT ");}
    breathState = PLATEAU;            //go to next state 
    //digitalWrite(LED_BUILTIN, HIGH);  
    plateauTimer = millis();
  }
  if((breathState == PLATEAU) && (millis() - plateauTimer < plateauMillis)){
    if(debug){
      Serial.print("PLATEAU: "); Serial.print(millis() - plateauTimer); 
      Serial.print(" < ");Serial.println(plateauMillis);
    }
  }
  if((breathState == PLATEAU) && (millis() - plateauTimer >= plateauMillis)){
    breathState = EXHALE_INIT;        //go to next state
    exhaleTimer = millis();
  }


  //EXHALE STATES
  if(breathState == EXHALE_INIT){
    digitalWrite(LED_BUILTIN, HIGH);    //LED OFF = INACTIVE HIGH  
    digitalWrite(BREATH_PIN, HIGH);     //exhale
    digitalWrite(RELAY, LOW);           //EXHALE - open the exhale valve. EMPTY LUNGS HERE!!!
    if(debug){Serial.println("EXHALE_INIT");}
    breathState = EXHALE;               //go to next state
    exhaleTimer = millis();  
  } 
  if((breathState == EXHALE) && (millis() - exhaleTimer < exhaleMillis)){
     if(debug){
      Serial.print("EXHALE: ");  Serial.print(millis() - exhaleTimer); 
      Serial.print(" < ");Serial.println(exhaleMillis);
     }
  } 
  if((breathState == EXHALE) && (millis() - exhaleTimer >= exhaleMillis)){
     breathState = BEGIN;              //go to beginning
     breath_counter++;
  }

  
  //DISPLAY TEXT
  if(displayFlag){
    Serial.print("BREATH count: ");Serial.print(breath_counter);
    Serial.print(", state: ");Serial.print(breathState);
    unsigned long bpmMillis = inhaleMillis + plateauMillis + exhaleMillis;
    bpm = (float) 60000/bpmMillis;
    Serial.print(", BPM: ");Serial.print(bpm,1);
    Serial.print(", inhale: ");Serial.print(inhaleMillis);Serial.print(" ms");
    Serial.print(", exhale: ");Serial.print(exhaleMillis);
    //Serial.print(" pressure: ");Serial.print(pressureSensor);
    Serial.print(", LIMIT: "); Serial.print(beamBreakL);
    Serial.print(", refresh: ");Serial.print(millis() - loop_timer); Serial.print(" ms ");
    Serial.println();
  }

  //TEST COMPLETE
  if((breath_counter > TEST_CYCLES_MAX) && !testCompleteFlag){
    breathState = NONE;                 //stop, no more breaths
    displayFlag = false;                //stop, no more output
    plotFlag = false;                   //stop, no more output 
    digitalWrite(TEST_RUNNING_PIN, LOW); //stop, LOW test no longer running/active. Was HIGH during test. 
    Serial.print("TEST COMPLETE: "); Serial.print(breath_counter-1); Serial.println(" breaths.");
    testCompleteFlag = true;  //run once
  }

  //PLOT
  //plot sensor readings to the serial plotter
  if(plotFlag){
    Serial.println(pressureSensor);  
    Serial.println(flowSensor);  
  }

  //PERFORMANCE
  //loop performance
  if(perfFlag && ((millis() - second_timer) > 1000)){
    Serial.print("REFRESH RATE: ");
    //Serial.print(millis() - second_timer); Serial.print(" msec ");
    //counts per second is same as Hz
    Serial.print(refresh_counter); Serial.println(" Hz");  
    refresh_counter = 0;  
    second_timer = millis();
  }

  //RESET LOOP
  loop_timer = millis();
  refresh_counter++;
  loop_counter++;

  //SLOW DOWN
  delay(10);          //not too fast
}
