import argparse
import csv
import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import date
from pathlib import Path

OVERPASS_SERVERS = [
    "https://overpass-api.de/api/interpreter",
    "https://overpass.private.coffee/api/interpreter",
]

KOTSIUBYNSKE_RELATION_ID = 1754752
FIRST_ID = 1000

QUERY_TEMPLATE = """
[out:json][timeout:120];
area(id:{area_id})->.place;
(
  nwr["shop"~"^(supermarket|convenience)$"](area.place);
  nwr["amenity"~"^(pharmacy|post_office|cafe|restaurant|fast_food|fuel)$"](area.place);
);
out tags center;
"""

COLUMNS = ["id", "type", "name", "lat", "lng", "shelter_capacity", "underground", "generator",
           "address", "source", "food_reserves", "max_parcel_kg", "open_24h", "wifi", "opening_hours"]

DEFAULT_NAMES = {
    "supermarket": "Супермаркет",
    "grocery": "Продуктовий магазин",
    "pharmacy": "Аптека",
    "post_office": "Поштове відділення",
    "cafe": "Заклад харчування",
    "gas_station": "АЗС",
}


def classify(tags):
    shop = tags.get("shop")
    amenity = tags.get("amenity")
    if shop == "supermarket":
        return "supermarket"
    if shop == "convenience":
        return "grocery"
    if amenity == "pharmacy":
        return "pharmacy"
    if amenity == "post_office":
        return "post_office"
    if amenity in ("cafe", "restaurant", "fast_food"):
        return "cafe"
    if amenity == "fuel":
        return "gas_station"
    return None


def build_address(tags):
    street = tags.get("addr:street", "")
    house = tags.get("addr:housenumber", "")
    return f"{street}, {house}".strip(", ")


def fetch_overpass(query):
    body = urllib.parse.urlencode({"data": query}).encode("utf-8")
    last_error = None
    for server in OVERPASS_SERVERS:
        for attempt in range(2):
            try:
                request = urllib.request.Request(server, data=body,
                                                 headers={"User-Agent": "SafeMap-student-project"})
                with urllib.request.urlopen(request, timeout=180) as response:
                    return json.load(response)
            except (urllib.error.URLError, TimeoutError) as error:
                last_error = error
                print(f"  {server}: {error}, повтор...", file=sys.stderr)
                time.sleep(5)
    raise RuntimeError(f"Overpass недоступний: {last_error}")


def to_rows(elements):
    rows = []
    for element in sorted(elements, key=lambda e: (e["type"], e["id"])):
        tags = element.get("tags", {})
        type_key = classify(tags)
        if type_key is None:
            continue
        center = element.get("center") or {"lat": element.get("lat"), "lon": element.get("lon")}
        address = build_address(tags)
        name = tags.get("name") or tags.get("brand") or DEFAULT_NAMES[type_key]
        is_24h = tags.get("opening_hours", "").strip() == "24/7"
        rows.append({
            "id": FIRST_ID + len(rows),
            "type": type_key,
            "name": name,
            "lat": f"{center['lat']:.6f}",
            "lng": f"{center['lon']:.6f}",
            "shelter_capacity": 0,
            "underground": 0,
            "generator": 0,
            "address": address,
            "source": f"OSM {element['type']}/{element['id']}",
            "food_reserves": "",
            "max_parcel_kg": "",
            "open_24h": 1 if is_24h else "",
            "wifi": "",
            "opening_hours": tags.get("opening_hours", "").strip(),
        })
    return rows


def main():
    parser = argparse.ArgumentParser(description="Завантажує заклади з OpenStreetMap у CSV для SafeMap")
    parser.add_argument("--relation", type=int, default=KOTSIUBYNSKE_RELATION_ID,
                        help="OSM id адміністративної межі (за замовчуванням Коцюбинське)")
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent.parent / "data" / "osm_places.csv")
    args = parser.parse_args()

    area_id = 3600000000 + args.relation
    print(f"Запит до Overpass для relation {args.relation}...")
    data = fetch_overpass(QUERY_TEMPLATE.format(area_id=area_id))
    rows = to_rows(data.get("elements", []))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=COLUMNS)
        writer.writeheader()
        writer.writerows(rows)

    counts = {}
    for row in rows:
        counts[row["type"]] = counts.get(row["type"], 0) + 1
    print(f"{date.today().isoformat()}: записано {len(rows)} закладів у {args.output}: {counts}")
    print("Дані (c) OpenStreetMap contributors, ODbL")


if __name__ == "__main__":
    main()
