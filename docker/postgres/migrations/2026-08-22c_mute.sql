-- Muting, as a timestamp rather than a flag.
--
-- Kept separate from notify_via on purpose: overwriting the channel with 'none'
-- would lose what it was, and unmute would have nothing to restore. This way a
-- mute is a temporary state laid over a preference that survives it.
--
-- A timestamp rather than a boolean for the same reason watches expire - the
-- thing being silenced is a bilge alarm, and "I'll turn it back on later" is
-- exactly the intention that gets forgotten. An indefinite mute is still
-- possible; it just has to be asked for.

ALTER TABLE owners ADD COLUMN IF NOT EXISTS muted_until TIMESTAMP;

COMMENT ON COLUMN owners.muted_until IS
  'While in the future, all notifications for this owner are suppressed on every channel. NULL means not muted.';
