#!/bin/sh
set -e

NOCODB_URL="http://nocodb:8080"
EMAIL="${NOCODB_ADMIN_EMAIL}"
PASSWORD="${NOCODB_ADMIN_PASSWORD}"

echo "Waiting for NocoDB..."
until curl -sf "${NOCODB_URL}/api/v1/health" > /dev/null 2>&1; do
  sleep 2
done
echo "NocoDB is up"

# Try signup (silently fails if account already exists)
curl -sf -X POST "${NOCODB_URL}/api/v1/auth/user/signup" \
  -H "Content-Type: application/json" \
  -d "{\"email\":\"${EMAIL}\",\"password\":\"${PASSWORD}\"}" > /dev/null 2>&1 || true

# Sign in and get token
TOKEN=$(curl -sf -X POST "${NOCODB_URL}/api/v1/auth/user/signin" \
  -H "Content-Type: application/json" \
  -d "{\"email\":\"${EMAIL}\",\"password\":\"${PASSWORD}\"}" | jq -r '.token')

if [ -z "$TOKEN" ] || [ "$TOKEN" = "null" ]; then
  echo "ERROR: Could not sign in to NocoDB"
  exit 1
fi
echo "Signed in OK"

# Check if project already exists
EXISTING=$(curl -sf -H "xc-auth: ${TOKEN}" "${NOCODB_URL}/api/v1/db/meta/projects/" \
  | jq -r '.list[] | select(.title=="SensorSentinel") | .id' 2>/dev/null || true)

if [ -n "$EXISTING" ]; then
  echo "Project 'SensorSentinel' already exists, skipping"
  exit 0
fi

# Step 1: Create the postgres integration
echo "Creating Postgres integration..."
INTEGRATION_ID=$(curl -sf -X POST "${NOCODB_URL}/api/v2/meta/integrations" \
  -H "Content-Type: application/json" \
  -H "xc-auth: ${TOKEN}" \
  -d "{
    \"title\": \"SensorSentinel DB\",
    \"type\": \"database\",
    \"sub_type\": \"pg\",
    \"config\": {
      \"client\": \"pg\",
      \"connection\": {
        \"host\": \"postgres\",
        \"port\": 5432,
        \"database\": \"sensorsentinel\",
        \"user\": \"sensorsentinel\",
        \"password\": \"${DB_PASSWORD}\"
      }
    }
  }" | jq -r '.id')

if [ -z "$INTEGRATION_ID" ] || [ "$INTEGRATION_ID" = "null" ]; then
  echo "ERROR: Failed to create integration"
  exit 1
fi
echo "Integration created: $INTEGRATION_ID"

# Step 2: Create the project
echo "Creating SensorSentinel project..."
PROJECT_ID=$(curl -sf -X POST "${NOCODB_URL}/api/v1/db/meta/projects/" \
  -H "Content-Type: application/json" \
  -H "xc-auth: ${TOKEN}" \
  -d "{\"title\": \"SensorSentinel\"}" | jq -r '.id')

if [ -z "$PROJECT_ID" ] || [ "$PROJECT_ID" = "null" ]; then
  echo "ERROR: Failed to create project"
  exit 1
fi
echo "Project created: $PROJECT_ID"

# Step 3: Link the integration as a source
echo "Linking Postgres source to project..."
curl -sf -X POST "${NOCODB_URL}/api/v1/db/meta/projects/${PROJECT_ID}/bases" \
  -H "Content-Type: application/json" \
  -H "xc-auth: ${TOKEN}" \
  -d "{
    \"type\": \"pg\",
    \"fk_integration_id\": \"${INTEGRATION_ID}\",
    \"inflection_table\": \"camelize\",
    \"inflection_column\": \"camelize\"
  }" > /dev/null

echo "Project 'SensorSentinel' created and connected to Postgres successfully"
