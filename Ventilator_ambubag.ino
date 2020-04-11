/*
 * Ventilator Ambubag
 * 
 * @author: Will Patton https://github.com/willpatton
 * @date: April 3, 2020
 * @license: MIT
 * 
 * BUILD
 * Arduino IDE Board type: ESP8266 12-E NodeMCU 
 * 
 * Processor/Pinout
 * ESP8266
 * 
 * Sensors:
 * Beam Break
 * https://www.adafruit.com/product/2167
 * 
 * NXP Pressure Sensor
 * MPXV5010G -  SINGLE 1.45 psi 
 *              0 to 10 kPa (0 to 1.45 psi) (0 to 1019.78 mm H2O) 0.2 to 4.7 V Output
 *              Wire to 5.0V
 *
 * 
 * NOTES:
 * Options: 
 *    serial console/plotter
 *    pressure sensor, water gauge, beam break right, duty cycle potentiometer
 * 
 * 
 */

int debug = true;

//PINOUTS ESP8266
#define RELAY 14        //GPIO14 goes to D5 pin
#define POT A0          //ADCO  controls the BPM breaths per minute
#define BBL    12       //GPIO12 goes to D6 pin
#define BBR    13       //GPIO13 goes to D7 pin

//SENSORS
unsigned int pressureSensor = 0;
unsigned int beamBreakL = 0;          //beam break left
unsigned int beamBreakR = 0;          //beam break right - NOT USED 

//FLAGS
bool displayFlag = false;             //text
bool plotFlag = false;                //plot to graph

//BREATH STATES
#define MAX_INHALE_MSEC 6000          //3400 
#define MIN_INHALE_MSEC 700           //500
#define NONE 0                        //breath states
#define INHALE_INIT 1
#define INHALE 2
#define PLATEAU_INIT 3
#define PLATEAU 4
#define EXHALE_INIT 5
#define EXHALE 6 
int breathState  = NONE;              //breath state

//BREATH TIMERS
unsigned long inhaleMillis  = 1001;   //milliseconds  1000 = 1 sec
unsigned long plateauMillis = 1;      //  "       "     "
unsigned long exhaleMillis  = 1003;   //  "       "     "
unsigned long inhaleTimer   = 0;      //breath timers
unsigned long plateauTimer  = 0;
unsigned long exhaleTimer   = 0;
unsigned long bpmTimer      = 0;
float bpm = 0;                        //breaths per minute


//PERFORMANCE TIMERS, COUNTERS
int perfFlag = false;
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
  pinMode(LED_BUILTIN, OUTPUT);

  //RELAY
  pinMode(RELAY, OUTPUT);     //when asserted, it applies 24VDC to the acuator to compress the ambubag

  //POT
  pinMode(POT, INPUT);        //BPM breaths per minute

  //BEAM BREAK
  pinMode(BBL, INPUT_PULLUP); //acts as a limit switch
  pinMode(BBR, INPUT_PULLUP);
  
}


/*
 * loop
 */
void loop() {


  //POT
  unsigned int pot = analogRead(POT);
  //Serial.println(pot);
  inhaleMillis = exhaleMillis = map(pot, 0,1024, MAX_INHALE_MSEC, MIN_INHALE_MSEC);

  //BB
  beamBreakL = digitalRead(BBL);
  beamBreakR = digitalRead(BBR);

  //BREATH STATE
  if(breathState == NONE){
    breathState = INHALE_INIT;
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
    digitalWrite(LED_BUILTIN, LOW);
    //FILL LUNGS
    if(debug){Serial.println("INHALE_INIT ");}
    digitalWrite(RELAY, HIGH);
    breathState = INHALE;             //go to next state
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
    digitalWrite(LED_BUILTIN, HIGH);  
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
    
    digitalWrite(LED_BUILTIN, HIGH);     
    if(debug){Serial.println("EXHALE_INIT");}
    //digitalWrite(RELAY, LOW);           //open the exhale valve
    breathState = EXHALE;            //go to next state
    exhaleTimer = millis();  
  } 
  if((breathState == EXHALE) && (millis() - exhaleTimer < exhaleMillis)){
     if(debug){
      Serial.print("EXHALE: ");  Serial.print(millis() - exhaleTimer); 
      Serial.print(" < ");Serial.println(exhaleMillis);
     }
  } 
  if((breathState == EXHALE) && (millis() - exhaleTimer >= exhaleMillis)){
     breathState = NONE;              //go to beginning
  }

  
  //DISPLAY TEXT
  if(displayFlag){
    Serial.print("BREATH state: ");Serial.print(breathState);
    unsigned long bpmMillis = inhaleMillis + plateauMillis + exhaleMillis;
    bpm = (float) 60000/bpmMillis;
    Serial.print(" BPM: ");Serial.print(bpm,1);
    Serial.print(" inhale: ");Serial.print(inhaleMillis);
    Serial.print(" ms exhale: ");Serial.print(exhaleMillis);
    //Serial.print(" pressure: ");Serial.print(pressureSensor);
    Serial.print(" BBL: "); Serial.print(beamBreakL);
    Serial.print(" BBR: "); Serial.print(beamBreakR);
    Serial.print(" refresh: ");Serial.print(millis() - loop_timer); Serial.print(" msec ");
    Serial.println();
  }

  //PLOT
  //plot sensor readings to the serial plotter
  if(plotFlag){
    Serial.println(pressureSensor);   
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
