#include <Arduino.h>
#include "ino_helper.h"

// simple test to verify serial monitor input/output works
// this test opens a serial monitor connection and echoes back
// any characters typed on the keyboard
void setup(void) {
    // initialize serial communication at 115200 baud
    Serial.begin(115200);
    
    // wait a bit for serial monitor to connect
    delay_ms(3000);
    Serial.flush();
    
    // print startup message
    Serial.println("\n\n");
    Serial.println("SM echo test");
    Serial.println();
    Serial.println("this test will echo back any characters you type!!!!!!");
    Serial.println("the esp32 will print what it received");
    Serial.println();
    Serial.println("waiting for input...");
    Serial.println();
    Serial.flush();
}

void loop(void) {
    // check if there are characters available to read
    if (Serial.available() > 0) {
        // read a single character
        char received_char = Serial.read();
        
        // print the character back with some formatting
        Serial.print("received: '");
        
        // handle special characters that might not print well
        if (received_char == '\n') {
            Serial.print("\\n");
        } else if (received_char == '\r') {
            Serial.print("\\r");
        } else if (received_char == '\t') {
            Serial.print("\\t");
        } else if (received_char == '\b') {
            Serial.print("\\b");
        } else if (received_char >= 32 && received_char < 127) {
            // printable ascii character
            Serial.print(received_char);
        } else {
            // non-printable character, show hex value
            Serial.print("\\x");
            if ((unsigned char)received_char < 16) {
                Serial.print("0");
            }
            Serial.print((unsigned char)received_char, HEX);
        }
        
        Serial.print("' (ascii: ");
        Serial.print((int)(unsigned char)received_char);
        Serial.print(")\n");
        
        // if enter was pressed, print a newline for readability
        if (received_char == '\n' || received_char == '\r') {
            Serial.println();
        }
        
        Serial.flush();
    }
    
    // small delay to prevent cpu spinning
    delay_ms(10);
}

