#!/bin/bash
# SensorSentinel Setup Script
# Usage:
#   ./setup.sh            — start stack, seed if fresh, run tests
#   ./setup.sh --seed     — (re)load sample data into running stack
#   ./setup.sh --test     — run test suite only
#   ./setup.sh --reset    — DANGER: wipe data volumes and start fresh

set -e
cd "$(dirname "$0")"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'

info()    { echo -e "${CYAN}▸ $*${NC}"; }
success() { echo -e "${GREEN}✓ $*${NC}"; }
warn()    { echo -e "${YELLOW}⚠ $*${NC}"; }
die()     { echo -e "${RED}✗ $*${NC}"; exit 1; }

# ── Parse args ────────────────────────────────────────────────────────────────
SEED=false
TEST_ONLY=false
RESET=false

for arg in "$@"; do
  case $arg in
    --seed)      SEED=true ;;
    --test)      TEST_ONLY=true ;;
    --reset)     RESET=true ;;
    *) die "Unknown option: $arg" ;;
  esac
done

# ── Reset (dangerous) ─────────────────────────────────────────────────────────
if $RESET; then
  warn "This will delete ALL data (Postgres, NocoDB, Node-RED, Grafana)."
  read -r -p "Type 'yes' to confirm: " CONFIRM
  [ "$CONFIRM" = "yes" ] || die "Aborted"
  info "Stopping containers..."
  docker compose down
  info "Removing data volumes..."
  rm -rf postgres/data nocodb/data grafana/data
  success "Data volumes cleared"
fi

# ── Test only ─────────────────────────────────────────────────────────────────
if $TEST_ONLY; then
  exec "$(dirname "$0")/../tests/run_tests.sh" "$@"
fi

# ── Start stack ───────────────────────────────────────────────────────────────
info "Starting Docker Compose stack..."
docker compose up -d

# ── Wait for Postgres ─────────────────────────────────────────────────────────
info "Waiting for Postgres to be healthy..."
RETRIES=30
until docker exec postgres pg_isready -U sensorsentinel -q 2>/dev/null; do
  RETRIES=$((RETRIES - 1))
  [ $RETRIES -le 0 ] && die "Postgres did not become healthy in time"
  sleep 2
done
success "Postgres is healthy"

# ── Seed sample data ──────────────────────────────────────────────────────────
# Auto-seed on fresh install (no owners), or when --seed is passed
OWNER_COUNT=$(docker exec postgres psql -U sensorsentinel -d sensorsentinel -t -c "SELECT COUNT(*) FROM owners;" 2>/dev/null | xargs || echo "0")

if $SEED || [ "$OWNER_COUNT" = "0" ]; then
  info "Loading sample data..."
  docker exec -i postgres psql -U sensorsentinel -d sensorsentinel \
    < postgres/init/03_sample_data.sql
  success "Sample data loaded"
else
  info "Sample data already present ($OWNER_COUNT owners), skipping (use --seed to reload)"
fi

# ── Wait for NocoDB ───────────────────────────────────────────────────────────
info "Waiting for NocoDB..."
RETRIES=30
until curl -sf "http://localhost:8090/api/v1/health" > /dev/null 2>&1; do
  RETRIES=$((RETRIES - 1))
  [ $RETRIES -le 0 ] && die "NocoDB did not become healthy in time"
  sleep 3
done
success "NocoDB is up"

# ── Wait for Node-RED ─────────────────────────────────────────────────────────
info "Waiting for Node-RED..."
RETRIES=20
until curl -sf "http://localhost:1880/" > /dev/null 2>&1; do
  RETRIES=$((RETRIES - 1))
  [ $RETRIES -le 0 ] && warn "Node-RED not ready after wait, continuing..."
  sleep 3
done
success "Node-RED is up"

# ── Run tests ─────────────────────────────────────────────────────────────────
echo ""
info "Running test suite..."
"$(dirname "$0")/../tests/run_tests.sh"

echo ""
success "Setup complete"
echo ""
echo "  Grafana:  http://localhost:3000  (admin / \$(GRAFANA_PASSWORD))"
echo "  NocoDB:   http://localhost:8090  (admin@sensorsentinel.com / \$(NOCODB_ADMIN_PASSWORD))"
echo "  Node-RED: http://localhost:1880"
echo ""
