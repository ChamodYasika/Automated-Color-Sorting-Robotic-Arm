#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// --- HARDWARE CONFIGURATION ---
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Servo Settings
#define SERVOMIN  170 
#define SERVOMAX  550 
#define SERVO_FREQ 40 
#define MOVESPEED  15  // Lower = Faster, Higher = Slower

// Servo Channels
const int BASE = 0;
const int SHOULDER = 1;
const int ELBOW = 2;
const int GRIPPER = 3;

// Color Sensor Pins (ESP32)
#define S0 32
#define S1 33
#define S2 25
#define S3 26
#define sensorOut 27

// --- GLOBAL VARIABLES ---
// Current Positions (Initialized to HOME position)
int currentBase = 110;
int currentShoulder = 70;
int currentElbow = 100;
int currentGripper = 35;

// Sensor Variables
int redFreq = 0;
int greenFreq = 0;
int blueFreq = 0;

void setup() {
  Serial.begin(115200);
  
  // 1. Initialize Color Sensor
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(sensorOut, INPUT);
  
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW); // 20% Frequency Scaling

  // 2. Initialize Servo Driver
  pwm.begin();
  pwm.setPWMFreq(SERVO_FREQ);

  // 3. Move Arm to HOME Position Immediately (No jerk)
  setAngleInstant(BASE, currentBase);
  setAngleInstant(SHOULDER, currentShoulder);
  setAngleInstant(ELBOW, currentElbow);
  setAngleInstant(GRIPPER, currentGripper);

  Serial.println("--- SYSTEM READY: Waiting for Object ---");
  delay(1000);
}

void loop() {
  // --- STEP 1: READ COLOR SENSOR ---
  readColorSensor();

  // --- STEP 2: CHECK FOR OBJECTS ---
  // Using your threshold: If all > 400, no object is present.
  if (redFreq > 400 && greenFreq > 400 && blueFreq > 400) {
    // No object found, stay in loop and keep checking
    // Serial.println("Scanning..."); 
  } 
  else {
    // --- STEP 3: OBJECT DETECTED! ---
    // Identify Color
    int detectedColor = 0; // 1=Red, 2=Green, 3=Blue

    if (redFreq < greenFreq && redFreq < blueFreq) {
      Serial.println(">> OBJECT FOUND: RED");
      detectedColor = 1;
    } 
    else if (greenFreq < redFreq && greenFreq < blueFreq) {
      Serial.println(">> OBJECT FOUND: GREEN");
      detectedColor = 2;
    } 
    else if (blueFreq < redFreq && blueFreq < greenFreq && blueFreq < 300) {
      Serial.println(">> OBJECT FOUND: BLUE");
      detectedColor = 3;
    }

    if (detectedColor > 0) {
      Serial.println(">> WAITING 2 SECONDS...");
      delay(2000); // User Request: Wait 2 seconds before acting

      // --- STEP 4: EXECUTE ROBOT ARM SEQUENCE ---
      performPickAndPlace(detectedColor);
      
      // Reset logic: Wait a moment so we don't detect the same object twice immediately
      Serial.println(">> SEQUENCE COMPLETE. RETURNING TO SCAN.");
      delay(1000); 
    }
  }
  
  delay(100); // Small delay for sensor stability
}

// ---------------------------------------------------------
//           ROBOT ARM SEQUENCE FUNCTIONS
// ---------------------------------------------------------

void performPickAndPlace(int color) {
  // 1. GO DOWN TO PICK (B-110, S-0, E-110, G-5)
  // Note: We are already at B-110 from Home position
  Serial.println("1. Picking Up...");
  moveSmoothly(SHOULDER, currentShoulder, 0);
  moveSmoothly(ELBOW, currentElbow, 110);
  delay(500);
  
  // Close Gripper (G-5)
  moveSmoothly(GRIPPER, currentGripper, 5); 
  delay(500);

  // 2. LIFT UP / CARRY POSITION (B-110, S-70, E-100, G-5)
  Serial.println("2. Lifting...");
  moveSmoothly(SHOULDER, currentShoulder, 70);
  moveSmoothly(ELBOW, currentElbow, 100);
  delay(500);

  // 3. MOVE TO DROP LOCATION BASED ON COLOR
  Serial.println("3. Moving to Drop Zone...");
  
  if (color == 1) { // RED (B-75, S-0, E-110, G-35)
    moveSmoothly(BASE, currentBase, 75);
    moveSmoothly(SHOULDER, currentShoulder, 0);
    moveSmoothly(ELBOW, currentElbow, 110);
  } 
  else if (color == 2) { // GREEN (B-35, S-0, E-110, G-35)
    moveSmoothly(BASE, currentBase, 35);
    moveSmoothly(SHOULDER, currentShoulder, 0);
    moveSmoothly(ELBOW, currentElbow, 110);
  } 
  else if (color == 3) { // BLUE (B-0, S-0, E-110, G-35)
    moveSmoothly(BASE, currentBase, 0);
    moveSmoothly(SHOULDER, currentShoulder, 0);
    moveSmoothly(ELBOW, currentElbow, 110);
  }

  delay(500);

  // 4. PLACE OBJECT (OPEN GRIPPER)
  Serial.println("4. Dropping...");
  moveSmoothly(GRIPPER, currentGripper, 35); 
  delay(500);

  // 5. RETURN TO HOME (B-110, S-70, E-100, G-35)
  Serial.println("5. Returning Home...");
  
  // First lift up slightly to clear the object area
  moveSmoothly(SHOULDER, currentShoulder, 70); 
  moveSmoothly(ELBOW, currentElbow, 100);
  
  // Then rotate Base back to 110
  moveSmoothly(BASE, currentBase, 110);
}

// ---------------------------------------------------------
//           HELPER FUNCTIONS (MOVEMENT & SENSOR)
// ---------------------------------------------------------

// Read the TCS230 Sensor
void readColorSensor() {
  // Read Red
  digitalWrite(S2, LOW); digitalWrite(S3, LOW);
  redFreq = pulseIn(sensorOut, LOW);
  delay(10);
  
  // Read Green
  digitalWrite(S2, HIGH); digitalWrite(S3, HIGH);
  greenFreq = pulseIn(sensorOut, LOW);
  delay(10);
  
  // Read Blue
  digitalWrite(S2, LOW); digitalWrite(S3, HIGH);
  blueFreq = pulseIn(sensorOut, LOW);
  delay(10);
}

// Move Servo Smoothly and Update Global Position
void moveSmoothly(int channel, int &startAngle, int targetAngle) {
  // Determine direction
  int step = (targetAngle > startAngle) ? 1 : -1; 

  if (startAngle != targetAngle) {
    for (int i = startAngle; i != targetAngle; i += step) {
      setAngleInstant(channel, i);
      delay(MOVESPEED); 
    }
    // Final Adjust
    setAngleInstant(channel, targetAngle);
    
    // IMPORTANT: Update the global variable
    startAngle = targetAngle;
  }
}

// Low-level PWM command
void setAngleInstant(int channel, int degrees) {
  int pulse = map(degrees, 0, 180, SERVOMIN, SERVOMAX);
  pwm.setPWM(channel, 0, pulse);
}