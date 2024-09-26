CREATE TABLE power_shaft_sensor (
    id BIGSERIAL PRIMARY KEY,
    timestamp TIMESTAMPTZ NOT NULL,
    rpm INTEGER NOT NULL,
    torque DOUBLE PRECISION NOT NULL,
    temperature DOUBLE PRECISION NOT NULL,
    vibration_x DOUBLE PRECISION NOT NULL,
    vibration_y DOUBLE PRECISION NOT NULL,
    vibration_z DOUBLE PRECISION NOT NULL,
    strain DOUBLE PRECISION NOT NULL,
    power_output DOUBLE PRECISION NOT NULL,
    efficiency DOUBLE PRECISION NOT NULL,
    shaft_angle DOUBLE PRECISION NOT NULL,
    sensor_id VARCHAR(50) NOT NULL
);

-- Create an index on the timestamp column for faster time-based queries
CREATE INDEX idx_power_shaft_sensor_timestamp ON power_shaft_sensor (timestamp);

-- Add a comment to the table for documentation
COMMENT ON TABLE power_shaft_sensor IS 'Stores sensor data for the power shaft connecting the engine to the propeller';

-- Add comments to the columns for clarity
COMMENT ON COLUMN power_shaft_sensor.id IS 'Unique identifier for each sensor reading';
COMMENT ON COLUMN power_shaft_sensor.timestamp IS 'Time when the sensor reading was taken';
COMMENT ON COLUMN power_shaft_sensor.rpm IS 'Rotations per minute of the power shaft';
COMMENT ON COLUMN power_shaft_sensor.torque IS 'Torque applied to the shaft in Newton-meters';
COMMENT ON COLUMN power_shaft_sensor.temperature IS 'Temperature of the shaft in degrees Celsius';
COMMENT ON COLUMN power_shaft_sensor.vibration_x IS 'Vibration along the X-axis in m/s^2';
COMMENT ON COLUMN power_shaft_sensor.vibration_y IS 'Vibration along the Y-axis in m/s^2';
COMMENT ON COLUMN power_shaft_sensor.vibration_z IS 'Vibration along the Z-axis in m/s^2';
COMMENT ON COLUMN power_shaft_sensor.strain IS 'Strain on the shaft (unitless)';
COMMENT ON COLUMN power_shaft_sensor.power_output IS 'Power output in kilowatts';
COMMENT ON COLUMN power_shaft_sensor.efficiency IS 'Efficiency of power transfer as a percentage';
COMMENT ON COLUMN power_shaft_sensor.shaft_angle IS 'Angle of the shaft in degrees';
COMMENT ON COLUMN power_shaft_sensor.sensor_id IS 'Unique identifier for the specific sensor';
