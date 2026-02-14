-- SensorSentinel Database Schema

CREATE TABLE owners (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255),
    phone VARCHAR(50),
    notify_via VARCHAR(20) DEFAULT 'email',
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE devices (
    id SERIAL PRIMARY KEY,
    node_id BIGINT NOT NULL UNIQUE,
    display_name VARCHAR(100) NOT NULL,
    owner_id INTEGER REFERENCES owners(id) ON DELETE SET NULL,
    last_seen TIMESTAMP,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE digital_pins (
    id SERIAL PRIMARY KEY,
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    pin_index SMALLINT NOT NULL CHECK (pin_index BETWEEN 0 AND 7),
    label VARCHAR(100) DEFAULT '',
    trigger VARCHAR(10) DEFAULT 'None' CHECK (trigger IN ('None', 'High', 'Low', 'Change')),
    alert_level VARCHAR(10) DEFAULT 'None' CHECK (alert_level IN ('None', 'Low', 'Medium', 'High')),
    UNIQUE(device_id, pin_index)
);

CREATE TABLE analog_pins (
    id SERIAL PRIMARY KEY,
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    pin_index SMALLINT NOT NULL CHECK (pin_index BETWEEN 0 AND 3),
    label VARCHAR(100) DEFAULT '',
    low_threshold NUMERIC,
    high_threshold NUMERIC,
    alert_level VARCHAR(10) DEFAULT 'None' CHECK (alert_level IN ('None', 'Low', 'Medium', 'High')),
    UNIQUE(device_id, pin_index)
);

CREATE TABLE alerts (
    id SERIAL PRIMARY KEY,
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    pin_label VARCHAR(100) NOT NULL,
    alert_message TEXT,
    alert_level VARCHAR(10) DEFAULT 'Medium',
    count INTEGER DEFAULT 1,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW(),
    UNIQUE(device_id, pin_label)
);

CREATE TABLE events (
    id SERIAL PRIMARY KEY,
    device_id INTEGER REFERENCES devices(id) ON DELETE SET NULL,
    node_id BIGINT NOT NULL,
    message_type VARCHAR(10) NOT NULL CHECK (message_type IN ('sensor', 'gnss')),
    payload JSONB NOT NULL,
    created_at TIMESTAMP DEFAULT NOW()
);

-- Index for fast event lookups
CREATE INDEX idx_events_device_id ON events(device_id);
CREATE INDEX idx_events_created_at ON events(created_at);
CREATE INDEX idx_events_node_id ON events(node_id);
CREATE INDEX idx_devices_node_id ON devices(node_id);

-- Auto-create 8 digital + 4 analog pins when a device is added
CREATE OR REPLACE FUNCTION create_device_pins()
RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO digital_pins (device_id, pin_index)
    SELECT NEW.id, i FROM generate_series(0, 7) AS i;

    INSERT INTO analog_pins (device_id, pin_index)
    SELECT NEW.id, i FROM generate_series(0, 3) AS i;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_create_device_pins
    AFTER INSERT ON devices
    FOR EACH ROW EXECUTE FUNCTION create_device_pins();

-- Event retention: function to prune old events (call from cron/Node-RED)
CREATE OR REPLACE FUNCTION prune_events(days_to_keep INTEGER DEFAULT 30)
RETURNS INTEGER AS $$
DECLARE
    deleted INTEGER;
BEGIN
    DELETE FROM events WHERE created_at < NOW() - (days_to_keep || ' days')::INTERVAL;
    GET DIAGNOSTICS deleted = ROW_COUNT;
    RETURN deleted;
END;
$$ LANGUAGE plpgsql;
