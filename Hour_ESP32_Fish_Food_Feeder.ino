#include <FS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#ifdef ESP32
  #include <SPIFFS.h>
  #define FILESYSTEM SPIFFS
#elif defined(ESP8266)
  #include <LittleFS.h>
  #define FILESYSTEM LittleFS
#endif

#include <Stepper.h>

const int stepsPerRevolution = 2048;

#define IN1 19
#define IN2 18
#define IN3 5
#define IN4 17
#define BTN_CENTER 16
#define BTN_RIGHT 4
#define BTN_LEFT 26

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32 // Ensure this matches your display height
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

Stepper myStepper(stepsPerRevolution, IN1, IN3, IN2, IN4);

unsigned long lastRunTime = 0;
unsigned long buttonPressTime = 0;
unsigned long nextFeedTime = 0;

int hours = 0;
int minutes = 0;
int seconds = 0;
int stepsToMove = 0;
bool settingMode = false;
bool stepSetting = false;
bool settingsSaved = false;

// Function prototype for getTotalInterval
int getTotalInterval();

void setup() {
    Serial.begin(115200);
    myStepper.setSpeed(10);

    pinMode(BTN_CENTER, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_LEFT, INPUT_PULLUP);

    if (!FILESYSTEM.begin()) {
        Serial.println("Filesystem Mount Failed");
        return;
    }

    // Initialize the display with the correct I2C address and dimensions
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 allocation failed");
        for (;;); // Don't proceed, loop forever
    }

    display.clearDisplay(); // Clear the display buffer
    animateText("Hello I'm YaYa");
    delay(1000);
    loadSettings();
    
    if (!settingsSaved) {
        displayInstructions("Hold Center Btn", "3 Seconds to Set Time");
    } else {
        lastRunTime = millis();
        nextFeedTime = lastRunTime + getTotalInterval() * 1000UL; // Set nextFeedTime on startup
        displayInstructions("Loaded", "Ready to Run");
    }

    Serial.println("Hold center button (3 sec) to set time.");
}

void loop() {
    unsigned long currentTime = millis();

    if (digitalRead(BTN_CENTER) == LOW) {
        buttonPressTime = millis();
        while (digitalRead(BTN_CENTER) == LOW) {
            if (millis() - buttonPressTime >= 3000) {
                settingMode = true;
                stepSetting = false;
                displayInstructions("Set Interval", "Set Hours First");
                Serial.println("Setting Mode Activated!");
                delay(500);
                return;
            }
        }
    }

    if (settingMode) {
        setInterval();
        setSteps();
        saveSettings();
        settingsSaved = true;
        settingMode = false;
        displayInstructions("Settings Saved!", "Running...");
        delay(500);
    }

    if (settingsSaved) {
        runMotor(currentTime);
    }

    if (digitalRead(BTN_RIGHT) == LOW && digitalRead(BTN_LEFT) == LOW) {
        clearSettings();
    }
}

void runMotor(unsigned long currentTime) {
    unsigned long totalInterval = getTotalInterval();  // Total interval in seconds

    // Ensure that nextFeedTime is correctly initialized before first use
    if (totalInterval > 0 && stepsToMove > 0) {
        // Check if nextFeedTime is valid, if not reset it to 0
        if (nextFeedTime == 0) {
            nextFeedTime = currentTime + totalInterval * 1000UL; // Calculate nextFeedTime
            Serial.print("Initializing nextFeedTime: ");
            Serial.println(nextFeedTime);  // Debug: initial value of nextFeedTime
        }

        // Run motor if the interval is reached
        if (currentTime - lastRunTime >= totalInterval * 1000UL) {
            lastRunTime = currentTime;  // Update last run time
            nextFeedTime = currentTime + totalInterval * 1000UL;  // Update next feed time
            
            displayInstructions1("Food", "Feeding...");
            Serial.println("Motor running...");
            myStepper.step(stepsToMove);  // Run the motor
            Serial.println("Motor stopped. Waiting...");
        } else {
            // Calculate the remaining time in seconds
            unsigned long remainingTime = (nextFeedTime - currentTime) / 1000;  // Remaining time in seconds

            // Ensure remaining time doesn't show as a large negative number
            if (remainingTime < 0) {
                remainingTime = 0;  // Reset to zero if negative
                Serial.println("Negative remaining time! Resetting.");
            }

            // Debugging output to check remaining time calculation
            Serial.print("Remaining Time: ");
            Serial.println(remainingTime);  // Debugging: Print remaining time in seconds

            // Display the remaining time on the OLED
            updateOLED("Next Feed in:", formatCountdown(remainingTime));  // Show the countdown on OLED
        }
    } else {
        // In case of invalid settings
        Serial.println("Settings incomplete! Hold center button (3 sec) to set.");
        displayInstructions("Error", "Set Time & Steps");
        settingsSaved = false;  // Mark settings as incomplete
        delay(2000);  // Delay to display the error message
    }
}

String formatCountdown(unsigned long remainingSeconds) {
    int h = remainingSeconds / 3600;
    int m = (remainingSeconds % 3600) / 60;
    int s = remainingSeconds % 60;

    // Format the countdown in HH:MM:SS format with leading zeros
    String formattedCountdown = String(h < 10 ? "0" + String(h) : String(h)) + ":" +
                               String(m < 10 ? "0" + String(m) : String(m)) + ":" +
                               String(s < 10 ? "0" + String(s) : String(s));
    return formattedCountdown;
}

// Function to calculate total interval in seconds
int getTotalInterval() {
    return hours * 3600 + minutes * 60 + seconds;
}

void setInterval() {
    // Set Hours
    updateOLED("Set Hours", String(hours));  // Display current value of hours

    while (true) {
        displayButtonInstructions("+", "OK", "-");

        if (digitalRead(BTN_RIGHT) == LOW) {
            hours++;
            if (hours >= 24) hours = 0; // Wrap around if needed
            Serial.println("Hours: " + String(hours)); // Debugging line
            updateOLED("Set Hours", String(hours)); // Update display with new hours
            delay(200); // Debounce delay
        }

        if (digitalRead(BTN_LEFT) == LOW) {
            hours--;
            if (hours < 0) hours = 23; // Wrap around if needed
            Serial.println("Hours: " + String(hours)); // Debugging line
            updateOLED("Set Hours", String(hours)); // Update display with new hours
            delay(200); // Debounce delay
        }

        if (digitalRead(BTN_CENTER) == LOW) {
            stepSetting = true; // Move to minutes setting
            displayInstructions("Set Minutes", String(minutes));
            delay(500);
            break;
        }
    }

    // Set Minutes
    while (stepSetting && minutes < 60) {
        updateOLED("Set Minutes", String(minutes));  // Display current minutes value
        displayButtonInstructions("+", "OK", "-");

        if (digitalRead(BTN_RIGHT) == LOW) {
            minutes++;
            if (minutes >= 60) minutes = 0; // Wrap around to 0 when 60 is reached
            Serial.println("Minutes: " + String(minutes)); // Debugging line
            updateOLED("Set Minutes", String(minutes)); // Update display with new minutes
            delay(200); // Debounce delay
        }

        if (digitalRead(BTN_LEFT) == LOW) {
            minutes--;
            if (minutes < 0) minutes = 59; // Wrap around to 59 if minutes go below 0
            Serial.println("Minutes: " + String(minutes)); // Debugging line
            updateOLED("Set Minutes", String(minutes)); // Update display with new minutes
            delay(200); // Debounce delay
        }

        if (digitalRead(BTN_CENTER) == LOW) {
            stepSetting = false; // Move to seconds setting
            displayInstructions("Set Seconds", String(seconds));
            delay(500);
            break;
        }
    }

    // Set Seconds
    while (!stepSetting && seconds < 60) {
        updateOLED("Set Seconds", String(seconds));  // Display current seconds value
        displayButtonInstructions("+", "OK", "-");

        if (digitalRead(BTN_RIGHT) == LOW) {
            seconds++;
            if (seconds >= 60) seconds = 0; // Wrap around to 0 if seconds reach 60
            Serial.println("Seconds: " + String(seconds)); // Debugging line
            updateOLED("Set Seconds", String(seconds)); // Update display with new seconds
            delay(200); // Debounce delay
        }

        if (digitalRead(BTN_LEFT) == LOW) {
            seconds--;
            if (seconds < 0) seconds = 59; // Wrap around to 59 if seconds go below 0
            Serial.println("Seconds: " + String(seconds)); // Debugging line
            updateOLED("Set Seconds", String(seconds)); // Update display with new seconds
            delay(200); // Debounce delay
        }

        if (digitalRead(BTN_CENTER) == LOW) {
            stepSetting = true; // Move to steps setting
            displayInstructions("Set Steps", String(stepsToMove));
            delay(500);
            break;
        }
    }
}


void setSteps() {
   while (stepSetting) {
       updateOLED("Set Steps", String(stepsToMove));
       displayButtonInstructions("+", "OK", "-");

       if (digitalRead(BTN_RIGHT) == LOW) {
           stepsToMove++;
           delay(200);
       }

       if (digitalRead(BTN_LEFT) == LOW && stepsToMove > 0) {
           stepsToMove--;
           delay(200);
       }

       if (digitalRead(BTN_CENTER) == LOW) {
           buttonPressTime = millis();
           displayInstructions1("Hold OK Btn", "5 sec");
           Serial.println("Please hold for 5 seconds to save.");
           while (digitalRead(BTN_CENTER) == LOW) {
               if (millis() - buttonPressTime >= 5000) {
                   Serial.println("Settings Saved!");
                   saveSettings();
                   settingsSaved = true;
                   stepSetting = false; 
                   displayInstructions1("Saved!", "Restarting...");
                   delay(500);
                   
                   // Restart the board
                   ESP.restart(); // Use this line to restart ESP32/ESP8266
                   return; // Optional: Return after restart
               }
           }
       }
   }
}




void updateOLED(String line1, String line2){
   display.clearDisplay();
   display.setTextSize(1);
   display.setCursor(0, 0);
   display.println(line1);
   display.setCursor(0, 10);
   display.println(line2);
   display.display();
}

void displayInstructions(String line1, String line2){
   updateOLED(line1, line2);
}

void displayInstructions1(String line1, String line2){
   display.clearDisplay();
   display.setTextSize(1);
   display.setCursor(0, 0);
   display.println(line1);
   display.setTextSize(2);
   display.setCursor(0, 10);
   display.println(line2);
   display.display();
}

void displayButtonInstructions(String line1, String line2, String line3){
   display.setCursor(0, 20);
   display.println(line1);
   display.setCursor(50, 20);
   display.println(line2);
   display.setCursor(100, 20);
   display.println(line3);
   display.display();
}

void loadSettings() {
   File file = FILESYSTEM.open("/settings.txt", "r");

   if (!file){
       Serial.println("No previous settings found, please configure.");
       return;
   }

   String data = file.readStringUntil('\n');
   file.close();

   // Debugging: Show the raw data read from the file
   Serial.println("Raw Data from File: " + data);

   // Parse the data correctly using a comma as the delimiter
   int commaIndex[3];

   // Find the commas and parse the data
   commaIndex[0] = data.indexOf(',');
   commaIndex[1] = data.indexOf(',', commaIndex[0] + 1);
   commaIndex[2] = data.indexOf(',', commaIndex[1] + 1);

   if (commaIndex[0] != -1 && commaIndex[1] != -1 && commaIndex[2] != -1) {
       hours = data.substring(0, commaIndex[0]).toInt();
       minutes = data.substring(commaIndex[0] + 1, commaIndex[1]).toInt();
       seconds = data.substring(commaIndex[1] + 1, commaIndex[2]).toInt();
       stepsToMove = data.substring(commaIndex[2] + 1).toInt();
   } else {
       // Default values if the parsing fails
       hours = 0;
       minutes = 0;
       seconds = 0;
       stepsToMove = 0;
   }

   // Debugging: Show the parsed values
   Serial.print("Loaded Settings - ");
   Serial.print("Hours: "); Serial.print(hours);
   Serial.print(", Minutes: "); Serial.print(minutes);
   Serial.print(", Seconds: "); Serial.print(seconds);
   Serial.print(", Steps: "); Serial.println(stepsToMove);

   settingsSaved = true;

   // Now display the settings on OLED right after loading them
   updateOLED("Loaded", String(hours) + ":" + String(minutes) + ":" + String(seconds));
}

void saveSettings() {
   File file = FILESYSTEM.open("/settings.txt", "w");

   if (!file){
       Serial.println("Failed to open settings file for writing");
       return;
   }

   // Save hours, minutes, seconds, and steps as comma-separated values
   file.print(hours); file.print(",");
   file.print(minutes); file.print(",");
   file.print(seconds); file.print(",");
   file.print(stepsToMove);

   file.close();
   
   Serial.println("Settings saved to SPIFFS/LittleFS");

   // Debugging: Print the saved values
   Serial.print("Saved Settings - ");
   Serial.print("Hours: "); Serial.print(hours);
   Serial.print(", Minutes: "); Serial.print(minutes);
   Serial.print(", Seconds: "); Serial.print(seconds);
   Serial.print(", Steps: "); Serial.println(stepsToMove);
}



void clearSettings(){
    if(FILESYSTEM.exists("/settings.txt")){ 
         FILESYSTEM.remove("/settings.txt"); 
         Serial.println("Settings cleared!"); 
         updateOLED("Clear", "");
         delay(1000); 
         updateOLED("", "Hold Center Btn"); 
         delay(100); 
         updateOLED("", "3 Sec to Set"); 
     }else{ 
         Serial.println("No settings file to delete."); 
     } 

     myStepper.step(0); // Stop the motor 
}

void animateText(String text) {
     display.clearDisplay(); // Clear the display before starting animation
     display.setTextSize(2);
     display.setTextColor(SSD1306_WHITE);
     for (int i=0; i<=text.length(); i++) {
         display.clearDisplay(); // Clear previous text
         display.setCursor(0,0); // Reset cursor position
         display.print(text.substring(0,i));
         display.display(); // Update the display
         delay(150); // Adjust delay for animation speed
     }  
}
