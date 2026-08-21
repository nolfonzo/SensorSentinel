-- Telegram as a notification channel, and per-device verbose mode.
--
-- notify_via has been in the schema since the beginning but nothing ever read
-- it - send-notification emailed unconditionally. Email was only ever the
-- channel we happened to build first, not a decision. This makes the column
-- mean something.
--
-- Apply to an existing database with:
--   docker exec -i postgres psql -U sensorsentinel -d sensorsentinel \
--     < docker/postgres/migrations/2026-08-22_telegram_and_verbose.sql
-- Fresh databases get all of this from 01_schema.sql instead.

BEGIN;

-- ── owners: who to reach, and how ───────────────────────────────────────────

-- The Telegram chat this owner talks to the bot in. Null until they pair.
-- Telegram chat IDs are 64-bit and can be negative (groups), so BIGINT.
ALTER TABLE owners ADD COLUMN IF NOT EXISTS telegram_chat_id BIGINT;

-- A single-use token binding a Telegram chat to this owner. It is delivered
-- as a deep link in the footer of their alert email -
-- t.me/<bot>?start=<token> - which taps straight through to a paired chat
-- with nothing typed. The mailbox is the proof of identity: it is the address
-- already on file for this owner, so whoever can read it is that owner.
--
-- Stable until redeemed, so any old alert email still works as an invitation,
-- and cleared on use. The same token also works typed (/pair <token>) for mail
-- clients that mangle links.
ALTER TABLE owners ADD COLUMN IF NOT EXISTS pair_code VARCHAR(32);

-- An admin may act on any device, not just their own. Everyone else is scoped
-- to what they own - which is also what makes it safe to let the bot answer
-- questions about history later.
ALTER TABLE owners ADD COLUMN IF NOT EXISTS is_admin BOOLEAN DEFAULT FALSE;

-- Now that more than one value is meaningful, constrain it. 'none' is for
-- someone who wants to pull status on demand but never be pushed at.
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_constraint WHERE conname = 'owners_notify_via_check') THEN
        ALTER TABLE owners ADD CONSTRAINT owners_notify_via_check
            CHECK (notify_via IN ('email','telegram','both','none'));
    END IF;
END $$;

-- Two owners must never share a chat ID, or a reply could be attributed to the
-- wrong person. Partial, because many owners legitimately have no chat yet.
CREATE UNIQUE INDEX IF NOT EXISTS idx_owners_telegram_chat_id
    ON owners (telegram_chat_id) WHERE telegram_chat_id IS NOT NULL;

-- Set when the one-time welcome email goes out. Polled rather than driven by a
-- trigger, so an owner created any way at all - NocoDB, psql, a future signup
-- form - gets welcomed without that path having to know to do it.
ALTER TABLE owners ADD COLUMN IF NOT EXISTS welcome_sent_at TIMESTAMP;

-- ── devices: verbose mode ───────────────────────────────────────────────────

-- A timestamp, deliberately not a boolean. A node reporting every 30s produces
-- 120 digests an hour; verbose mode has to be able to switch itself off when
-- whoever armed it has walked out of signal and forgotten about it.
ALTER TABLE devices ADD COLUMN IF NOT EXISTS verbose_until TIMESTAMP;

-- Digests go to whoever armed it, not to the device owner - the person
-- watching is the person who asked. That lets an admin diagnose someone
-- else's boat without spamming them.
ALTER TABLE devices ADD COLUMN IF NOT EXISTS verbose_chat_id BIGINT;

COMMIT;
