#!/bin/bash
# SensorSentinel Regression Test Suite
# Usage: ./tests/run_tests.sh
#        ./tests/run_tests.sh --verbose

set -e
VERBOSE=false
[[ "${1}" == "--verbose" ]] && VERBOSE=true

# ── Helpers ────────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
PASS=0; FAIL=0; SKIP=0

pass() { echo -e "  ${GREEN}✓${NC} $1"; PASS=$((PASS+1)); }
fail() { echo -e "  ${RED}✗${NC} $1"; FAIL=$((FAIL+1)); }
skip() { echo -e "  ${YELLOW}~${NC} $1 (skipped)"; SKIP=$((SKIP+1)); }
section() { echo -e "\n${CYAN}▸ $1${NC}"; }

assert_eq() {
  local desc="$1" expected="$2" actual="$3"
  if [ "$actual" = "$expected" ]; then
    pass "$desc"
  else
    fail "$desc (expected: '$expected', got: '$actual')"
  fi
}

assert_contains() {
  local desc="$1" needle="$2" haystack="$3"
  if echo "$haystack" | grep -q "$needle"; then
    pass "$desc"
  else
    fail "$desc (expected to contain: '$needle')"
    $VERBOSE && echo "    Got: $haystack"
  fi
}

assert_gt() {
  local desc="$1" expected="$2" actual="$3"
  if [ "$actual" -gt "$expected" ] 2>/dev/null; then
    pass "$desc"
  else
    fail "$desc (expected > $expected, got: '$actual')"
  fi
}

pg() {
  docker exec postgres psql -U sensorsentinel -d sensorsentinel -t -c "$1" 2>/dev/null | xargs
}

# ── Section 1: Infrastructure ──────────────────────────────────────────────────
section "Infrastructure"

for container in postgres nocodb mosquitto nodered; do
  STATUS=$(docker inspect --format='{{.State.Status}}' "$container" 2>/dev/null || echo "missing")
  assert_eq "$container is running" "running" "$STATUS"
done

for container in postgres nodered; do
  HEALTH=$(docker inspect --format='{{.State.Health.Status}}' "$container" 2>/dev/null || echo "unknown")
  assert_eq "$container is healthy" "healthy" "$HEALTH"
done

# ── Section 2: Port connectivity ───────────────────────────────────────────────
section "Port connectivity"

for port_desc in "5432:PostgreSQL" "8090:NocoDB" "1883:MQTT" "1880:Node-RED"; do
  port="${port_desc%%:*}"
  name="${port_desc##*:}"
  if nc -z localhost "$port" 2>/dev/null; then
    pass "$name reachable on port $port"
  else
    fail "$name not reachable on port $port"
  fi
done

# ── Section 3: Database schema ─────────────────────────────────────────────────
section "Database schema"

for table in owners devices digital_pins analog_pins alerts events; do
  COUNT=$(pg "SELECT COUNT(*) FROM information_schema.tables WHERE table_name='$table' AND table_schema='public';")
  assert_eq "Table '$table' exists" "1" "$COUNT"
done

for index in idx_events_device_id idx_events_created_at idx_events_node_id idx_devices_node_id; do
  COUNT=$(pg "SELECT COUNT(*) FROM pg_indexes WHERE indexname='$index';")
  assert_eq "Index '$index' exists" "1" "$COUNT"
done

FUNC=$(pg "SELECT COUNT(*) FROM pg_proc WHERE proname='create_device_pins';")
assert_eq "Trigger function 'create_device_pins' exists" "1" "$FUNC"

FUNC=$(pg "SELECT COUNT(*) FROM pg_proc WHERE proname='prune_events';")
assert_eq "Function 'prune_events' exists" "1" "$FUNC"

# ── Section 4: Database behaviour ─────────────────────────────────────────────
section "Database behaviour"

# Insert a test device and check pins are auto-created
pg "INSERT INTO devices (node_id, display_name) VALUES (9999999999, 'Test Device (auto-cleanup)') ON CONFLICT DO NOTHING;" > /dev/null
TEST_DEVICE_ID=$(pg "SELECT id FROM devices WHERE node_id=9999999999;")

if [ -n "$TEST_DEVICE_ID" ]; then
  DIGITAL=$(pg "SELECT COUNT(*) FROM digital_pins WHERE device_id=$TEST_DEVICE_ID;")
  ANALOG=$(pg "SELECT COUNT(*) FROM analog_pins WHERE device_id=$TEST_DEVICE_ID;")
  assert_eq "Device insert auto-creates 8 digital pins" "8" "$DIGITAL"
  assert_eq "Device insert auto-creates 4 analog pins" "4" "$ANALOG"

  # Clean up test device
  pg "DELETE FROM devices WHERE node_id=9999999999;" > /dev/null
  pass "Test device cleaned up"
else
  skip "Could not insert test device (conflict?)"
fi

# ── Section 5: NocoDB ──────────────────────────────────────────────────────────
section "NocoDB"

NOCODB_EMAIL="${NOCODB_ADMIN_EMAIL:-$(grep NOCODB_ADMIN_EMAIL "$(dirname "$0")/../docker/.env" 2>/dev/null | cut -d= -f2)}"
NOCODB_PASS="${NOCODB_ADMIN_PASSWORD:-$(grep NOCODB_ADMIN_PASSWORD "$(dirname "$0")/../docker/.env" 2>/dev/null | cut -d= -f2)}"

if [ -z "$NOCODB_EMAIL" ]; then
  skip "NocoDB credentials not found — set NOCODB_ADMIN_EMAIL / NOCODB_ADMIN_PASSWORD"
else
  assert_contains "NocoDB health endpoint OK" '"message":"OK"' "$(curl -sf http://localhost:8090/api/v1/health 2>/dev/null)"

  TOKEN=$(curl -sf -X POST "http://localhost:8090/api/v1/auth/user/signin" \
    -H "Content-Type: application/json" \
    -d "{\"email\":\"$NOCODB_EMAIL\",\"password\":\"$NOCODB_PASS\"}" 2>/dev/null | \
    python3 -c "import sys,json; print(json.load(sys.stdin).get('token',''))" 2>/dev/null || echo "")

  if [ -z "$TOKEN" ]; then
    fail "NocoDB sign-in failed"
  else
    pass "NocoDB sign-in OK"

    PROJECT=$(curl -sf -H "xc-auth: $TOKEN" "http://localhost:8090/api/v1/db/meta/projects/" 2>/dev/null | \
      python3 -c "import sys,json; projects=json.load(sys.stdin)['list']; print(next((p['id'] for p in projects if p['title']=='SensorSentinel'),''))" 2>/dev/null || echo "")

    if [ -z "$PROJECT" ]; then
      fail "NocoDB 'SensorSentinel' project not found"
    else
      pass "NocoDB 'SensorSentinel' project exists"

      TABLES=$(curl -sf -H "xc-auth: $TOKEN" "http://localhost:8090/api/v1/db/meta/projects/$PROJECT/tables" 2>/dev/null | \
        python3 -c "import sys,json; print(' '.join(t['title'] for t in json.load(sys.stdin)['list']))" 2>/dev/null || echo "")

      for table in Owners Devices DigitalPins AnalogPins Alerts Events; do
        assert_contains "NocoDB table '$table' visible" "$table" "$TABLES"
      done
    fi
  fi
fi

# ── Section 6: MQTT ────────────────────────────────────────────────────────────
section "MQTT"

if docker exec mosquitto mosquitto_pub -h localhost -t "test/ping" -m "pong" 2>/dev/null; then
  pass "MQTT publish to test/ping OK"
else
  fail "MQTT publish failed"
fi

# ── Section 7: Node-RED ────────────────────────────────────────────────────────
section "Node-RED"

NR_STATUS=$(curl -sf -o /dev/null -w "%{http_code}" "http://localhost:1880/" 2>/dev/null || echo "0")
assert_eq "Node-RED UI accessible" "200" "$NR_STATUS"

FLOWS=$(curl -sf "http://localhost:1880/flows" 2>/dev/null | python3 -c "import sys,json; d=json.load(sys.stdin); print(len(d) if isinstance(d,list) else 0)" 2>/dev/null || echo "0")
assert_gt "Node-RED has flows deployed" "0" "$FLOWS"

# ── Summary ────────────────────────────────────────────────────────────────────
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "  ${GREEN}Passed: $PASS${NC}  ${RED}Failed: $FAIL${NC}  ${YELLOW}Skipped: $SKIP${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

[ $FAIL -eq 0 ]
