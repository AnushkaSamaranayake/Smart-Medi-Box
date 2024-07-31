# Smart-Medi-Box
Smart Medi Box is a tool that allows a person to store his or her medicines in the proper conditions. It alerts people when it is time for them to take the medication. By the time it measures the temperature and the humidity inside the box. If these conditions are not in the proper conditions then it will alert. 

# Design Methodology
When designing the Medi-Box, there were main features to add. They are, showing the current time, setting the time manually, setting the time by timezones, setting alarms for reminders to take medication, alerts when humidity and temperature are not in good condition, and covering the box using a slider window according to the light intensity. This device is smart. IoT is used to build this device. The graphical representation of the temperature and humidity, an indication of which slider door is closed are included in the dashboard of the device. Also, we can select the medicine type and give it as an input to the device to keep proper conditions inside the box.

# Main Components used.
As the main microcontroller, ESP32 is used to build the device.
Photoresistor (LDR) sensors, DHT22 Temp and Humidity sensors are mainly used as sensors and as actuators, servo-motors are used to control the slider windows.
