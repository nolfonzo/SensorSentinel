-- Re-welcome an owner whose email address changes.
--
-- A trigger rather than logic in the bot, because the address can be edited
-- from NocoDB, from psql, or from a signup form that does not exist yet, and
-- none of those paths should have to know to do this. Same reasoning as
-- polling for unwelcomed owners rather than hooking one creation route.
--
-- The case this exists for: a device is handed over, the owner row still
-- carries the previous address, and correcting it should introduce the system
-- to whoever now actually owns the boat.

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
