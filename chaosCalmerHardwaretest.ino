
bool debug = true;


// constants 
const int valveLatency = 50; // could be less 1/50 * 1000 = 20ms
const long checkPressureInterval = 1000; // checking for every second should be enough
const int maxTankFillTries = 10; // randomly set, for tankFillPressure
const int pressureReleaseTime = 10; // secs, 5 was not enough
const long pneumaticActivationTimeFast = 1000; // ms
const long pneumaticActivationTimeSlow = 500; // ms
const long pneumaticRefreshTime = 50;

//ugly global variable
int serialInt = 0;
char serialChar = 0;
String serialString = "";


// for millis
long interval01 = 500; // analogRead          
long interval02 = 0;  // fillTank procedure handler, inveral variates depending on the step       
long interval03 = 1000; // check for commands from serial -> ", "
long interval04 = 1000; // check for argument for command ->  "."
long interval05 = 1; // pneumatic activationTime
long interval06 = 1000;

unsigned long prevMillis01 = 0;
unsigned long prevMillis02 = 0;
unsigned long prevMillis03 = 0;
unsigned long prevMillis04 = 0;
unsigned long prevMillis05 = 0;
unsigned long prevMillis06 = 0;

// activate interval (at activation set previous to millis)
bool shouldInterval01 = true; // update pressure value periodically
bool shouldInterval02 = false;
bool shouldInterval03 = false;
bool shouldInterval04 = false;
bool shouldInterval05 = false;
bool shouldInterval06 = false;

// states for millis based stuff
bool shouldEmptyTank = false;
bool shouldFillTank = false;
bool shouldCompressor = false;
bool shouldVacuum = false; // not needed yet
bool shouldBicep = false;
bool shouldShoulder = false;
bool shouldShakerClose = false; // not needed yet
bool shouldShakerSwivel = false; // not needed yet

bool shouldPneumatic = false;
bool shouldPneumaticFast = false;

bool shouldCheckCommand = true; // generally should wait for command, if false, it checks for arguments
int commandArgument = 0; // modified by handler
int lastCommand = 0; // if not 0 then call previous command (with arguments)
int currentCommand = 0; // if not 0 then call command
int argumentIndex = 0; // I hope, i dont need this

// not needed yet but, whether liquids should be kept pouring
bool shouldGin = false;
bool shouldCointreau = false;
bool shouldOrange = false;
bool shouldLime = false;
bool shouldGrenadine = false;
bool shouldWater = false;

bool statusLED = false;

// Tank
unsigned long fillTankMS = 0; // will be modified in the call, should be default 0
int tankState = 0;
int currentPressure = 0; // pressure offset?
int targetPressure = 0; // updated by handler
int tankFillTries = 0; // to detect error

unsigned long currentMillis = 0; // there is only one now

void setup() {
  currentMillis = millis();

  // initialize digital pins outputs
  pinMode(D1, OUTPUT); // IN4 Kompressor 
  pinMode(D2, OUTPUT); // IN3 INput-valve
  pinMode(D3, OUTPUT); // IN2 Emergency off (for some reason on)
  pinMode(D4, OUTPUT); // IN1 Vakuum // LED_BUILTIN
  
  pinMode(D5, OUTPUT); // IN3 // right
  pinMode(D6, OUTPUT); // IN2 // left (unused
  pinMode(D8, OUTPUT); // IN1 // mid

  // HIGH IS OFF
  digitalWrite(D1, HIGH);
  digitalWrite(D2, HIGH);
  digitalWrite(D3, HIGH);
  //digitalWrite(D4, HIGH); // shouldnt write on serial?
  digitalWrite(D5, LOW);
  digitalWrite(D6, LOW);
  digitalWrite(D8, LOW);
  //Serial.println("InitCompleate");

  digitalWrite(D4, LOW);
  emptyTank(); // it is smart to empty while waiting
  
  //digitalWrite(D4, LOW);
  //Serial.begin(115200); // need to figure out a way to make this optional
  
  //Serial.println("Waiting for Serial");
  while (!Serial) {
    Serial.begin(9600); // need to figure out a way to make this optional
    statusLED = !statusLED;
    digitalWrite(D4, statusLED);
    delay(100);
    //yield(); // wait for serial port to connect. Needed for native USB
  }
  Serial.println("Tank should be empty by now, limited airflow");

  
  //pinMode(A0, INPUT);
  // stay in setup until any command is received
  serialInt = 0;
  int initBlinkCounter = 0;
  while(serialInt == 0){
    readSerialInt();
    statusLED = !statusLED;
    digitalWrite(D4, statusLED);
    delay(1000);
  }
  // turn off, after value is received
  statusLED = true; //this is off
  digitalWrite(D4, statusLED);

  printMenu();
  
}
// the loop function runs over and over again forever

void loop() {
  checkMillis();
}

void fillTankForSecs(int secs){
  if (debug) Serial.println("Starting to fill the tank for secs...");
  tankState = 2; // 2 is without pressure check, just start umping 
  interval02 = 1;
  shouldFillTank = true;
  fillTankMS = secs*1000;
}

void fillTankForPSI(int psi){
   if (debug) Serial.println("Starting to fill the tank for psi...");
  tankState = 1; // 1 checks pressure check, just start umping 
  interval02 = 1;
  shouldFillTank = true;
  targetPressure = psi;
}

void intervalHandler01(){ // check pressure periodically, but why?
  //sendReliablyAnalog();
  currentPressure = analogRead(A0);
  //if (debug) Serial.print("IH01: Update current pressure: ");
  //if (debug) Serial.println(currentPressure);
}
void intervalHandler02(){ // fill tank procedure
  switch (tankState) {
    case 0:
      if (debug) sendReliablyStrln("Case 0: In case an error occors and this case get called, turn everything off by calling 6");
      tankState = 4; // stop procedure
      interval02 = 1;
      break;
    case 1:
      if (debug) sendReliablyStrln("Case 1: Check pressure");
      if (debug) sendReliablyStr("Current pressure: ");
      if (debug) sendReliablyStrln(String(currentPressure));
      if (debug) sendReliablyStr("Target pressure: ");
      if (debug) sendReliablyStrln(String(targetPressure));
      if (debug) sendReliablyStrln("__________"); //for overview

        currentPressure = analogRead(A0);
        if ((targetPressure != 0) && (currentPressure < targetPressure) ){
          if (debug) sendReliablyStrln("  start pumping");
          tankState = 2;
          interval02 = 1; // no need to wait but should be at least something (ms)
        } else {
          if (debug) sendReliablyStrln("  stop pumping");
          tankState = 4; // start stopping procedure
          interval02 = 1; // no need to wait but should be at least something (ms)
          targetPressure = 0; // important to stop
        }
      break;
    case 2: // pumping for duration
      if (debug) sendReliablyStrln("Case 2: Open input valve");
      digitalWrite(D3, HIGH); // close release Valve
      digitalWrite(D2, LOW); // open inputValve
      interval02 = valveLatency; // time for valve to open, could be less
      tankState = 3;
      break;
    case 3:
       if (debug) sendReliablyStrln("Case 3: start pumping");
      digitalWrite(D1, LOW); // kompressor on
      
      if(targetPressure == 0){
        if (debug) sendReliablyStr("  Fill for milli seconds: ");
        if (debug) sendReliablyStrln(String(fillTankMS));
        interval02 = fillTankMS; // how long it should keep pumping
        tankState = 4;
      } else {
        if(tankFillTries >= maxTankFillTries){
          if (debug) sendReliablyStrln("  Case 3 - ERROR: pressure does not seem to rise, please check!");
          tankState = 4;
          targetPressure = 0; // important to abort
        } else {
          if (debug) sendReliablyStrln("  Continue pumping for pressure "); 
          if (debug) sendReliablyStr("  Current Tank Fill Tries: "); 
          if (debug) sendReliablyStrln(String(tankFillTries));
          interval02 = checkPressureInterval;
          tankState = 1; // check Pressure then
          tankFillTries++; // only here at case 3
        }
      }
      break;
    case 4:
      if (debug) sendReliablyStrln("Case 4: stop pumping");
      digitalWrite(D1, HIGH); // compressor off
      interval02 = valveLatency; // time for valve to close
      tankState = 5;
      break;
    case 5:
      if (debug) sendReliablyStrln("Case 5: close input valve or iterate if target pressure is set");
      if(targetPressure > 0){
        if (debug) sendReliablyStrln("  Target pressure is set -> Iterate to Check pressure");
        tankState = 1;
        interval02 = 1; // ms
      } else {
        if (debug) sendReliablyStrln("  Close input valve .");
        digitalWrite(D2, HIGH); // close inputValve
        tankState = 6;
        interval02 = 1; // ms
      }
      break;
    case 6:
      if (debug) sendReliablyStrln("Case 6: reset duration, states");
      tankState = 0; // should be only set 0 here at case 6
      interval02 = 0;
      shouldFillTank = false;
      fillTankMS = 0; // need to be reset
      tankFillTries = 0;
      break;
    default:
      // 6< reset to 4
      if (debug) sendReliablyStrln("Case default: Error detected");
      tankState = 4; // saftly shut off
  }
}

void printMenu(){
  sendReliablyStrln("0. Do nothing");
  sendReliablyStrln("1. fillForSecs(commandArgument)");
  sendReliablyStrln("(2.) fillForSecs(commandArgument)");
  sendReliablyStrln("3. fillForPSI(commandArgument)");
  sendReliablyStrln("(4). [do not select] fillForPSI(commandArgument)");
  sendReliablyStrln("5. Empty Tank");
  sendReliablyStrln("6.");
  sendReliablyStrln("7. Right on (Arm up)");
  sendReliablyStrln("8. Right off (Arm down)");
  sendReliablyStrln("9. Mid on (Shoulder up)");
  sendReliablyStrln("10.Mid off (Shoulder down)");
  sendReliablyStrln("11.Right + mid -> on ()");
  sendReliablyStrln("12.Right + mid -> off ()");
  
}

void intervalHandler03(){ // check if there is command
  if(lastCommand == 0){
    sendReliablyStr(",");
    currentCommand = readSerialInt();
  }
  switch (currentCommand) {
    case 0:
      if (debug) sendReliablyStr(" ");
      break;
    case 1:
      if (debug) sendReliablyStrln("Ok, How many seconds to pump?");
      shouldCheckCommand = false;
      lastCommand = currentCommand; // so it executes in the next command check
      currentCommand = 2; // it is not current but the next, but yeah..
      commandArgument = 0; // reset command Argument, so that it does not take previous input
      break;
    case 2:
      fillTankForSecs(commandArgument);
      lastCommand = 0;
      commandArgument = 0; 
      break;
    case 3:
      if (debug) sendReliablyStrln("Ok, What is Target Pressure in PSI?");
      shouldCheckCommand = false;
      lastCommand = currentCommand; // so it executes in the next command check
      currentCommand = 4; // it is not current but the next, but yeah..
      commandArgument = 0; // reset command Argument, so that it does not take previous input
      break;
    case 4:
      fillTankForPSI(commandArgument);
      lastCommand = 0;
      commandArgument = 0; 
      break;
    case 5:
      if (debug) sendReliablyStrln("Ok, releasing pressure inside my heart.");
      digitalWrite(D3, LOW); // Empty tank, will be closed when pumped
      break;
    case 6:
      break;
    case 7:
      if (debug) sendReliablyStrln("Right On (Arm up)");
      activatePneumatic();
      digitalWrite(D5, HIGH);
      break;
    case 8:
      if (debug) sendReliablyStrln("Right Off (Arm down)");
      activatePneumatic();
      digitalWrite(D5, LOW);
      break;
    case 9:
      if (debug) sendReliablyStrln("Mid On (shoulder up)");
      activatePneumatic();
      digitalWrite(D8, HIGH);
      break;
    case 10:
      if (debug) sendReliablyStrln("Mid Off (shoulder down)");
      activatePneumatic();
      digitalWrite(D8, LOW);
      break;
    case 11:
      if (debug) sendReliablyStrln("Right + left On");
      activatePneumatic();
      digitalWrite(D5, HIGH);
      digitalWrite(D8, HIGH);
      break;
    case 12:
      if (debug) sendReliablyStrln("Right + left Off");
      activatePneumatic();
      digitalWrite(D5, LOW);
      digitalWrite(D8, LOW);
      break;
    default:
      sendReliablyStrln("Unknown Command! Please select from following:");
      printMenu();
      break;
  }
  
}

void activatePneumatic(){
  shouldPneumatic = true;
  shouldPneumaticFast = true; // HARDCODED
  if(shouldPneumaticFast){
    interval05 = pneumaticActivationTimeFast;
  } else {
    interval05 = pneumaticActivationTimeSlow; // do not solve this like this
  }
  updateMillisFlag();
  digitalWrite(D4, LOW); // activate arm
  if (debug) sendReliablyStrln("Arm activated");
  prevMillis05 = millis();
  
}

void intervalHandler04(){ // check for argument
  commandArgument = readSerialInt();
  if(commandArgument > 0){
    shouldCheckCommand = true;
    updateMillisFlag();
    if (debug) sendReliablyStrln("Ok, got your argument");
  } else {
    if (debug) sendReliablyStr(".");
  }
}
void intervalHandler05(){ // deactivate Pneumatic
    if (debug) sendReliablyStrln("Arm Deactivated");
    digitalWrite(D4, HIGH); // deactivate arm
    shouldPneumatic = false;
    interval05 = 1; // be instantly responsive for the next time?
    updateMillisFlag();
}
void intervalHandler06(){
  ;
}

void emptyTank(){
  digitalWrite(D3, LOW); // Empty tank at first
  delay(pressureReleaseTime); // need longer time
  yield(); // may be unncessary
  digitalWrite(D3, HIGH);
}

void depricated_fillTank(int fillTime){
  digitalWrite(D2, LOW); // open inputValve
  delay(500);
  digitalWrite(D1, LOW); // turn on compressor
  delay(fillTime);
  digitalWrite(D1, HIGH); // turn off compressor
  digitalWrite(D2, HIGH); //cloase inputValve
}

void sendReliablyAnalog(){
  while (!Serial) {
     // wait for serial port to connect. Needed for native USB
    yield();
  }
  yield();
  Serial.print("Reliable Value: ");
  Serial.println(analogRead(A0));
}

void sendReliablyStr(String message){
  while (!Serial) {
     // wait for serial port to connect. Needed for native USB
    yield();
  }
  yield();
  Serial.print(message);
}

void sendReliablyStrln(String message){
  while (!Serial) {
     // wait for serial port to connect. Needed for native USB
    yield();
  }
  yield();
  Serial.println(message);
}

void updateMillisFlag(){
  shouldInterval02 = shouldFillTank;
  shouldInterval03 = shouldCheckCommand;
  shouldInterval04 = !shouldCheckCommand; // ensuring that either command or argument is being checked
  shouldInterval05 = shouldPneumatic;
}

void checkMillis(){
  updateMillisFlag();
  
  currentMillis = millis();
  if (shouldInterval01 && (currentMillis - prevMillis01 >= interval01) ){
    // save the last time you blinked the LED
    prevMillis01 = currentMillis;
    intervalHandler01();
  }
  
  currentMillis = millis();
  if (shouldInterval02 && (currentMillis - prevMillis02 >= interval02) ){
    // save the last time you blinked the LED
    prevMillis02 = currentMillis;
    intervalHandler02();
  }

  currentMillis = millis();
  if (shouldInterval03 && (currentMillis - prevMillis03 >= interval03) ){
    // save the last time you blinked the LED
    prevMillis03 = currentMillis;
    intervalHandler03();
  }

  currentMillis = millis();
  if (shouldInterval04 && (currentMillis - prevMillis04 >= interval04) ){
    // save the last time you blinked the LED
    prevMillis04 = currentMillis;
    intervalHandler04();
  }

  currentMillis = millis();
  if (shouldInterval05 && (currentMillis - prevMillis05 >= interval05) ){
    // save the last time you blinked the LED
    prevMillis05 = currentMillis;
    intervalHandler05();
  }

  currentMillis = millis();
  if (shouldInterval06 && (currentMillis - prevMillis06 >= interval06) ){
    // save the last time you blinked the LED
    prevMillis06 = currentMillis;
    intervalHandler06();
  }
  yield();
}

int readSerialInt(){ // return int number, if nothing avaiable, then 0
  if(Serial.available() > 0){
    while (Serial.available() > 0) {    
      serialInt = Serial.parseInt();
      if (Serial.read() == '\n') {
        // do something at the end
        //Serial.print("Message: "); // just for testing
        if (debug) Serial.println(serialInt);
      }
      yield();
    }
    return serialInt; // bad usage but keeps it universal
  }else{
    return 0;
  }
}
