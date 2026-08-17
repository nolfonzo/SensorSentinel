#!/usr/bin/env python3
import paho.mqtt.client as mqtt
import os, json, base64, struct, time, smtplib, imaplib
from email.mime.text import MIMEText

MQTT_BROKER = '192.168.9.77'
MQTT_PORT = 1883
GMAIL_USER = os.environ.get('NOTIFY_EMAIL_USER', 'nolfonzo@gmail.com')
# Never hardcode this. The app password that used to sit here was committed
# to a public repo, detected by GitGuardian, and had to be revoked
# (2026-08-17). Set NOTIFY_EMAIL_PASS in the environment instead.
GMAIL_PASS = os.environ.get('NOTIFY_EMAIL_PASS')
RECIPIENT = 'nolfonzo@gmail.com'

# Device & Threshold Profiles (Mirrors NocoDB / PostgreSQL Configuration)
DEVICES = {
    2697689064: {
        'name': 'Boat SensorSentinel',
        'bus_pins': {
            0: {'label': 'Boat Battery', 'low': 12100, 'high': 14500, 'level': 'High'}
        }
    }
}

device_states = {}

def send_email(subject, body, recipient=RECIPIENT):
    msg = MIMEText(body)
    msg['Subject'] = subject
    msg['From'] = f'"SensorSentinel" <{GMAIL_USER}>'
    msg['To'] = recipient
    try:
        with smtplib.SMTP_SSL('smtp.gmail.com', 465) as server:
            server.login(GMAIL_USER, GMAIL_PASS)
            server.send_message(msg)
            print(f"  📧 [EMAIL DELIVERED] Subject: {subject} -> {recipient}")
            return True
    except Exception as e:
        print(f"  ❌ [EMAIL ERROR] {e}")
        return False

def on_connect(client, userdata, flags, rc, properties=None):
    print(f"✅ Connected to Mosquitto at {MQTT_BROKER}:{MQTT_PORT}")
    client.subscribe('GWID/#')

def on_message(client, userdata, msg):
    if not msg.topic.startswith('GWID/'):
        return

    try:
        payload = json.loads(msg.payload.decode('utf-8'))
        raw = base64.b64decode(payload.get('data', ''))
        if len(raw) != 48:
            return

        proto_ver, msg_type, node_flags = raw[0], raw[1], raw[2]
        node_id, counter, uptime = struct.unpack('<III', raw[4:16])
        bat_pct, bat_mv = raw[16], struct.unpack('<H', raw[17:19])[0]
        digital_mask = struct.unpack('<H', raw[20:22])[0]
        analog_gpio = struct.unpack('<4H', raw[22:30])
        bus_slots = struct.unpack('<8H', raw[30:46])
        discovered = raw[46]

        v_float = bus_slots[0] / 1000.0
        print(f"\n[GATEWAY RX] Proto: v{proto_ver} | NodeID: 0xA0CB77E8 ({node_id}) | Msg #{counter} | RF: {payload.get('freq')} MHz | RSSI: {payload.get('rssi')} dBm")
        print(f"         ⚡ Bus Slot 0 (SEN0291): {v_float:.2f}V ({bus_slots[0]} mV)")

        dev_config = DEVICES.get(node_id, {'name': 'Boat SensorSentinel', 'bus_pins': {0: {'label': 'Boat Battery', 'low': 12100, 'high': 14500}}})
        dev_name = dev_config['name']

        # Evaluate Bus Slot 0 (Boat Battery Voltage)
        slot0_cfg = dev_config['bus_pins'].get(0)
        if slot0_cfg:
            val = bus_slots[0]
            low_th = slot0_cfg.get('low')
            high_th = slot0_cfg.get('high')
            label = slot0_cfg.get('label', 'Boat Battery')

            state_key = f"bus_{node_id}_0"
            prev_state = device_states.get(state_key, 'NORMAL')

            new_state = 'NORMAL'
            if low_th is not None and val < low_th:
                new_state = 'LOW'
            elif high_th is not None and val > high_th:
                new_state = 'HIGH'

            print(f"  [STATE ENGINE] {label}: {v_float:.2f}V -> State: {new_state} (Prev: {prev_state})")

            if new_state != prev_state:
                device_states[state_key] = new_state
                v_str = f"{v_float:.2f}V"
                low_str = f"{low_th/1000.0:.2f}V" if low_th is not None else ""
                high_str = f"{high_th/1000.0:.2f}V" if high_th is not None else ""

                if new_state == 'LOW':
                    subj = f"[ALERT] {label} Low Voltage Warning {v_str} (<{low_str})"
                    body = (
                        f"SensorSentinel Alert Notification\n\n"
                        f"Sensor: {label}\n"
                        f"Status: LOW VOLTAGE WARNING: {v_str} (Threshold: < {low_str})\n"
                        f"Device: {dev_name} (Node ID: {node_id})\n"
                        f"Timestamp: {time.ctime()}\n"
                    )
                    send_email(subj, body)
                elif new_state == 'HIGH':
                    subj = f"[ALERT] {label} High Voltage Warning {v_str} (>{high_str})"
                    body = (
                        f"SensorSentinel Alert Notification\n\n"
                        f"Sensor: {label}\n"
                        f"Status: HIGH VOLTAGE WARNING: {v_str} (Threshold: > {high_str})\n"
                        f"Device: {dev_name} (Node ID: {node_id})\n"
                        f"Timestamp: {time.ctime()}\n"
                    )
                    send_email(subj, body)
                elif new_state == 'NORMAL':
                    subj = f"[RESOLVED] {label} Restored: {v_str}"
                    body = (
                        f"SensorSentinel Alert Notification\n\n"
                        f"Sensor: {label}\n"
                        f"Status: RESOLVED: Voltage restored to normal operating range ({v_str})\n"
                        f"Device: {dev_name} (Node ID: {node_id})\n"
                        f"Timestamp: {time.ctime()}\n"
                    )
                    send_email(subj, body)
    except Exception as e:
        print(f"Processing error: {e}")

if __name__ == '__main__':
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    print("🚀 SensorSentinel Alert Engine started with Dynamic Subjects...")

    start = time.time()
    while time.time() - start < 55:
        client.loop(timeout=1.0)

    # Validate in Gmail via IMAP
    print("\n--- PROGRAMMATIC GMAIL IMAP CHECK ---")
    try:
        mail = imaplib.IMAP4_SSL('imap.gmail.com')
        mail.login(GMAIL_USER, GMAIL_PASS)
        mail.select('"[Gmail]/Sent Mail"')
        _, data = mail.search(None, 'ALL')
        latest_id = data[0].split()[-1]
        _, msg_data = mail.fetch(latest_id, '(RFC822.HEADER)')
        import email
        hdr = email.message_from_bytes(msg_data[0][1])
        print(f"✅ Confirmed in Gmail: Subject: \"{hdr.get('Subject')}\" | Date: {hdr.get('Date')}")
        mail.logout()
    except Exception as e:
        print('IMAP Check Error:', e)
