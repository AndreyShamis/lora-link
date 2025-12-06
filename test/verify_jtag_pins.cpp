// Test program to verify JTAG pin connections
// Flash this to ESP32-S3, then use multimeter to check pins

#include <Arduino.h>

// ESP32-S3 JTAG pins
#define JTAG_TMS  42
#define JTAG_TCK  39
#define JTAG_TDO  40
#define JTAG_TDI  41

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== ESP32-S3 JTAG Pin Verification Test ===");
    Serial.println("This will toggle JTAG pins to verify wiring");
    Serial.println("DO NOT connect ESP-Prog during this test!");
    
    // Configure JTAG pins as outputs
    pinMode(JTAG_TMS, OUTPUT);
    pinMode(JTAG_TCK, OUTPUT);
    pinMode(JTAG_TDO, OUTPUT);
    pinMode(JTAG_TDI, OUTPUT);
    
    Serial.println("\nAll JTAG pins set LOW");
    digitalWrite(JTAG_TMS, LOW);
    digitalWrite(JTAG_TCK, LOW);
    digitalWrite(JTAG_TDO, LOW);
    digitalWrite(JTAG_TDI, LOW);
}

void loop() {
    Serial.println("\n--- Toggling pins ---");
    
    Serial.println("TMS (GPIO42) HIGH");
    digitalWrite(JTAG_TMS, HIGH);
    delay(2000);
    digitalWrite(JTAG_TMS, LOW);
    delay(500);
    
    Serial.println("TCK (GPIO39) HIGH");
    digitalWrite(JTAG_TCK, HIGH);
    delay(2000);
    digitalWrite(JTAG_TCK, LOW);
    delay(500);
    
    Serial.println("TDO (GPIO40) HIGH");
    digitalWrite(JTAG_TDO, HIGH);
    delay(2000);
    digitalWrite(JTAG_TDO, LOW);
    delay(500);
    
    Serial.println("TDI (GPIO41) HIGH");
    digitalWrite(JTAG_TDI, HIGH);
    delay(2000);
    digitalWrite(JTAG_TDI, LOW);
    delay(500);
    
    Serial.println("Cycle complete. Waiting 3s...\n");
    delay(3000);
}
