## Config Guide For MQTT Mosquitto Broker

Add the following line to enable remote connections (binding to all interfaces):

listener 1883
allow_anonymous true

Run Mosquitto by:
mosquitto -v

Remember to use flag -lpaho-mqtt3c when compiling!
