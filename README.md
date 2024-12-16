# Meta Power Quad - Sensor Data Handler System

## Project Overview

This project addresses the challenges of sensor data management for maritime sensor systems, specifically designed for Kongsberg Maritime's Meta Power Quad system.

## How to run:

docker-compose build (build the SensorDHS system)
docker-compose up (Run the compose with SensorDHS and Postgres)

Connect to db: docker-compose exec db psql -U postgres fordypningsprosjekt

## Key Objectives

- **Flexible Sensor Integration**: Create a modular system that allows adding new sensors without recompiling the entire codebase
- **Efficient Data Storage**: Implement intelligent data compression and storage mechanisms
- **Performance Optimization**: Design a system that can handle large volumes of sensor data efficiently
- **Zero-Value Data Management**: Develop strategies to minimize storage consumption of empty/zero-value measurements

## System Architecture

### Core Components

1. **Data Handler**:

   - Receives sensor data from various sources
   - Provides a pluggable architecture for sensor integration
   - Supports dynamic sensor type registration

2. **Storage Mechanism**:

   - Utilizes PostgreSQL for robust, scalable data storage
   - Implements Run-Length Encoding (RLE) for zero-value data compression
   - Preserves metadata for zero-value measurements

3. **Compression Strategy**:
   - Detects consecutive zero-value blocks
   - Stores timestamp and run-length information instead of full zero datasets
   - Maintains data integrity while reducing storage requirements

## Key Technical Features

- Non-invasive sensor addition
- Zero-data compression
- High-performance data handling
- Flexible configuration
- Minimal recompilation requirements

## Performance Considerations

- Non-blocking socket operations
- Efficient memory management
- Modular design for scalability
- Low-overhead data processing

## Potential Future Improvements

- Web-based GUI integration
- Enhanced sensor type discovery
- Machine learning-based data pattern recognition
- Advanced compression algorithms

## Technologies Used

- C programming language
- PostgreSQL database
- libpq for database interactions
- CJson
- Custom memory management
- Socket programming
- Run-Length Encoding compression techniques

## Research Opportunities

- Comparative analysis of data storage strategies
- Performance benchmarking
- Compression algorithm effectiveness
- Sensor data pattern recognition

## Deployment Considerations

- Suitable for maritime sensor systems
- Low-resource hardware environments
- Real-time data processing requirements
- Scalable sensor ecosystem

## Contributors

Magnus Gjerstad, Ingar Asheim, Ole Fredrik Høivang Heggum
