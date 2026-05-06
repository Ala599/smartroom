import paho.mqtt.client as mqtt
import json
import random
import time
import math


BROKER = "localhost"
PORT   = 1883

# Etat interne des noeuds (simule la physique de la salle)
nodes_state = {
    "smartroom-node-1": {"co2_base": 450, "presence_streak": 0, "ventilateur": False},
    "smartroom-node-2": {"co2_base": 420, "presence_streak": 0, "ventilateur": False},
    "smartroom-node-3": {"co2_base": 400, "presence_streak": 0, "ventilateur": False},
}

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("[OK] Connecte au broker Mosquitto")
        # Souscrire aux commandes pour simuler la reaction des actionneurs
        client.subscribe("smartroom/cmds/#")
    else:
        print(f"[ERR] Connexion echouee, code={rc}")

def on_message(client, userdata, msg):
    """Receptionne les commandes Node-RED et met a jour l'etat simule"""
    try:
        node_id = msg.topic.split("/")[2]  # smartroom/cmds/NODE_ID
        cmd = json.loads(msg.payload.decode())
        act = cmd.get("act", "")
        val = cmd.get("val", "OFF")

        if act == "relay2":  # relay2 = ventilateur
            nodes_state[node_id]["ventilateur"] = (val == "ON")
            status = "ON" if val == "ON" else "OFF"
            print(f"  [CMD] {node_id} -> Ventilateur {status}")
        elif act == "relay1":
            print(f"  [CMD] {node_id} -> Eclairage {'ON' if val=='ON' else 'OFF'}")
        elif act == "buzzer":
            print(f"  [CMD] {node_id} -> ALERTE BUZZER {'ON' if val=='ON' else 'OFF'}")
    except Exception as e:
        pass

def generate_payload(node_id, ts, tick):
    state = nodes_state[node_id]

    # --- Presence : cycle realiste (present 70% du temps par vagues) ---
    presence = 1 if (math.sin(tick * 0.08) > -0.3) else 0

    if presence:
        state["presence_streak"] += 1
    else:
        state["presence_streak"] = max(0, state["presence_streak"] - 3)

    # --- CO2 : monte avec la presence, baisse avec le ventilateur ---
    if presence and not state["ventilateur"]:
        state["co2_base"] = min(1600, state["co2_base"] + random.uniform(8, 18))
    elif state["ventilateur"]:
        state["co2_base"] = max(400, state["co2_base"] - random.uniform(20, 40))
    else:
        state["co2_base"] = max(400, state["co2_base"] - random.uniform(2, 5))

    co2 = int(state["co2_base"] + random.uniform(-15, 15))

    # --- Temperature : monte legerement avec presence ---
    temp_base = 22.0 + (state["presence_streak"] * 0.05)
    temp = round(temp_base + random.uniform(-0.3, 0.3), 1)
    temp = max(18.0, min(30.0, temp))

    # --- Humidite ---
    hum = round(random.uniform(45.0, 62.0), 1)

    # --- Luminosite : basse la nuit (simulation), haute le jour ---
    hour_sim = (tick // 120) % 24  # 1h simulee = 120 ticks de 30s
    if 7 <= hour_sim <= 19:
        lux = random.randint(250, 700) if presence else random.randint(50, 150)
    else:
        lux = random.randint(0, 80)

    return {
        "id":   node_id,
        "ts":   ts,
        "temp": temp,
        "hum":  hum,
        "co2":  co2,
        "lux":  lux,
        "pres": presence,
        "rssi": random.randint(-75, -45)
    }

def main():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    print(f"[...] Connexion a {BROKER}:{PORT}")
    try:
        client.connect(BROKER, PORT, 60)
    except Exception as e:
        print(f"[ERR] Impossible de se connecter : {e}")
        print("      Verifiez que Docker Desktop est lance et que la stack tourne.")
        return

    client.loop_start()
    time.sleep(1)

    ts   = 0
    tick = 0

    print("\n[Smart Room Simulator] Demarrage - Ctrl+C pour arreter\n")
    print(f"{'Noeud':<22} {'Temp':>6} {'Hum':>6} {'CO2':>6} {'Lux':>6} {'Pres':>5}")
    print("-" * 60)

    try:
        while True:
            for node_id in nodes_state:
                payload = generate_payload(node_id, ts, tick)
                topic   = f"smartroom/{node_id}/data"
                client.publish(topic, json.dumps(payload), qos=1)

                co2_warn = " !! ALERTE CO2" if payload["co2"] > 1000 else ""
                print(
                    f"{node_id:<22} "
                    f"{payload['temp']:>5.1f}C "
                    f"{payload['hum']:>5.1f}% "
                    f"{payload['co2']:>5}ppm "
                    f"{payload['lux']:>5}lux "
                    f"{'OUI' if payload['pres'] else 'NON':>5}"
                    f"{co2_warn}"
                )

            ts   += 30
            tick += 1
            print()
            time.sleep(5)  # 5s pour la demo (au lieu de 30s en prod)

    except KeyboardInterrupt:
        print("\n[STOP] Simulation arretee.")
        client.loop_stop()
        client.disconnect()

if __name__ == "__main__":
    main()