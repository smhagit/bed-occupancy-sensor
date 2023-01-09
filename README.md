# Bed occupancy sensor
HX711 Load cell connected to NodeMCU (esp8266) and with integrated WiFi and MQTT.

The load cells are not calibrated, because I do not want to measure my weight but only know if someone is lying in bed :)

The measured and uncalibrated values are published into separate MQTT topics and picked up by Home assistant.

# Used Hardware

### HX711 Load Cell
I bought mine at Ali Express. That was a set consisting of four load cells and one hx711 chip. You can use the hx711 with one, two, three or four load cells. Each set was about 6€ incl. shipping cost from china.

Because I wanted to build a bed sensor, I used four load cells and one hx711 which comes to a total load of 200kg (4x 50kg). And that for both sides of the bed.


### NodeMCU (ESP8266) 
Is used the nodeMCU with an ESP8266 chip and the wired hx711 chip. 
