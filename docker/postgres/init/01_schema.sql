-- SensorSentinel Database Schema

CREATE TABLE owners (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255),
    phone VARCHAR(50),
    -- How this owner wants to be reached. Email is the default because it
    -- needs nothing of them - no app, no account, no pairing. Telegram is
    -- opt-in, and for those who take it, it can carry the alerts too.
    notify_via VARCHAR(20) DEFAULT 'email'
        CHECK (notify_via IN ('email','telegram','both','none')),
    -- The Telegram chat this owner talks to the bot in. Null until they pair.
    -- Chat IDs are 64-bit and negative for groups, so BIGINT.
    telegram_chat_id BIGINT,
    -- A single-use token binding a Telegram chat to this owner, delivered as a
    -- deep link in their alert email footer (t.me/<bot>?start=<token>): one tap,
    -- nothing typed. The mailbox is the proof of identity - it is the address
    -- already on file, so whoever reads it is that owner. Stable until redeemed
    -- so old emails keep working; also accepted typed, as /pair <token>.
    pair_code VARCHAR(32),
    -- An admin may act on any device. Everyone else is scoped to their own,
    -- which is what makes it safe to let the bot answer questions later.
    is_admin BOOLEAN DEFAULT FALSE,
    -- While in the future, every notification for this owner is suppressed on
    -- every channel. Separate from notify_via so unmuting restores what they
    -- actually chose, and a timestamp so a mute can expire by itself.
    muted_until TIMESTAMP,
    -- Set when the one-time welcome email goes out. Polled rather than trigger
    -- driven, so an owner created any way at all gets welcomed.
    welcome_sent_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Two owners sharing a chat ID would mean attributing a message to the wrong
-- person. Partial, because most owners have no chat yet.
CREATE UNIQUE INDEX idx_owners_telegram_chat_id
    ON owners (telegram_chat_id) WHERE telegram_chat_id IS NOT NULL;

-- Correcting an owner's address re-introduces the system to whoever now
-- actually owns the boat. A trigger, so every edit path is covered.
CREATE OR REPLACE FUNCTION owners_rewelcome_on_email_change() RETURNS trigger AS $$
BEGIN
    -- IS DISTINCT FROM, not <>, so NULL -> address counts as a change.
    IF NEW.email IS DISTINCT FROM OLD.email THEN
        NEW.welcome_sent_at := NULL;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_owners_rewelcome ON owners;
CREATE TRIGGER trg_owners_rewelcome
    BEFORE UPDATE OF email ON owners
    FOR EACH ROW EXECUTE FUNCTION owners_rewelcome_on_email_change();


CREATE TABLE devices (
    id SERIAL PRIMARY KEY,
    node_id BIGINT NOT NULL UNIQUE,
    display_name VARCHAR(100) NOT NULL,
    owner_id INTEGER REFERENCES owners(id) ON DELETE SET NULL,
    last_seen TIMESTAMP,
    -- Watchdog: a node that goes quiet looks identical to a node reporting
    -- "all fine", so silence has to raise an alarm of its own. Per-device
    -- because nodes transmit at different intervals.
    stale_after_minutes INTEGER DEFAULT 30,
    watchdog_enabled BOOLEAN DEFAULT TRUE,
    -- Verbose mode: a digest of every message received from this device.
    -- A timestamp, deliberately not a boolean - a node reporting every 30s
    -- produces 120 digests an hour, so this has to switch itself off when
    -- whoever armed it has walked out of signal and forgotten about it.
    verbose_until TIMESTAMP,
    -- Digests go to whoever armed it, not to the device owner: the person
    -- watching is the person who asked. Lets an admin diagnose someone else's
    -- boat without spamming them.
    verbose_chat_id BIGINT,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE digital_pins (
    id SERIAL PRIMARY KEY,
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    pin_index SMALLINT NOT NULL CHECK (pin_index BETWEEN 0 AND 15),
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

CREATE TABLE bus_pins (
    id SERIAL PRIMARY KEY,
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    pin_index SMALLINT NOT NULL CHECK (pin_index BETWEEN 0 AND 7),
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

-- Auto-create 16 digital + 4 analog + 8 bus pins when a device is added
CREATE OR REPLACE FUNCTION create_device_pins()
RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO digital_pins (device_id, pin_index)
    SELECT NEW.id, i FROM generate_series(0, 15) AS i
    ON CONFLICT DO NOTHING;

    INSERT INTO analog_pins (device_id, pin_index)
    SELECT NEW.id, i FROM generate_series(0, 3) AS i
    ON CONFLICT DO NOTHING;

    INSERT INTO bus_pins (device_id, pin_index)
    SELECT NEW.id, i FROM generate_series(0, 7) AS i
    ON CONFLICT DO NOTHING;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_create_device_pins
    AFTER INSERT ON devices
    FOR EACH ROW EXECUTE FUNCTION create_device_pins();

-- Keep devices.last_seen honest. It is a field that looks authoritative during
-- diagnosis, so it must not drift: driving it from the events insert means it
-- stays correct regardless of what changes in the Node-RED flow.
CREATE OR REPLACE FUNCTION update_device_last_seen()
RETURNS TRIGGER AS $$
BEGIN
    UPDATE devices
       SET last_seen = NEW.created_at,
           updated_at = NOW()
     WHERE id = NEW.device_id;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_update_last_seen
    AFTER INSERT ON events
    FOR EACH ROW
    WHEN (NEW.device_id IS NOT NULL)
    EXECUTE FUNCTION update_device_last_seen();

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
