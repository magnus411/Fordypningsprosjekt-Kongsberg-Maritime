# Meta Power Quad - Sensor Data Handler System

## Project Overview

This project aims to develop a prototype exploring technical solutions for challenges in Kongsberg Maritime's Meta Power Quad datahandler. The main focus is demonstrating a flexible architecture that allows adding new sensors without code modifications or recompilation, while also implementing efficient fail safe mechanisms like data dumping, and null-data compression. The prototype will serve as a basis for decision-making in Kongsberg Maritime's future production system development and is not intended for production use.

## System Documentation
Full system documentation generated with Doxygen is available at: https://magnus411.github.io/Fordypningsprosjekt/


## Installation & Running the System

### Prerequisites
- GCC compiler
- PostgreSQL development libraries (`libpq-dev`)
- Make
- Docker and Docker Compose (optional)



### Option 1: Using Docker Compose
1. Install  Docker and Docker Compose

https://docs.docker.com/engine/install/ <br>
https://docs.docker.com/compose/install/


2. Build and run the application:

```bash
#Building the docker
docker-compose build

#Running the docker-compose
docker-compose up
```

This will:

    Start a PostgreSQL database container
    Build and run the SensorDHS application
    Set up the necessary network connections between containers
    Mount the configuration files from your local configs directory

To stop the application:

```bash
docker compose down
```

#### To connect to the database:
```bash

docker-compose exec db psql -U postgres fordypningsprosjekt
```


### Option 2: Building with Make

1. Install required dependencies:
```bash
sudo apt-get update
sudo apt-get install gcc make libpq-dev
```

2. Build the project:
```bash

# Default build (debug)
make

```
3. Run the application:
```bash

./build/SensorDHS

```

**Note:** This requires you to have a postgres database already running. This can be done with for example docker:

```bash
docker run -d --name postgres-sensordb -p 5432:5432 -e POSTGRES_PASSWORD=passord -e POSTGRES_DB=fordypningsprosjekt postgres:14
```
You then need to configure the SensorDHS `postgres-conf` that is located inside the `configs`folder to use that postgres database.

**Additional make commands**:

    make clean: Remove build files
    make docs: Generate documentation (requires Doxygen)
    make format: Format code using clang-format
    make lint: Run clang-tidy checks
    make data_generator: Build the test data generator




### Configuration
The project comes with default configurations, but these can be changed. 

1. `postgres-conf`: Database connection settings
  `host=db
  dbname=fordypningsprosjekt
  user=postgres
  password=passord`

**Note:** When using Docker, `host=db` refers to the Docker container hostname. For local builds, use `host=127.0.0.1` or your specific PostgreSQL host.



2. `modbus-conf`: Modbus connection settings

ipv4Addr=127.0.0.1 <br>
port=1312

3. `sdb_conf.json`: System configuration
```json
{
  "data_handlers": [
    {
      "name": "modbus_with_postgres",
      "enabled": true,
      "modbus": {
        "mem": "8mB",
        "scratch_size": "128kB"
      },
      "postgres": {
        "mem": "8mB",
        "scratch_size": "128kB"
      },
      "pipe": {
        "buf_count": 2,
        "buf_size": "32kB"
      },
      "testing": {
        "enabled": true
      }
    }
  ]
}
```

`sensor_schemas.json`: Sensor configuration
```
{
  "sensors": [
    {
      "name": "shaft_power",
      "data": {
        "packet_id": "BIGINT",
        "time": "TIMESTAMP",
        "rpm": "DOUBLE PRECISION",
        "torque": "DOUBLE PRECISION",
        "power": "DOUBLE PRECISION",
        "peak_peak_pfs": "DOUBLE PRECISION"
      }
    }
  ]
}
```





## Contributors

Magnus Gjerstad, Ingar Asheim, Ole Fredrik Høivang Heggum
