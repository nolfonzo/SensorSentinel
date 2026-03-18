#!/bin/sh
set -e

NOCODB_URL="http://nocodb:8080"
EMAIL="${NOCODB_ADMIN_EMAIL}"
PASSWORD="${NOCODB_ADMIN_PASSWORD}"

# psql helper — runs SQL against nocodb_meta
ncdb() {
  PGPASSWORD="${DB_PASSWORD}" psql -h postgres -U sensorsentinel -d nocodb_meta -t -c "$1" 2>/dev/null | xargs
}

echo "Waiting for NocoDB..."
until curl -sf "${NOCODB_URL}/api/v1/health" > /dev/null 2>&1; do
  sleep 2
done
echo "NocoDB is up"

# Sign in and get token
TOKEN=$(curl -sf -X POST "${NOCODB_URL}/api/v1/auth/user/signin" \
  -H "Content-Type: application/json" \
  -d "{\"email\":\"${EMAIL}\",\"password\":\"${PASSWORD}\"}" | jq -r '.token')

if [ -z "$TOKEN" ] || [ "$TOKEN" = "null" ]; then
  echo "ERROR: Could not sign in to NocoDB"
  exit 1
fi
echo "Signed in OK"

# ── 1. Create project ────────────────────────────────────────────────────────
PROJECT_ID=$(curl -sf -H "xc-auth: ${TOKEN}" "${NOCODB_URL}/api/v1/db/meta/projects/" \
  | jq -r '.list[] | select(.title=="SensorSentinel") | .id' 2>/dev/null | head -1 || true)

if [ -z "$PROJECT_ID" ]; then
  echo "Creating SensorSentinel project..."
  PROJECT_ID=$(curl -sf -X POST "${NOCODB_URL}/api/v1/db/meta/projects/" \
    -H "Content-Type: application/json" \
    -H "xc-auth: ${TOKEN}" \
    -d "{
      \"title\": \"SensorSentinel\",
      \"external\": true,
      \"config\": { \"client\": \"pg\" },
      \"sources\": [{
        \"alias\": \"sensorsentinel\",
        \"type\": \"pg\",
        \"config\": {
          \"client\": \"pg\",
          \"connection\": {
            \"host\": \"postgres\",
            \"port\": 5432,
            \"database\": \"sensorsentinel\",
            \"user\": \"sensorsentinel\",
            \"password\": \"${DB_PASSWORD}\"
          }
        },
        \"enabled\": true,
        \"is_meta\": false,
        \"inflection_table\": \"camelize\",
        \"inflection_column\": \"camelize\"
      }]
    }" | jq -r '.id')

  if [ -z "$PROJECT_ID" ] || [ "$PROJECT_ID" = "null" ]; then
    echo "ERROR: Failed to create project"
    exit 1
  fi
  echo "Project created (id: ${PROJECT_ID})"
else
  echo "Project already exists (id: ${PROJECT_ID})"
fi

# ── 2. Ensure admin is project owner ────────────────────────────────────────
curl -sf -X POST "${NOCODB_URL}/api/v1/db/meta/projects/${PROJECT_ID}/users" \
  -H "Content-Type: application/json" \
  -H "xc-auth: ${TOKEN}" \
  -d "{\"email\":\"${EMAIL}\",\"roles\":\"owner\"}" > /dev/null 2>&1 || true
echo "Admin set as project owner"

# ── 3. Resolve table IDs ─────────────────────────────────────────────────────
TABLES=$(curl -sf -H "xc-auth: ${TOKEN}" "${NOCODB_URL}/api/v1/db/meta/projects/${PROJECT_ID}/tables")
DIGITAL_TABLE_ID=$(echo "$TABLES" | jq -r '.list[] | select(.title=="DigitalPins") | .id')
ANALOG_TABLE_ID=$(echo "$TABLES"  | jq -r '.list[] | select(.title=="AnalogPins")  | .id')
echo "DigitalPins table: ${DIGITAL_TABLE_ID}"
echo "AnalogPins table:  ${ANALOG_TABLE_ID}"

# ── 4. Add PinTitle formula field and set as title ───────────────────────────
add_pin_title() {
  TABLE_ID="$1"
  TABLE_NAME="$2"

  # Skip if formula already exists
  EXISTING=$(ncdb "SELECT id FROM nc_columns_v2 WHERE fk_model_id='${TABLE_ID}' AND title='PinTitle';")
  if [ -n "$EXISTING" ]; then
    echo "${TABLE_NAME}: PinTitle already exists, skipping"
    return
  fi

  # Create formula column
  curl -sf -X POST "${NOCODB_URL}/api/v1/db/meta/tables/${TABLE_ID}/columns" \
    -H "Content-Type: application/json" \
    -H "xc-auth: ${TOKEN}" \
    -d '{"title":"PinTitle","uidt":"Formula","formula_raw":"CONCAT({Devices}, \" - \", {Label})"}' \
    > /dev/null
  echo "${TABLE_NAME}: PinTitle formula column created"

  # Get the new column id
  FORMULA_ID=$(ncdb "SELECT id FROM nc_columns_v2 WHERE fk_model_id='${TABLE_ID}' AND title='PinTitle';")

  # Set as title field (pv) via direct DB update
  PGPASSWORD="${DB_PASSWORD}" psql -h postgres -U sensorsentinel -d nocodb_meta -c "
    UPDATE nc_columns_v2 SET pv = false WHERE fk_model_id = '${TABLE_ID}' AND pv = true;
    UPDATE nc_columns_v2 SET pv = true  WHERE id = '${FORMULA_ID}';
  " > /dev/null 2>&1
  echo "${TABLE_NAME}: PinTitle set as title field"
}

add_pin_title "$DIGITAL_TABLE_ID" "DigitalPins"
add_pin_title "$ANALOG_TABLE_ID"  "AnalogPins"

echo "Done"
