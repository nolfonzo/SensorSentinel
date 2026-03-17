-- SensorSentinel Sample Data
-- Run via: ./setup.sh --seed
-- Or manually: docker exec -i postgres psql -U sensorsentinel -d sensorsentinel < docker/postgres/init/03_sample_data.sql

-- Guard: skip if sample data already loaded
DO $$ BEGIN
  IF EXISTS (SELECT 1 FROM owners WHERE email = 'alice@example.com') THEN
    RAISE NOTICE 'Sample data already loaded, skipping';
    RETURN;
  END IF;
END $$;

-- ── Owners ────────────────────────────────────────────────────────────────────
INSERT INTO owners (name, email, phone, notify_via) VALUES
  ('Alice Harbour',  'alice@example.com',  '+61400000001', 'email'),
  ('Bob Seawright',  'bob@example.com',    '+61400000002', 'email'),
  ('Carol Mariner',  'carol@example.com',  '+61400000003', 'email');

-- ── Devices ───────────────────────────────────────────────────────────────────
-- node_id values are realistic MAC-derived IDs (fake)
INSERT INTO devices (node_id, display_name, owner_id) VALUES
  (2712847316, 'Bow Sensor - Seabreeze',    1),  -- Alice's boat
  (3054261894, 'Engine Room - Seabreeze',   1),  -- Alice's boat
  (1876543210, 'Bilge Monitor - Waverunner', 2), -- Bob's boat
  (4123456789, 'Cockpit Sensor - Odyssey',  3);  -- Carol's boat
-- Trigger auto-creates 8 digital + 4 analog pins for each device

-- ── Analog pin labels & thresholds ───────────────────────────────────────────
-- Device 1: Bow Sensor
UPDATE analog_pins SET label = 'Bilge Level',    high_threshold = 2500, alert_level = 'High'   WHERE device_id = 1 AND pin_index = 0;
UPDATE analog_pins SET label = 'Battery Voltage', low_threshold = 1100,  alert_level = 'Medium' WHERE device_id = 1 AND pin_index = 1;
UPDATE analog_pins SET label = 'Fuel Level',      low_threshold = 500,   alert_level = 'Medium' WHERE device_id = 1 AND pin_index = 2;

-- Device 2: Engine Room
UPDATE analog_pins SET label = 'Engine Temp',    high_threshold = 3500, alert_level = 'High'   WHERE device_id = 2 AND pin_index = 0;
UPDATE analog_pins SET label = 'Oil Pressure',   low_threshold = 800,   alert_level = 'High'   WHERE device_id = 2 AND pin_index = 1;
UPDATE analog_pins SET label = 'Bilge Level',    high_threshold = 2000, alert_level = 'High'   WHERE device_id = 2 AND pin_index = 2;

-- Device 3: Bilge Monitor
UPDATE analog_pins SET label = 'Bilge Level',    high_threshold = 3000, alert_level = 'High'   WHERE device_id = 3 AND pin_index = 0;
UPDATE analog_pins SET label = 'Battery',         low_threshold = 1200,  alert_level = 'Low'    WHERE device_id = 3 AND pin_index = 1;

-- Device 4: Cockpit Sensor
UPDATE analog_pins SET label = 'Bilge Level',    high_threshold = 2800, alert_level = 'High'   WHERE device_id = 4 AND pin_index = 0;
UPDATE analog_pins SET label = 'Battery Voltage', low_threshold = 1000,  alert_level = 'Medium' WHERE device_id = 4 AND pin_index = 1;

-- ── Digital pin labels & triggers ────────────────────────────────────────────
-- Device 1: Bow Sensor
UPDATE digital_pins SET label = 'Hatch Open',    trigger = 'High', alert_level = 'Medium' WHERE device_id = 1 AND pin_index = 0;
UPDATE digital_pins SET label = 'Shore Power',   trigger = 'Low',  alert_level = 'Low'    WHERE device_id = 1 AND pin_index = 1;
UPDATE digital_pins SET label = 'Bilge Pump',    trigger = 'High', alert_level = 'Low'    WHERE device_id = 1 AND pin_index = 2;

-- Device 2: Engine Room
UPDATE digital_pins SET label = 'Engine Running', trigger = 'Change', alert_level = 'None'   WHERE device_id = 2 AND pin_index = 0;
UPDATE digital_pins SET label = 'Alarm Trigger',  trigger = 'High',   alert_level = 'High'   WHERE device_id = 2 AND pin_index = 1;

-- Device 3: Bilge Monitor
UPDATE digital_pins SET label = 'Float Switch',  trigger = 'High', alert_level = 'High'   WHERE device_id = 3 AND pin_index = 0;

-- Device 4: Cockpit
UPDATE digital_pins SET label = 'Companionway',  trigger = 'High', alert_level = 'Medium' WHERE device_id = 4 AND pin_index = 0;
UPDATE digital_pins SET label = 'Shore Power',   trigger = 'Low',  alert_level = 'Low'    WHERE device_id = 4 AND pin_index = 1;

SELECT 'Sample data loaded: ' || count(*) || ' owners' AS status FROM owners;
SELECT count(*) || ' devices' AS status FROM devices;
