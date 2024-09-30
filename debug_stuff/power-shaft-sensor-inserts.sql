-- Insert 1
INSERT INTO power_shaft_sensor (timestamp, rpm, torque, temperature, vibration_x, vibration_y, vibration_z, strain, power_output, efficiency, shaft_angle, sensor_id)
VALUES 
(NOW(), 3000, 150.5, 65.2, 0.05, 0.03, 0.04, 0.002, 47.1, 0.92, 45.0, 'SENSOR001');

-- Insert 2
INSERT INTO power_shaft_sensor (timestamp, rpm, torque, temperature, vibration_x, vibration_y, vibration_z, strain, power_output, efficiency, shaft_angle, sensor_id)
VALUES 
(NOW() - INTERVAL '5 minutes', 3050, 155.2, 66.8, 0.06, 0.04, 0.05, 0.0025, 49.3, 0.93, 90.0, 'SENSOR001');

-- Insert 3
INSERT INTO power_shaft_sensor (timestamp, rpm, torque, temperature, vibration_x, vibration_y, vibration_z, strain, power_output, efficiency, shaft_angle, sensor_id)
VALUES 
(NOW() - INTERVAL '10 minutes', 2980, 148.7, 64.5, 0.04, 0.03, 0.03, 0.0018, 46.2, 0.91, 135.0, 'SENSOR001');

-- Insert 4
INSERT INTO power_shaft_sensor (timestamp, rpm, torque, temperature, vibration_x, vibration_y, vibration_z, strain, power_output, efficiency, shaft_angle, sensor_id)
VALUES 
(NOW() - INTERVAL '15 minutes', 3100, 160.0, 68.0, 0.07, 0.05, 0.06, 0.003, 51.7, 0.94, 180.0, 'SENSOR001');

-- Insert 5
INSERT INTO power_shaft_sensor (timestamp, rpm, torque, temperature, vibration_x, vibration_y, vibration_z, strain, power_output, efficiency, shaft_angle, sensor_id)
VALUES 
(NOW() - INTERVAL '20 minutes', 3020, 152.8, 65.9, 0.05, 0.04, 0.04, 0.0022, 48.0, 0.92, 225.0, 'SENSOR001');
