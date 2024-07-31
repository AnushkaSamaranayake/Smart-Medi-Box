#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

//define screen details for the OLED display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

//define the Pins
#define BUZZER 4 //pin 5 --> BUZZER
#define LED_1 15 //pin 15 --> LED_1
#define PB_CANCEL 23 //pin 34 --> PB_CANCEL
#define PB_OK 18 //pin 32 --> PB_OK
#define PB_UP 5 //pin 33 --> PB_UP
#define PB_DOWN 19 //pin 35 --> PB_DOWN
#define DHTPIN 12 //pin 12 --> DHTPIN

const int ldr_left = 34;
const int ldr_right = 35;

//for time update
#define NTP_SERVER "pool.ntp.org"
#define UTC_OFFSET_DST 0 
long UTC_OFFSET = 0;

//Declare objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;
Servo myservo;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

//default values for servo motor
int pos_offset = 30;
float r = 0.75;

//Global Variables
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

int med_type;
float pos = 0;
float I = 0;
float D_servo = 0;
float ldrValue_left = 0;
float ldrValue_right = 0;

char ldrArLeft[6];
char ldrArRight[6];
char tempAr[6];
char humiAr[6];

//for buzzer, defined notes
int C = 262; //note C
int D = 294; //note D
int E = 330; //note E
int F = 349; //note F
int G = 392; //note G
int A = 440; //note A
int B = 494; //note B
int C_H = 523; //note C_H
int notes[] = {C, D, E, F, G, A, B, C_H};
int n = 8;

unsigned long timeNow = 0;
unsigned long timeLast = 0;

bool alarm_enabled = true; //turn all alarm on
int n_alarms = 3; // keep track of the alarm
int alarm_hours[] = {0, 1, 12}; // set hours for three alarms
int alarm_minutes[] = {1, 0, 0}; // set minutes for three alarms
bool alarm_triggered[] = {false, false,false}; //keep the track whether alarm triggered

int current_mode = 0; //current mode in the menu
int max_modes = 6; //number of modes in the menu
//Modes of the menu
String modes[] = {"1 - Set Time", "2 - Set time zone", "3 - Set Alarm 1", "4 - Set Alarm 2", "5 - Set Alarm 3", "6 - Disable Alarm"};

void setup() {
  //define the OUTPUT and INPUT pins
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);

  dhtSensor.setup(DHTPIN, DHTesp::DHT22); //configure the sensor setup
  myservo.attach(2);
  //Intialize serial monitor and OLED display
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;) ;
  }

  //turn on OLED display
  display.display();
  delay(2000);

  //Connect to WIFI and mqtt client
  setupWiFi();
  setupMqtt();

  display.clearDisplay();
  print_line("Connected to WiFi", 0, 0, 2); // Display after wifi is connected
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);//Update the time

  display.clearDisplay();
  print_line("Welcome to Medibox", 10, 20, 2); //welcome message
  display.clearDisplay();
}

void loop() {
  if(!mqttClient.connected()){
    connectToBroker();
  }

  mqttClient.loop();

  update_time_with_check_alarm();
  if (digitalRead(PB_OK) == LOW) {
    delay(200);
    go_to_menu();
  }
  check_temp_and_humidity();

  ldrValue_left = analogRead(ldr_left);
  ldrValue_left = ldrValue_left/4096;
  ldrValue_right = analogRead(ldr_right);
  ldrValue_right = ldrValue_right/4096;

  updateLdrValues();
  servoslider();

  // Publish LDR values
  mqttClient.publish("MedBOX-LDR_LEFT", ldrArLeft);
  mqttClient.publish("MedBOX-LDR_RIGHT", ldrArRight);
  mqttClient.publish("MedBOX-Temp", tempAr);
  mqttClient.publish("MedBOX-Humi", humiAr);

  if (ldrValue_left>ldrValue_right){
    mqttClient.publish("MedBOX-HIGHINT","LDR-LEFT");
  }
  else if(ldrValue_left<ldrValue_right){
    mqttClient.publish("MedBOX-HIGHINT","LDR_RIGHT");
  }

  delay(1000);
}

void LED_Blink() {
  digitalWrite(LED_1, HIGH);
  delay(500);
  digitalWrite(LED_1, LOW);
  delay(500);
}

void print_line(String text, int column, int row, int text_size) {
  //this will display anything in the display according to paras
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);
  display.display();
}

void print_time_now() {
  //time will be displayed in the display
  display.clearDisplay();
  print_line(String(days), 0, 0, 2);
  print_line(":", 20, 0, 2);
  print_line(String(hours), 30, 0, 2);
  print_line(":", 50, 0, 2);
  print_line(String(minutes), 60, 0, 2);
  print_line(":", 80, 0, 2);
  print_line(String(seconds), 90, 0, 2);
}

void update_time() {
   /*time will be updated through WIFI automatically*/
  /*get time data from the internet and assign them to following global variables to display*/
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  hours = atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute, 3, "%M", &timeinfo);
  minutes = atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  seconds = atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay, 3, "%d", &timeinfo);
  days = atoi(timeDay);
}

void ring_alarm() {
  //Function to ring the alarm
  display.clearDisplay();
  print_line("MEDICINE TIME!", 0, 0, 2); //Display when alarm triggered
  LED_Blink(); //Blink the LED when alarm triggered

  bool break_happened = false; //alarm status

  while (!break_happened && digitalRead(PB_CANCEL) == HIGH) {
    for (int i = 0; i < n; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {
        delay(200);
        break_happened = true; //stop the alarm when PB_cancle is pressed
        break;
      }
      tone(BUZZER, notes[i]); //ring the buzzer
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  digitalWrite(LED_1, LOW);
  display.clearDisplay();
}

void update_time_with_check_alarm() {
  //function to update the time with alarm
  update_time();
  print_time_now();

  if (alarm_enabled) {
    for (int i = 0; i < n_alarms; i++) {
      if (!alarm_triggered[i] && alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
        ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  }
}

int wait_for_button_press() {
  //function for buttons
  while (true) {
    if (digitalRead(PB_UP) == LOW) {
      delay(200);
      return PB_UP;
    } else if (digitalRead(PB_DOWN) == LOW) {
      delay(200);
      return PB_DOWN;
    } else if (digitalRead(PB_OK) == LOW) {
      delay(200);
      return PB_OK;
    } else if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      return PB_CANCEL;
    }
    update_time();
  }
}

void go_to_menu() {
  //function for the menu access
  while (digitalRead(PB_CANCEL) == HIGH) {
    display.clearDisplay();
    print_line(modes[current_mode], 0, 0, 2);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {
      delay(200);
      current_mode = (current_mode + 1) % max_modes;
    } else if (pressed == PB_DOWN) {
      delay(200);
      current_mode = (current_mode - 1 + max_modes) % max_modes;
    } else if (pressed == PB_OK) {
      delay(200);
      run_mode(current_mode);
    } else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
}

void set_time_zone() {
  //function for setting the time zone
  float temp_zone = 0;
  while (true) {
    display.clearDisplay();
    print_line("Enter time zone: " + String(temp_zone), 0, 0, 2);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {
      delay(200);
      temp_zone += 0.5;
      temp_zone = fmod(temp_zone, 14);
      if (temp_zone > 14) {
        temp_zone = -11;
      }
    } else if (pressed == PB_DOWN) {
      delay(200);
      temp_zone -= 0.5;
      if (temp_zone < -11) {
        temp_zone = 14;
      }
    } else if (pressed == PB_OK) {
      delay(200);
      UTC_OFFSET = temp_zone*3600;
      break;
    } else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
  display.clearDisplay();
  print_line("Time Zone is set", 0, 0, 2);
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
  delay(1000);
}

void set_time() {
  //function to set the time from menu
  int temp_hour = hours;
  while (true) {
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 0, 0, 2);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {
      delay(200);
      temp_hour = (temp_hour + 1) % 24;
    } else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour = (temp_hour - 1 + 24) % 24;
    } else if (pressed == PB_OK) {
      delay(200);
      hours = temp_hour;
      break;
    } else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  int temp_minute = minutes;
  while (true) {
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute), 0, 0, 2);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {
      delay(200);
      temp_minute = (temp_minute + 1) % 60;
    } else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute = (temp_minute - 1 + 60) % 60;
    } else if (pressed == PB_OK) {
      delay(200);
      minutes = temp_minute;
      break;
    } else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  display.clearDisplay();
  print_line("Time is set", 0, 0, 2);
  update_time();
  delay(1000);
}

void set_alarm(int alarm) {
  //function to set the alarm
  int temp_hour = alarm_hours[alarm];
  while (true) {
    display.clearDisplay();
    print_line("Enter alarm hour: " + String(temp_hour), 0, 0, 2);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {
      delay(200);
      temp_hour = (temp_hour + 1) % 24;
    } else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour = (temp_hour - 1 + 24) % 24;
    } else if (pressed == PB_OK) {
      delay(200);
      alarm_hours[alarm] = temp_hour;
      break;
    } else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  int temp_minute = alarm_minutes[alarm];
  while (true) {
    display.clearDisplay();
    print_line("Enter alarm minutes: " + String(temp_minute), 0, 0, 2);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {
      delay(200);
      temp_minute = (temp_minute + 1) % 60;
    } else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute = (temp_minute - 1 + 60) % 60;
    } else if (pressed == PB_OK) {
      delay(200);
      alarm_minutes[alarm] = temp_minute;
      break;
    } else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  display.clearDisplay();
  print_line("Alarm is set", 0, 0, 2);
  delay(1000);
}

void run_mode(int mode) {
  //function to identify the mode
  if (mode == 0) {
    set_time();
  }
  if (mode == 1){
    set_time_zone();
  }
  if (mode == 2 || mode == 3 || mode == 4) {
    set_alarm(mode - 1);
  }
  if (mode == 5) {
    alarm_enabled = false;
  }
}

void check_temp_and_humidity() {
  // function to check the temprature and humidity
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature, 2).toCharArray(tempAr,6);
  String(data.humidity, 2).toCharArray(humiAr,6);

  if (data.temperature >= 32) {
    display.clearDisplay();
    print_line("TEMP_HIGH", 0, 40, 1);
    LED_Blink();
  }
  else if (data.temperature <= 26) {
    display.clearDisplay();
    print_line("TEMP_LOW", 0, 40, 1);
    LED_Blink();
  }

  if (data.humidity >= 80) {
    display.clearDisplay();
    print_line("HUMIDITY_HIGH", 0, 50, 1);
    LED_Blink();
  }
  else if (data.humidity <= 60) {
    display.clearDisplay();
    print_line("HUMIDITY_LOW", 0, 50, 1);
    LED_Blink();
  }
}

void setupWiFi(){
  WiFi.begin("Wokwi-GUEST", "");

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address");
  Serial.println(WiFi.localIP());
}

void setupMqtt(){
  mqttClient.setServer("test.mosquitto.org",1883);
  mqttClient.setCallback(receiveCallback);
}

void connectToBroker(){
  while(!mqttClient.connected()){
    Serial.print("Attempting MQTT connection...");
    if(mqttClient.connect("ESP32-5342356436523")){
      Serial.println("Connected");
      mqttClient.subscribe("MedBOX-min-angle");
      mqttClient.subscribe("MedBOX-Ctrl-factor");
      mqttClient.subscribe("MedBOX-Med-type");
    }
    else{
      Serial.println("failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void receiveCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char payloadCharAr[length + 1]; // Increase size to accommodate null terminator

  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }
  payloadCharAr[length] = '\0'; // Add null terminator to make it a valid C string

  Serial.println();

  // Process "MedBOX-min-angle" topic
  if (strcmp(topic, "MedBOX-min-angle") == 0) {
    int new_pos_offset = atoi(payloadCharAr); // Convert char array to integer
    if (new_pos_offset != pos_offset) { // Check if value has changed
      pos_offset = new_pos_offset; // Update pos_offset
    }
  }

  // Process "MedBOX-Ctrl-factor" topic
  if (strcmp(topic, "MedBOX-Ctrl-factor") == 0) {
    float new_r = atof(payloadCharAr); // Convert char array to float
    if (new_r != r) { // Check if value has changed
      r = new_r; // Update r
    }
  }

  // Process "MedBOX-med-type" topic
  if (strcmp(topic, "MedBOX-Med-type") == 0) {
    int new_med_type = atoi(payloadCharAr); // Convert char array to float
    if (new_med_type != med_type) { // Check if value has changed
      med_type = new_med_type; // Update Medicine Type
    }
  }
}

void updateLdrValues(){
  snprintf(ldrArLeft, 6, "%f", ldrValue_left);
  snprintf(ldrArRight, 6, "%f", ldrValue_right);
}

void servoslider(){
  if (ldrValue_left>ldrValue_right){
    D_servo = 1.5;
    I = ldrValue_left;
  }
  else if (ldrValue_left<ldrValue_right){
    D_servo = 0.5;
    I = ldrValue_right;
  }

  if(med_type != 0){
    pos_offset = 30;
    r = 0.75;
  }
  else if (med_type == 0){
    String pos_offset_str = String(pos_offset);
    String r_str = String(r);
    Serial.println("pos_offset: "+pos_offset_str);
    Serial.println("Gamma: "+r_str);
  }

  pos = pos_offset*D_servo + (180-pos_offset)*I*r;
  myservo.write(pos);
  delay(500);
}
