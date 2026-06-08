#!/usr/bin/env python3
import datetime
import json
import pathlib
import re
import sys
import xml.etree.ElementTree as ET
import zipfile


NS = {
    "a": "http://schemas.openxmlformats.org/spreadsheetml/2006/main",
    "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
}


def slug(text):
    return re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")


def read_xlsx_rows(path):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        shared = []
        if "xl/sharedStrings.xml" in names:
            root = ET.fromstring(archive.read("xl/sharedStrings.xml"))
            for item in root.findall("a:si", NS):
                text = "".join(
                    (node.text or "")
                    for node in item.iter(
                        "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}t"
                    )
                )
                shared.append(text)

        root = ET.fromstring(archive.read("xl/worksheets/sheet1.xml"))
        rows = []
        for row in root.findall(".//a:sheetData/a:row", NS):
            cells = []
            max_col = 0
            for cell in row.findall("a:c", NS):
                ref = cell.attrib.get("r", "")
                match = re.match(r"([A-Z]+)", ref)
                col = 0
                if match:
                    for char in match.group(1):
                        col = (col * 26) + ord(char) - 64
                value_node = cell.find("a:v", NS)
                value = ""
                if value_node is not None:
                    value = value_node.text or ""
                    if cell.attrib.get("t") == "s" and value.isdigit():
                        value = shared[int(value)]
                cells.append((col, value))
                max_col = max(max_col, col)
            values = [""] * max_col
            for col, value in cells:
                if col > 0:
                    values[col - 1] = value
            rows.append(values)
        return rows


def build_kb(source):
    rows = read_xlsx_rows(source)
    headers = [str(header).strip() for header in rows[0]]
    entries = []
    for row in rows[1:]:
        item = {}
        for index, header in enumerate(headers):
            value = row[index] if index < len(row) else ""
            item[header] = value.strip() if isinstance(value, str) else value
        name = item.get("name", "")
        keywords = [
            keyword.strip().lower()
            for keyword in str(item.get("keywords", "")).split(",")
            if keyword.strip()
        ]
        entries.append(
            {
                "id": "service-" + slug(name),
                "title": name,
                "department": "service",
                "category": item.get("category", ""),
                "summary": item.get("summary", ""),
                "availability": item.get("available", ""),
                "conditions": item.get("conditions", ""),
                "limits": item.get("limits", ""),
                "notes": item.get("notes", ""),
                "keywords": keywords,
                "capture_policy": (
                    "collect caller name, callback phone, vehicle, request, "
                    "and preferred callback time; service team confirms details"
                ),
            }
        )

    return {
        "version": "pilot-memory-kb-v1",
        "source_files": [str(source)],
        "generated_at": datetime.datetime.now(datetime.timezone.utc)
        .replace(microsecond=0)
        .isoformat()
        .replace("+00:00", "Z"),
        "runtime_policy": {
            "load_mode": "in_memory_at_startup_or_compile_time",
            "live_call_network_lookup": False,
            "latency_goal": "no KB network request during live turn",
        },
        "dealership": {"name": "the dealership", "timezone": "America/Toronto"},
        "service_entries": entries,
        "vehicle_lexicon": {
            "years": [str(year) for year in range(2000, 2027)],
            "makes": ["Hyundai"],
            "models": [
                "Ioniq 5",
                "Tucson",
                "Santa Fe",
                "Elantra",
                "Palisade",
                "Kona",
                "Sonata",
                "Venue",
                "Accent",
                "Veloster",
                "Genesis",
            ],
            "aliases": {
                "ionic 5": "Ioniq 5",
                "ioniq five": "Ioniq 5",
                "f one fifty": "F-150",
                "f dash one fifty": "F-150",
                "f won fifty": "F-150",
                "f one 50": "F-150",
                "f one five zero": "F-150",
                "f one five oh": "F-150",
                "f 150": "F-150",
                "f150": "F-150",
                "santa fe hybrid": "Santa Fe hybrid",
                "palisades": "Palisade",
            },
            "reject_generic": [
                "car",
                "my car",
                "the car",
                "vehicle",
                "my vehicle",
                "old car",
                "new car",
                "truck",
                "suv",
                "ev",
                "hyundai",
            ],
        },
        "confirmation_phrases": {
            "yes": [
                "yes",
                "correct",
                "yup",
                "yep",
                "yeah",
                "right",
                "that is right",
                "that's right",
                "all correct",
                "that is mine",
                "that's mine",
                "yep that is mine",
                "yup that is mine",
                "looks right",
                "sounds right",
                "correct number",
            ]
        },
        "response_sanitizer": {
            "remove": [
                "for our records",
                "so I can assist you better",
                "so I can assist you further",
                "Mr.",
                "Mr ",
                "Ms.",
                "Ms ",
                "Mrs.",
                "Mrs ",
            ],
            "replace": {
                "service appointment": "service request",
                "Service appointment": "Service request",
            },
        },
        "name_capture": {
            "name_precursors": [
                "my name is",
                "name is",
                "this is",
                "it's",
                "it is",
                "i'm",
                "i am",
                "speaking",
            ],
            "non_name_precursors": [
                "this is for",
                "this is about",
                "this is regarding",
                "it's for",
                "it is for",
                "i am looking",
                "i'm looking",
            ],
        },
        "spelling_capture": {
            "unsolicited_rule": (
                "all letter groups must be single letters; at least two "
                "total letters; no digits"
            )
        },
    }


def main():
    source = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(
        "/Users/admin/Downloads/VOXTEN/Service KB.xlsx"
    )
    output = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else pathlib.Path(
        "kb/pilot_kb.json"
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(build_kb(source), indent=2, ensure_ascii=False) + "\n")
    print(f"wrote {output}")


if __name__ == "__main__":
    main()
