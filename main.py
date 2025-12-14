import os
import time
import requests
from flask import Flask, jsonify

TFL_APP_ID = os.getenv("TFL_APP_ID")
TFL_APP_KEY = os.getenv("TFL_APP_KEY")
STOP_ID = os.getenv("TFL_STOP_ID")

URL = f"https://api.tfl.gov.uk/StopPoint/{STOP_ID}/Arrivals"

app = Flask(__name__)

CACHE_SECONDS = 20
_CACHE = {"timestamp": 0, "data": []}


def get_next_buses(limit=3):
    now = time.time()
    if now - _CACHE["timestamp"] < CACHE_SECONDS:
        return _CACHE["data"]

    params = {"app_id": TFL_APP_ID, "app_key": TFL_APP_KEY}

    try:
        r = requests.get(URL, params=params, timeout=5)
        r.raise_for_status()
        arrivals = r.json()
    except requests.RequestException as e:
        print(f"TfL API error: {e}")
        return _CACHE["data"]

    arrivals.sort(key=lambda x: x["timeToStation"])
    seen_routes = set()
    buses = []

    for a in arrivals:
        route = a["lineName"]
        if route in seen_routes:
            continue
        seen_routes.add(route)
        buses.append({
            "route": route,
            "destination": a["destinationName"],
            "due_in_minutes": max(a["timeToStation"] // 60, 0),
        })
        if len(buses) == limit:
            break

    _CACHE["timestamp"] = now
    _CACHE["data"] = buses
    return buses


@app.route("/buses")
def buses():
    return jsonify(get_next_buses())


@app.route("/health")
def health():
    return {"status": "ok"}


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000)
