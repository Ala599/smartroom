# Smart Room IoT — Démarrage rapide Windows

## Prérequis
- Docker Desktop installé et lancé (https://www.docker.com/products/docker-desktop/)
- Python 3.10+ installé (https://www.python.org/)
- pip install paho-mqtt

## Étape 1 — Lancer la stack
Ouvrir PowerShell dans le dossier smartroom/ :
    docker compose up -d
Vérifier :
    docker ps
→ 4 conteneurs : mosquitto, influxdb, nodered, grafana

## Étape 2 — Configurer InfluxDB
Ouvrir http://localhost:8086
Login : admin / smartroom123
→ Load Data → API Tokens → Generate All Access Token
→ Copier le token (nécessaire pour Node-RED)

## Étape 3 — Configurer Node-RED
Ouvrir http://localhost:1880
→ Menu (≡) → Import → Coller le contenu de flows_nodered.json
→ Dans le nœud "influxdb out", coller votre token API
→ Deploy

## Étape 4 — Lancer le simulateur
    python simulate_nodes.py

## Étape 5 — Vérifier Grafana
Ouvrir http://localhost:3000
Login : admin / smartroom123
→ Connections → Data Sources → Add → InfluxDB
    URL      : http://influxdb:8086
    Language : Flux
    Org      : smart-room
    Token    : (votre token)
    Bucket   : sensors
→ Créer un dashboard avec les requêtes du fichier grafana_queries.txt

## URLs de service
| Service    | URL                      |
|------------|--------------------------|
| Mosquitto  | localhost:1883 (MQTT)    |
| InfluxDB   | http://localhost:8086    |
| Node-RED   | http://localhost:1880    |
| Grafana    | http://localhost:3000    |