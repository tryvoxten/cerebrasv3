#!/usr/bin/env python3
import json
import pathlib
import sys


def c_escape(text):
    return (
        str(text)
        .replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
    )


def quoted(text):
    return '"' + c_escape(text) + '"'


def build_faq_prompt(kb):
    faq = kb["faq_runtime"]
    entries = runtime_faq_entries(kb)
    labels = ", ".join(entry["id"] for entry in entries)
    meanings = []
    for entry in entries:
        meanings.append(f'{entry["id"]}={entry["description"]}')
    priorities = "; ".join(faq["priority_rules"])
    return (
        "f must be exactly one of: "
        + labels
        + ". FAQ meanings: "
        + "; ".join(meanings)
        + ". Priority: "
        + priorities
        + "."
    )


def service_entry_answer(entry):
    title = entry.get("title", "")
    summary = entry.get("summary", "")
    availability = entry.get("availability", "")
    conditions = entry.get("conditions", "")
    limits = entry.get("limits", "")
    notes = entry.get("notes", "")
    parts = []
    if availability == "yes":
        parts.append(f"{title} is available.")
    elif availability == "conditional":
        parts.append(f"{title} may be available.")
    if summary:
        parts.append(summary + ".")
    if conditions:
        parts.append("Conditions: " + conditions + ".")
    if limits:
        parts.append("Limits: " + limits + ".")
    if notes:
        parts.append("Notes: " + notes + ".")
    if availability == "conditional":
        parts.append("The service team can confirm details when they call back.")
    return " ".join(parts)


def service_entry_to_faq(entry):
    title = entry.get("title", "")
    keywords = [str(keyword).lower() for keyword in entry.get("keywords", []) if keyword]
    aliases = sorted(set(keywords + [title.lower()]))
    description_parts = [title]
    summary = entry.get("summary", "")
    if summary:
        description_parts.append(summary)
    if keywords:
        description_parts.append(", ".join(keywords))
    return {
        "id": entry["id"],
        "description": "; ".join(description_parts),
        "answer": service_entry_answer(entry),
        "aliases": aliases,
    }


def runtime_faq_entries(kb):
    entries = list(kb["faq_runtime"]["entries"])
    existing_ids = set(entry["id"] for entry in entries)
    for entry in kb.get("service_entries", []):
        if entry["id"] not in existing_ids:
            entries.append(service_entry_to_faq(entry))
            existing_ids.add(entry["id"])
    return entries


def build_affirmation_prompt(kb):
    affirmation = kb["affirmation_runtime"]
    labels = ", ".join(affirmation["labels"])
    return (
        "a must be exactly one of: "
        + labels
        + ". "
        + affirmation["prompt_rule"]
    )


def write_header(path):
    path.write_text(
        """#ifndef cerebras_v3_generated_kb_h
#define cerebras_v3_generated_kb_h

namespace cerebras_v3
{
namespace generated_kb
{

struct Faq_entry
{
  const char* id;
  const char* description;
  const char* answer;
};

struct Faq_alias
{
  const char* phrase;
  const char* faq_id;
};

extern const Faq_entry faq_entries[];
extern const int faq_entry_count;
extern const Faq_alias faq_aliases[];
extern const int faq_alias_count;
extern const char* affirmation_yes_phrases[];
extern const int affirmation_yes_count;
extern const char* affirmation_no_phrases[];
extern const int affirmation_no_count;
extern const char* affirmation_unclear_phrases[];
extern const int affirmation_unclear_count;
extern const char* vehicle_models[];
extern const int vehicle_model_count;
extern const char* vehicle_aliases[];
extern const int vehicle_alias_count;
extern const char* interpreter_faq_rules;
extern const char* interpreter_affirmation_rules;

}
}

#endif
""",
        encoding="utf-8",
    )


def write_cpp(path, kb):
    faq_entries = runtime_faq_entries(kb)
    aliases = []
    for entry in faq_entries:
        for alias in entry["aliases"]:
            aliases.append((alias, entry["id"]))

    affirmation = kb["affirmation_runtime"]
    vehicles = kb["vehicle_lexicon"]["models"]
    vehicle_aliases = sorted(kb["vehicle_lexicon"].get("aliases", {}).keys())
    faq_prompt = build_faq_prompt(kb)
    affirmation_prompt = build_affirmation_prompt(kb)

    lines = [
        '#include "generated_kb.h"',
        "",
        "namespace cerebras_v3",
        "{",
        "namespace generated_kb",
        "{",
        "",
        "const Faq_entry faq_entries[] =",
        "{",
    ]
    for entry in faq_entries:
        lines.append(
            "  { "
            + quoted(entry["id"])
            + ", "
            + quoted(entry["description"])
            + ", "
            + quoted(entry["answer"])
            + " },"
        )
    lines.extend(
        [
            "};",
            "const int faq_entry_count = sizeof(faq_entries) / sizeof(faq_entries[0]);",
            "",
            "const Faq_alias faq_aliases[] =",
            "{",
        ]
    )
    for phrase, faq_id in aliases:
        lines.append("  { " + quoted(phrase) + ", " + quoted(faq_id) + " },")
    lines.extend(
        [
            "};",
            "const int faq_alias_count = sizeof(faq_aliases) / sizeof(faq_aliases[0]);",
            "",
            "const char* affirmation_yes_phrases[] =",
            "{",
        ]
    )
    for phrase in affirmation["yes"]:
        lines.append("  " + quoted(phrase) + ",")
    lines.extend(
        [
            "};",
            "const int affirmation_yes_count = sizeof(affirmation_yes_phrases) / sizeof(affirmation_yes_phrases[0]);",
            "",
            "const char* affirmation_no_phrases[] =",
            "{",
        ]
    )
    for phrase in affirmation["no"]:
        lines.append("  " + quoted(phrase) + ",")
    lines.extend(
        [
            "};",
            "const int affirmation_no_count = sizeof(affirmation_no_phrases) / sizeof(affirmation_no_phrases[0]);",
            "",
            "const char* affirmation_unclear_phrases[] =",
            "{",
        ]
    )
    for phrase in affirmation["unclear"]:
        lines.append("  " + quoted(phrase) + ",")
    lines.extend(
        [
            "};",
            "const int affirmation_unclear_count = sizeof(affirmation_unclear_phrases) / sizeof(affirmation_unclear_phrases[0]);",
            "",
            "const char* vehicle_models[] =",
            "{",
        ]
    )
    for model in vehicles:
        lines.append("  " + quoted(model) + ",")
    lines.extend(
        [
            "};",
            "const int vehicle_model_count = sizeof(vehicle_models) / sizeof(vehicle_models[0]);",
            "",
            "const char* vehicle_aliases[] =",
            "{",
        ]
    )
    for alias in vehicle_aliases:
        lines.append("  " + quoted(alias) + ",")
    lines.extend(
        [
            "};",
            "const int vehicle_alias_count = sizeof(vehicle_aliases) / sizeof(vehicle_aliases[0]);",
            "",
            "const char* interpreter_faq_rules =",
            "  " + quoted(faq_prompt) + ";",
            "",
            "const char* interpreter_affirmation_rules =",
            "  " + quoted(affirmation_prompt) + ";",
            "",
            "}",
            "}",
            "",
        ]
    )
    path.write_text("\n".join(lines), encoding="utf-8")


def main():
    kb_path = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path("kb/pilot_kb.json")
    header_path = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else pathlib.Path("src/generated_kb.h")
    cpp_path = pathlib.Path(sys.argv[3]) if len(sys.argv) > 3 else pathlib.Path("src/generated_kb.cpp")
    kb = json.loads(kb_path.read_text(encoding="utf-8"))
    write_header(header_path)
    write_cpp(cpp_path, kb)


if __name__ == "__main__":
    main()
