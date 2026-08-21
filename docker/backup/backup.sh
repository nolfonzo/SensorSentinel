#!/bin/sh
#
# Nightly backup of everything that would be painful to recreate.
#
# Three things, not one:
#   sensorsentinel  the telemetry and device/owner/threshold config
#   nocodb_meta     every NocoDB view, dashboard and field setting. Backing up
#                   only the data would restore the numbers and lose the entire
#                   interface built on top of them.
#   nodered         the flows - the alerting logic itself, which lives on disk
#                   rather than in postgres.
#
# pg_dump, never a copy of the data directory: postgres is running, and a
# filesystem copy of a live database restores as corruption.
#
# The copy is pushed OFF this Pi immediately. A backup that only exists on the
# machine it is protecting is not a backup - if the SD card dies, so does it.

set -u

PGHOST="${PGHOST:-postgres}"
PGUSER="${PGUSER:-sensorsentinel}"
REMOTE="${REMOTE:-nolfonzo@100.107.31.26}"
REMOTE_DIR="${REMOTE_DIR:-/opt/backups/sensorsentinel}"
LOCAL_DIR="${LOCAL_DIR:-/backups}"
KEEP_LOCAL="${KEEP_LOCAL:-7}"
AT_HOUR="${AT_HOUR:-02}"

log() { echo "[backup] $(date '+%Y-%m-%d %H:%M:%S') $*"; }

run_backup() {
  STAMP=$(date +%Y%m%d-%H%M%S)
  WORK="$LOCAL_DIR/$STAMP"
  mkdir -p "$WORK"
  FAILED=0

  for db in sensorsentinel nocodb_meta; do
    log "dumping $db"
    if pg_dump -h "$PGHOST" -U "$PGUSER" -d "$db" 2>/dev/null | gzip > "$WORK/$db.sql.gz"; then
      # A dump that failed halfway still leaves a file, and gzip still writes a
      # valid header - so check it is a plausible size rather than trusting
      # that it exists.
      SZ=$(stat -c %s "$WORK/$db.sql.gz" 2>/dev/null || echo 0)
      if [ "$SZ" -lt 1000 ]; then
        log "ERROR: $db dump is only ${SZ} bytes - treating as failed"
        FAILED=1
      else
        log "  $db: $SZ bytes"
      fi
    else
      log "ERROR: pg_dump failed for $db"
      FAILED=1
    fi
  done

  # Node-RED flows. Excludes the node_modules tree, which is large, reinstallable
  # and not yours.
  if [ -d /nodered-data ]; then
    log "archiving node-red flows"
    tar czf "$WORK/nodered.tar.gz" -C /nodered-data \
        --exclude=node_modules --exclude='*.backup' . 2>/dev/null \
      && log "  nodered: $(stat -c %s "$WORK/nodered.tar.gz") bytes" \
      || { log "ERROR: node-red archive failed"; FAILED=1; }
  fi

  # The .env is gitignored - correctly, it holds the database password, the
  # notification mail credentials, and the NocoDB/Grafana admin logins. That
  # means it exists nowhere else. Restore the databases without it and nothing
  # can send an alert. It is a few hundred bytes; back it up.
  if [ -f /stack/.env ]; then
    cp /stack/.env "$WORK/env.backup" 2>/dev/null \
      && log "  .env: $(stat -c %s "$WORK/env.backup") bytes" \
      || { log "ERROR: could not copy .env"; FAILED=1; }
  else
    log "WARNING: no .env found to back up"
  fi

  if [ "$FAILED" = "1" ]; then
    log "one or more dumps failed - NOT pushing a partial backup"
    return 1
  fi

  log "pushing to $REMOTE:$REMOTE_DIR"
  if ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o ConnectTimeout=20 \
         "$REMOTE" "mkdir -p '$REMOTE_DIR'" 2>/dev/null \
     && scp -q -o BatchMode=yes -o StrictHostKeyChecking=no \
            -r "$WORK" "$REMOTE:$REMOTE_DIR/" 2>/dev/null; then
    log "pushed $STAMP"
  else
    log "ERROR: push failed - the copy on this Pi is the only one"
    return 1
  fi

  # Local copies are a convenience for a quick restore; the off-box copy is the
  # one that matters, and restic handles retention there.
  COUNT=$(ls -1d "$LOCAL_DIR"/20* 2>/dev/null | wc -l)
  if [ "$COUNT" -gt "$KEEP_LOCAL" ]; then
    ls -1d "$LOCAL_DIR"/20* | sort | head -n $((COUNT - KEEP_LOCAL)) | while read -r old; do
      log "pruning local $old"
      rm -rf "$old"
    done
  fi
  return 0
}

log "started - will run daily at ${AT_HOUR}:00"
[ "${RUN_NOW:-0}" = "1" ] && run_backup

LAST=""
while true; do
  NOW=$(date +%H)
  TODAY=$(date +%Y-%m-%d)
  if [ "$NOW" = "$AT_HOUR" ] && [ "$LAST" != "$TODAY" ]; then
    LAST="$TODAY"
    run_backup
  fi
  sleep 300
done
