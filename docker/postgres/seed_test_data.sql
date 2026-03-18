-- SensorSentinel Test Data Seed
-- Idempotent: wipes prior test data (Carl / Stu) before reinserting.
-- Run: docker exec -i postgres psql -U sensorsentinel -d sensorsentinel < docker/postgres/seed_test_data.sql

BEGIN;

-- ── Remove prior test data ───────────────────────────────────────────────────
-- Delete devices by node_id first (pins/alerts/events cascade from device).
-- owner_id is ON DELETE SET NULL so deleting owners alone leaves orphan devices.
DELETE FROM devices WHERE node_id IN (2697662092, 1000000002);
DELETE FROM owners  WHERE email IN ('carl@example.com', 'stu@example.com');

-- ── Owners ──────────────────────────────────────────────────────────────────
INSERT INTO owners (name, email, notify_via) VALUES
  ('Carl',       'carl@example.com', 'email'),
  ('Dangar Stu', 'stu@example.com',  'email');

-- ── Devices ─────────────────────────────────────────────────────────────────
-- Trigger auto-creates 8 digital + 4 analog pins for each device on insert.
-- Using synthetic node_ids unlikely to collide with real hardware.
INSERT INTO devices (node_id, display_name, owner_id)
  SELECT 2697662092, 'Rita',  id FROM owners WHERE email = 'carl@example.com';

INSERT INTO devices (node_id, display_name, owner_id)
  SELECT 1000000002, 'Renko', id FROM owners WHERE email = 'stu@example.com';

-- ── Carl's Rita: analog pin 0 = Battery Level ───────────────────────────────
UPDATE analog_pins
   SET label       = 'Battery Level',
       low_threshold = 1100,
       alert_level = 'High'
 WHERE device_id = (SELECT id FROM devices WHERE node_id = 2697662092)
   AND pin_index = 0;

-- ── Carl's Rita: digital pin 0 = Bilge (alert when Low) ─────────────────────
UPDATE digital_pins
   SET label       = 'Bilge',
       trigger     = 'Low',
       alert_level = 'High'
 WHERE device_id = (SELECT id FROM devices WHERE node_id = 2697662092)
   AND pin_index = 0;

-- ── Stu's Renko: digital pin 0 = Forward Bilge (alert when High) ────────────
UPDATE digital_pins
   SET label       = 'Forward Bilge',
       trigger     = 'High',
       alert_level = 'High'
 WHERE device_id = (SELECT id FROM devices WHERE node_id = 1000000002)
   AND pin_index = 0;

COMMIT;

-- ── Summary ──────────────────────────────────────────────────────────────────
SELECT o.name AS owner, d.display_name AS device, d.node_id
  FROM owners o JOIN devices d ON d.owner_id = o.id
 WHERE o.email IN ('carl@example.com', 'stu@example.com')
 ORDER BY o.name, d.display_name;
