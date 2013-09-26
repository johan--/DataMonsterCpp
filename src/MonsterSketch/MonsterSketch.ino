//#include "Servo.cpp"g_oDataMonster
#include "DataMonster.cpp"
#include "SensorModule.cpp"
#include "TwitterModule.cpp"
#include <Ethernet.h>
//#include <WiFi.h>

// Function Signatures
void getSerialCommand();
void getPersonsLocation(float& _fX, float& _fY, float& _fZ);

// Globals
DataMonster* g_poDataMonster;
SensorModule* g_poMySensorModule;
TwitterModule* g_poTweeterListener;

//////////////////////////////////////////
// Ethernet/WiFi
//////////////////////////////////////////

bool g_bGotTweet = false;

#define TWITTER_POLLING_TIME 100
unsigned long int g_iTwitterPollCounter = 0;
//unsigned int g_iTwitterPollCounter = 0;

byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,1,20);
char server[] = "www.thingspeak.com"; //https://www.thingspeak.com/channels/7554/field/1/last.json
//String g_sHttpRequest = "GET /channels/7554/field/1/last.json HTTP/1.0"; // Carlos' channel
String g_sHttpRequest = "GET /channels/7556/field/1/last.json HTTP/1.0"; // Lucas' channel

EthernetClient client;
unsigned long g_iLastAttemptTime = 0;
const unsigned long g_iRequestInterval = 10000;  // delay between requests
boolean g_bRequested;                   // whether you've made a request since connecting

//String currentLine = "";            // string to hold the text from server
String g_sJsonString = "";
boolean readingJsonString = false;       // if you're currently reading the tweet

//////////////////////////////////////////
int g_iPwmValue = 128;
bool g_bSettingLowLimit = true;
//float g_fAngle = 0;
//float g_fOneDegInRad = 0;
int g_iJointSelect;
int g_iJointCounter;
long g_iJointUpdateTimerCounter;
#define JOINT_UPDATE_TIMER_COUNTER_LIMIT 1000000

int g_iByte = 0;

int iCount = 0;
bool bIncrementing = true;
float g_fX = 0;
float g_fY = 0;
float g_fZ = 0;
int iPwmValue = 0;

float target[5];
float velocity[5];
float force[5];
float magnitude[5];
float location[5];

float dampening;
float springValue;



#define UP 119 // w
#define DOWN 115 // s
#define LEFT 97 // a
#define RIGHT 100 // d
#define SPACE 32 // space 

// Test
int g_iLed13 = 13;

void setup() {

  ///  will need to move these to the update loop once they're inputing values dynamically
  /////////////////////////////springValue (usually small- (0.001-.1)
  springValue = 0.01;
  /////////////////////////////////////////////////////////////////////

  ////////////////////////////dampening value (1= no dampening, 0 = full dampening (no movement)) Usually .9 or so.
  dampening = 0.9;
  //////////////////////////////////////////////////////////////////////

  for (int i=0; i<5; i++){
    target[i] = 0.1;
  }
  for (int i=0; i<5; i++){
    force[i] = 0.1;
  }
  for (int i=0; i<5; i++){
    magnitude[i] = 0.1;
  }
  for (int i=0; i<5; i++){
    location[i] = 0.1;
  }

  // Blinking Led
  pinMode(g_iLed13, OUTPUT);     

  // Init Ethernet/WiFi
  initNetwork();

  // Init abstract hardware classes
  g_poDataMonster = new DataMonster();
  g_poMySensorModule = new SensorModule(SENSOR_MODULE_STATUS_LED_PIN);
  g_poTweeterListener = new TwitterModule(TWITTER_MODULE_BUTTON_PIN,
  TWITTER_MODULE_STATUS_LED_PIN,
  TWITTER_MODULE_STATUS_LED_PIN);

  //Initialize serial and wait for port to open:
  Serial.begin(9600); 
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

  // prints title with ending line break 
  Serial.println("*************************************"); 
  Serial.println("***** Initializing Data Monster ****"); 
  Serial.println("*************************************"); 

}

void loop() {

  // RUN JOINT CALIBRATION ROUTINE
#ifdef CALIBRATING 

  // Check Serial for command for Robot joint calibration
  getSerialCommand();

  //////////////
  // Check if Robot is calibrated  
  /////////////
  calibRobot();

#else // RUN ROBOT PROGRAM

  /////////////////////////////////////////////
  // Get External Stimulus
  /////////////////////////////////////////////

  // Get latest stimulus from Twitter (or Button)
  String sServerString = checkTwitter();
  g_bGotTweet = g_poTweeterListener->gotTweet(sServerString); 

  /////////////////////////////////////////////
  // Get Object Location
  /////////////////////////////////////////////

  // Get latest data from sensor module
  g_poMySensorModule->update();
  // Get the object's location
  g_poMySensorModule->getLocation(g_fX,g_fY,g_fZ);

  /////////////////////////////////////////////
  // Set Robot
  /////////////////////////////////////////////
  //////////////////HARDCODING VALUES TO JOINTS BASED ON COMMON SENSOR VALUES BELOW////////////////////////
  // X maps to joint 0
  if(g_fX < -.1){
    iPwmValue = mapfloat(g_fX, -.1, -0.6, g_poDataMonster->m_apJoinArray[0]->m_fPwmMin, g_poDataMonster->m_apJoinArray[0]->m_fPwmMin + 50);  ///////<-hardcoded!

  } 
  else if(g_fX > 0.1){
    iPwmValue = mapfloat(g_fX, 0.1, 0.6, g_poDataMonster->m_apJoinArray[0]->m_fPwmMax , g_poDataMonster->m_apJoinArray[0]->m_fPwmMax - 50); ///////<-hardcoded!
  }
  else{
    iPwmValue = mapfloat(g_fX, SENSOR_X_MIN, SENSOR_X_MAX, g_poDataMonster->m_apJoinArray[0]->m_fPwmMin, g_poDataMonster->m_apJoinArray[0]->m_fPwmMax);
  }
  target[0]= iPwmValue;


  // Z maps to joint 1
  if(g_fZ >= 0 && g_fZ < 0.1){
    iPwmValue = mapfloat(g_fX, 0, (.1), g_poDataMonster->m_apJoinArray[1]->m_fPwmMin, g_poDataMonster->m_apJoinArray[1]->m_fPwmMax-50);  ///////<-hardcoded! 
  }
  else if(g_fZ >= 0.1 && g_fZ< 0.2){
    iPwmValue = mapfloat(g_fZ, 0.1, 0.2, g_poDataMonster->m_apJoinArray[1]->m_fPwmMin + 50, g_poDataMonster->m_apJoinArray[1]->m_fPwmMax);  ///////<-hardcoded!
  }
  else if(g_fZ >= 0.2 && g_fZ< 0.3){
    iPwmValue = mapfloat(g_fZ, 0.1, 0.2, g_poDataMonster->m_apJoinArray[1]->m_fPwmMin + 30, g_poDataMonster->m_apJoinArray[1]->m_fPwmMax);  ///////<-hardcoded!
  }
  else if(g_fZ >= 0.3 && g_fZ< 0.6){
    iPwmValue = mapfloat(g_fZ, 0.3, 0.6, g_poDataMonster->m_apJoinArray[1]->m_fPwmMin, g_poDataMonster->m_apJoinArray[1]->m_fPwmMax);  ///////<-hardcoded!
  }
  else{
    iPwmValue = mapfloat(g_fZ, SENSOR_Z_MIN, SENSOR_Z_MAX, g_poDataMonster->m_apJoinArray[1]->m_fPwmMin, g_poDataMonster->m_apJoinArray[1]->m_fPwmMax);
  }
  target[1]= iPwmValue;

  // Z maps to joint 2
  if(g_fZ >= 0 && g_fZ < 0.1){
    iPwmValue = mapfloat(g_fZ, 0, (.1), g_poDataMonster->m_apJoinArray[2]->m_fPwmMin, g_poDataMonster->m_apJoinArray[2]->m_fPwmMax-50);  ///////<-hardcoded! 
  }
  else if(g_fZ >= 0.1 && g_fZ< 0.2){
    iPwmValue = mapfloat(g_fZ, 0.1, 0.2, g_poDataMonster->m_apJoinArray[2]->m_fPwmMin + 50, g_poDataMonster->m_apJoinArray[2]->m_fPwmMax);  ///////<-hardcoded!
  }
  else if(g_fZ >= 0.2 && g_fZ< 0.3){
    iPwmValue = mapfloat(g_fZ, 0.1, 0.2, g_poDataMonster->m_apJoinArray[2]->m_fPwmMin + 30, g_poDataMonster->m_apJoinArray[2]->m_fPwmMax);  ///////<-hardcoded!
  }
  else if(g_fZ >= 0.3 && g_fZ< 0.6){
    iPwmValue = mapfloat(g_fZ, 0.3, 0.6, g_poDataMonster->m_apJoinArray[2]->m_fPwmMin, g_poDataMonster->m_apJoinArray[2]->m_fPwmMax);  ///////<-hardcoded!
  }
  else{
    iPwmValue = mapfloat(g_fZ, SENSOR_Z_MIN, SENSOR_Z_MAX, g_poDataMonster->m_apJoinArray[2]->m_fPwmMin, g_poDataMonster->m_apJoinArray[2]->m_fPwmMax);

  }
  target[2]= iPwmValue;

  // Z maps to joint 3
  if(g_fZ >= 0 && g_fZ < 0.1){
    iPwmValue = mapfloat(g_fZ, 0, (.1), g_poDataMonster->m_apJoinArray[3]->m_fPwmMin, g_poDataMonster->m_apJoinArray[3]->m_fPwmMax);  ///////<-hardcoded! 
  }
  else if(g_fZ >= 0.1 && g_fZ< 0.2){
    iPwmValue = mapfloat(g_fZ, 0.1, 0.2, g_poDataMonster->m_apJoinArray[3]->m_fPwmMin, g_poDataMonster->m_apJoinArray[3]->m_fPwmMax);  ///////<-hardcoded!
  }
  else if(g_fZ >= 0.2 && g_fZ< 0.3){
    iPwmValue = mapfloat(g_fZ, 0.1, 0.2, g_poDataMonster->m_apJoinArray[3]->m_fPwmMin, g_poDataMonster->m_apJoinArray[3]->m_fPwmMax);  ///////<-hardcoded!
  }
  else if(g_fZ >= 0.3 && g_fZ< 0.6){
    iPwmValue = mapfloat(g_fZ, 0.3, 0.6, g_poDataMonster->m_apJoinArray[3]->m_fPwmMin, g_poDataMonster->m_apJoinArray[3]->m_fPwmMax);  ///////<-hardcoded!
  }
  else{
    iPwmValue = mapfloat(g_fZ, SENSOR_Z_MIN, SENSOR_Z_MAX, g_poDataMonster->m_apJoinArray[3]->m_fPwmMin, g_poDataMonster->m_apJoinArray[3]->m_fPwmMax);
  }
  target[3]= iPwmValue;

  // Y maps to joint 4
  if(g_fZ >= 0 && g_fZ < 0.1){
    iPwmValue = mapfloat(g_fY, 0, (.1), g_poDataMonster->m_apJoinArray[4]->m_fPwmMin, g_poDataMonster->m_apJoinArray[4]->m_fPwmMax-50);  ///////<-hardcoded! 
  }
  else if(g_fZ >= 0.1 && g_fZ< 0.2){
    iPwmValue = mapfloat(g_fY, 0.1, 0.2, g_poDataMonster->m_apJoinArray[4]->m_fPwmMin + 50, g_poDataMonster->m_apJoinArray[4]->m_fPwmMax);  ///////<-hardcoded!
  }
  else if(g_fZ >= 0.2 && g_fZ< 0.3){
    iPwmValue = mapfloat(g_fY, 0.1, 0.2, g_poDataMonster->m_apJoinArray[4]->m_fPwmMin + 30, g_poDataMonster->m_apJoinArray[4]->m_fPwmMax);  ///////<-hardcoded!
  }
  else if(g_fZ >= 0.3 && g_fZ< 0.6){
    iPwmValue = mapfloat(g_fY, 0.3, 0.6, g_poDataMonster->m_apJoinArray[4]->m_fPwmMin, g_poDataMonster->m_apJoinArray[4]->m_fPwmMax);  ///////<-hardcoded!
  }
  else{
    iPwmValue = mapfloat(g_fY, SENSOR_X_MIN, SENSOR_X_MAX, g_poDataMonster->m_apJoinArray[4]->m_fPwmMin, g_poDataMonster->m_apJoinArray[4]->m_fPwmMax);

  }
  target[4]= iPwmValue;

  for (int i=0; i<5; i++){
    magnitude[i] = target[i] - location[i];
  }  
  for (int i=0; i<5; i++){
    force[i] = (magnitude[i] * springValue);
  }  
  for (int i=0; i<5; i++){
    velocity[i] = dampening * (velocity[i] + force[i]);
  }  
  for (int i=0; i<5; i++){
    location[i] = location[i] + velocity[i];
  }  



  // Orient the robot towards the object
  g_poDataMonster->setPosture(location[0],  location[1],  location[2], location[3], location [4], g_bGotTweet);

#endif

}

// Robot calibration interface
void calibRobot()
{
  // Update the joint position every 100 draw cycles
  g_iJointUpdateTimerCounter++;
  if ( (g_iJointUpdateTimerCounter%JOINT_UPDATE_TIMER_COUNTER_LIMIT) == 0) {

    if (g_bSettingLowLimit)  
    {
      Serial.print("Calibrating | Joint: ");
      Serial.print(g_iJointSelect);
      Serial.print(" | Limit: LOW | PWM Value: ");
      Serial.println(g_iPwmValue);
    }
    else
    {
      Serial.print("Calibrating | Joint: ");
      Serial.print(g_iJointSelect);
      Serial.print(" | Limit: HIGH | PWM Value: ");
      Serial.println(g_iPwmValue);
    }

    // Move the joint to the next PWM value
    g_poDataMonster->moveJointToCalib(g_iJointSelect, g_iPwmValue);
  }
}

// Robot calibration interface
void carlosControlRobot()
{
  //print("Calibrating Robot\n");

  // Update the joint position every 100 draw cycles
  g_iJointUpdateTimerCounter++;
  if ( (g_iJointUpdateTimerCounter%JOINT_UPDATE_TIMER_COUNTER_LIMIT) == 0) {

    // Move the joint to the next PWM value
    g_poDataMonster->moveJoint(g_iJointSelect, g_iPwmValue);

    Serial.print("Controlingd | Joint: ");
    Serial.print(g_iJointSelect);
    Serial.print(" | Limit: LOW | PWM Value: ");
    Serial.println(g_iPwmValue);
  }
}


// Robot control
void controlRobot()
{
  // print("Controlling Robot\n");

  // Control Robot Code
  carlosControlRobot(); // This replaces all the steps below. Just testing.

  // 1) Input: Get data stream / Get camera stream

  // 2) Process: Get data stream / process camera

  // 3) Ouput: Actuate Robot

    // 4) Update GUI
}

void getSerialCommand() {
  if (Serial.available() > 0) {
    // get incoming byte:
    g_iByte = Serial.read();
    //Serial.println(g_iByte);

    if (g_iByte == UP) {
      g_iPwmValue += 10;
      //g_fAngle += g_fOneDegInRad;
    } 
    else if (g_iByte == DOWN) {
      g_iPwmValue -= 10;
      //g_fAngle -= g_fOneDegInRad;
    }
    else if (g_iByte == LEFT) {

      g_iPwmValue = 128;
      //g_fAngle = 0;
      g_iJointCounter--;
    } 
    else if (g_iByte == RIGHT) {

      g_iPwmValue = 128;
      //g_fAngle = 0;
      g_iJointCounter++;
    }
  }

  if (g_iPwmValue < 0)
    g_iPwmValue = 0;
  if (g_iPwmValue > 255)
    g_iPwmValue = 255;

  if (g_iJointCounter < 0)
    g_iJointCounter = 0;
  if (g_iJointCounter > TOTAL_NUM_JOINTS-1)
    g_iJointCounter = TOTAL_NUM_JOINTS-1;

  g_iJointSelect = g_iJointCounter;
}

//////////
void blinkPin13()
{
  digitalWrite(g_iLed13, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);               // wait for a second
  digitalWrite(g_iLed13, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);               // wait for a second
}

///////////////////////////////
// Network
///////////////////////////////
void initNetwork()
{
#ifdef WIFI

  // Wifi Version

#else

  // attempt a DHCP connection:
  Serial.println("Attempting to get an IP address using DHCP:");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
  }

  /*
  if (!Ethernet.begin(mac)) {
   // if DHCP fails, start with a hard-coded address:
   Serial.println("failed to get an IP address using DHCP, trying manually");
   Ethernet.begin(mac, ip);
   }
   */
  Serial.print("My address:");
  Serial.println(Ethernet.localIP());
  // connect to Twitter:
  connectToServer();

#endif

  // give the network shield a second to initialize:
  delay(1000);
}

String checkTwitter()
{
  String sRetString = "";
  g_iTwitterPollCounter++;
  //if(g_iTwitterPollCounter%TWITTER_POLLING_TIME == 0) // Check Tweeter every ~5 seconds
  //if(g_iTwitterPollCounter == TWITTER_POLLING_TIME) // Check Tweeter every ~5 seconds
  //  if( (g_iTwitterPollCounter%TWITTER_POLLING_TIME) == 0) // Check Tweeter every ~5 seconds
  {
    //g_iTwitterPollCounter = 0;
#ifdef WIFI

    sRetString = checkTwitterWiFi();

#else

    sRetString = checkTwitterEthernet();
    //   Serial.println("*********************** HERE");

#endif

  }

  return sRetString;
}


// String g_sJsonString = "";
// boolean readingJsonString = false;

String checkTwitterEthernet()
{
  String sRetString = "";

  if (client.connected()) {
    if (client.available()) {
      // read incoming bytes:
      char inChar = client.read();

      // add incoming byte to end of line:
      //  currentLine += inChar; 

      // if you get a newline, clear the line:
      //     if (inChar == '\n') {
      //       currentLine = "";
      //     } 
      // if the current line ends with <text>, it will
      // be followed by the tweet:
      if ( inChar == '{' ) {
        // tweet is beginning. Clear the tweet string:
        readingJsonString = true;         
        g_sJsonString = "";
      }
      // if you're currently reading the bytes of a tweet,
      // add them to the tweet String:
      if (readingJsonString) {
        if (inChar != '}') {
          g_sJsonString += inChar;
        } 
        else {
          // if you got a "<" character,
          // you've reached the end of the tweet:
          g_sJsonString += '}';
          readingJsonString = false;
          sRetString = g_sJsonString;
          Serial.println(g_sJsonString);   
          // close the connection to the server:
          client.stop(); 
        }
      }
    }   
  }
  else if (millis() - g_iLastAttemptTime > g_iRequestInterval) {
    // if you're not connected, and two minutes have passed since
    // your last connection, then attempt to connect again:
    connectToServer();
  }

  return sRetString;
}

void connectToServer() {

#ifdef WIFI

  // WiFi Version

#else

  // attempt to connect, and wait a millisecond:
  Serial.println("connecting to server...");
  if (client.connect(server, 80)) {
    Serial.println("making HTTP request...");
    // make HTTP GET request to twitter:
    client.println(g_sHttpRequest); // ThingsSpeak
    client.println();
  }

#endif

  // note the time of this connect attempt:
  g_iLastAttemptTime = millis();
}   

String checkTwitterWiFi()
{

}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


