#!/bin/bash
# SensorSentinel - Development / Production Setup Script
set -e

DOCKER_DIR="$(cd "$(dirname "$0")/docker" && pwd)"
ENV_FILE="$DOCKER_DIR/.env"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()    { echo -e "${GREEN}[setup]${NC} $1"; }
warn()    { echo -e "${YELLOW}[setup]${NC} $1"; }
error()   { echo -e "${RED}[setup]${NC} $1"; exit 1; }

# ── Prerequisites ──────────────────────────────────────────────────────────────
check_prereqs() {
  info "Checking prerequisites..."
  command -v docker   >/dev/null 2>&1 || error "Docker not found. Install Docker Desktop from https://www.docker.com"
  docker info         >/dev/null 2>&1 || error "Docker daemon not running. Start Docker Desktop first."
  info "Prerequisites OK"
}

# ── .env setup ─────────────────────────────────────────────────────────────────
setup_env() {
  if [ -f "$ENV_FILE" ]; then
    info ".env already exists, skipping"
    return
  fi

  info "Creating .env file..."
  echo ""
  warn "You'll need to provide a few configuration values."
  echo ""

  read -rp "  Database password [default: changeme]: " DB_PASSWORD
  DB_PASSWORD="${DB_PASSWORD:-changeme}"

  read -rp "  NocoDB admin email: " NOCODB_EMAIL
  read -rsp "  NocoDB admin password: " NOCODB_PASSWORD
  echo ""

  read -rp "  Notification email (Gmail address): " NOTIFY_EMAIL
  read -rsp "  Gmail App Password (see README for how to get one): " NOTIFY_PASS
  echo ""

  cat > "$ENV_FILE" <<EOF
DB_PASSWORD=${DB_PASSWORD}
NOCODB_ADMIN_EMAIL=${NOCODB_EMAIL}
NOCODB_ADMIN_PASSWORD=${NOCODB_PASSWORD}
NOTIFY_EMAIL_USER=${NOTIFY_EMAIL}
NOTIFY_EMAIL_PASS=${NOTIFY_PASS}
EOF
  info ".env created"
}

# ── Docker stack ───────────────────────────────────────────────────────────────
start_stack() {
  info "Starting Docker stack..."
  cd "$DOCKER_DIR"
  docker compose up -d
  info "Waiting for services to be healthy..."
  for service in postgres nodered; do
    for i in $(seq 1 30); do
      STATUS=$(docker inspect --format='{{.State.Health.Status}}' "$service" 2>/dev/null || echo "unknown")
      [ "$STATUS" = "healthy" ] && break
      sleep 2
    done
    STATUS=$(docker inspect --format='{{.State.Health.Status}}' "$service" 2>/dev/null || echo "unknown")
    [ "$STATUS" = "healthy" ] || warn "$service health check did not pass (status: $STATUS)"
  done
  info "Stack is up"
}

# ── Wait for NocoDB init ───────────────────────────────────────────────────────
wait_for_nocodb_init() {
  info "Waiting for NocoDB initialisation..."
  for i in $(seq 1 30); do
    STATUS=$(docker inspect --format='{{.State.Status}}' nocodb-init 2>/dev/null || echo "unknown")
    [ "$STATUS" = "exited" ] && break
    sleep 3
  done
  EXIT_CODE=$(docker inspect --format='{{.State.ExitCode}}' nocodb-init 2>/dev/null || echo "1")
  if [ "$EXIT_CODE" = "0" ]; then
    info "NocoDB initialised OK"
  else
    warn "NocoDB init exited with code $EXIT_CODE — check: docker logs nocodb-init"
  fi
}

# ── Sample data ────────────────────────────────────────────────────────────────
seed_sample_data() {
  if [ "${1}" != "--seed" ]; then
    return
  fi

  info "Seeding sample data..."
  docker exec -i postgres psql -U sensorsentinel -d sensorsentinel \
    < "$DOCKER_DIR/../docker/postgres/init/03_sample_data.sql" \
    && info "Sample data loaded" \
    || warn "Sample data seeding failed (may already exist)"
}

# ── Summary ────────────────────────────────────────────────────────────────────
print_summary() {
  echo ""
  echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
  echo -e "${GREEN}  SensorSentinel is running${NC}"
  echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
  echo ""
  echo "  NocoDB (device/owner management):  http://localhost:8090"
  echo "  Node-RED (flow editor):             http://localhost:1880"
  echo "  MQTT broker:                        localhost:1883"
  echo "  PostgreSQL:                         localhost:5432"
  echo ""
  echo "  Run tests:  ./tests/run_tests.sh"
  echo ""
}

# ── Main ───────────────────────────────────────────────────────────────────────
check_prereqs
setup_env
start_stack
wait_for_nocodb_init
seed_sample_data "$@"
print_summary
