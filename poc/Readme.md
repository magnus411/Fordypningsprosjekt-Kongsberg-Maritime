# Proof of concepts

This directory contains parts of the project that were not completely finished since they were not a priority or old code that was part of the program at some point.

## gui
Contains the backend for a website that can display the sensor data from a database.

## Metrics
This was used to perform analysis of throughput and occupancy in the circular buffer, but has not been implemented for the pipe that replaced the circular buffer.

## MQTT
Start of an MQTT implementation for the old module-based architecture. We decided to focus on just Modbus for the communication protocol, so this is no longer used.

## ZeroCompression
Implementation of compression and storage in a PostgreSQL database of 0-data. This was completed too late in development to integrate with the PostgreSQL part of the data handler.
