#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";

const callerRoles = new Set(["caller", "user", "customer", "human"]);
const assistantRoles = new Set(["agent", "assistant", "ai", "voxten"]);

function normalizeRole(role) {
  const text = String(role || "").trim().toLowerCase();
  if (callerRoles.has(text)) return "caller";
  if (assistantRoles.has(text)) return "assistant";
  return text || "unknown";
}

function getText(value) {
  if (typeof value === "string") return value;
  if (!value || typeof value !== "object") return "";
  return String(
    value.text ||
      value.content ||
      value.message ||
      value.transcript ||
      value.words ||
      "",
  );
}

function normalizeTurn(turn, index) {
  const role = normalizeRole(turn.role || turn.speaker || turn.source || turn.from);
  return {
    index,
    role,
    text: getText(turn).trim(),
    endCall: Boolean(
      turn.end_call ||
        turn.endCall ||
        turn.metadata?.end_call ||
        turn.response?.end_call,
    ),
  };
}

function turnsFromJson(parsed) {
  const candidate =
    Array.isArray(parsed)
      ? parsed
      : parsed.turns ||
        parsed.messages ||
        parsed.transcript ||
        parsed.transcript_with_tool_calls ||
        parsed.dialog ||
        parsed.conversation ||
        [];
  if (!Array.isArray(candidate)) return [];
  return candidate.map(normalizeTurn).filter((turn) => turn.text);
}

function turnsFromText(text) {
  const turns = [];
  const lines = text.split(/\r?\n/);
  let current = null;
  const flush = () => {
    if (current && current.text.trim()) {
      current.text = current.text.trim();
      current.index = turns.length;
      turns.push(current);
    }
  };
  for (const line of lines) {
    const match = line.match(/^\s*(caller|customer|user|agent|assistant|ai|voxten)\s*:\s*(.*)$/i);
    if (match) {
      flush();
      current = {
        role: normalizeRole(match[1]),
        text: match[2],
        endCall: /\bend_call\s*[:=]\s*true\b/i.test(line),
      };
    } else if (current) {
      current.text += ` ${line.trim()}`;
      current.endCall = current.endCall || /\bend_call\s*[:=]\s*true\b/i.test(line);
    }
  }
  flush();
  return turns;
}

function loadTranscript(filePath) {
  const raw = fs.readFileSync(filePath, "utf8");
  if (path.extname(filePath).toLowerCase() === ".json") {
    return turnsFromJson(JSON.parse(raw));
  }
  try {
    return turnsFromJson(JSON.parse(raw));
  } catch {
    return turnsFromText(raw);
  }
}

function countQuestions(text) {
  return (String(text || "").match(/\?/g) || []).length;
}

function containsAny(text, terms) {
  const lowered = String(text || "").toLowerCase();
  return terms.some((term) => lowered.includes(term));
}

function issue(type, severity, turn, evidence, recommendation) {
  return {
    type,
    severity,
    turn: turn ? turn.index : -1,
    role: turn ? turn.role : "",
    evidence,
    recommendation,
  };
}

function looksLikeFinalCloseQuestion(text) {
  const lowered = String(text || "").toLowerCase();
  return (
    lowered.includes("will that be all") ||
    lowered.includes("anything else") ||
    lowered.includes("anything more") ||
    lowered.includes("is there anything else")
  );
}

function looksLikeAffirmativeClose(text) {
  const lowered = String(text || "").toLowerCase().replace(/[^a-z0-9 ]/g, " ");
  return /\b(no|nope|nah|that is all|that's all|all good|nothing else|yes that is all|yes that s all|yeah that is all|yeah that s all|correct|yes|yeah|yep)\b/.test(lowered);
}

function asksNewIntakeQuestion(text) {
  const lowered = String(text || "").toLowerCase();
  return (
    countQuestions(text) > 0 &&
    containsAny(lowered, [
      "what is your",
      "can i get",
      "may i have",
      "what vehicle",
      "what day",
      "what time",
      "callback",
      "phone number",
      "last name",
      "spell",
    ])
  );
}

function hasResolvedDateReadback(text) {
  return /(?:monday|tuesday|wednesday|thursday|friday|saturday|sunday|january|february|march|april|may|june|july|august|september|october|november|december|\b20\d\d\b)/i.test(text);
}

function duplicateDateReadback(text) {
  const lowered = String(text || "").toLowerCase();
  const dateWords = [
    "monday",
    "tuesday",
    "wednesday",
    "thursday",
    "friday",
    "saturday",
    "sunday",
    "january",
    "february",
    "march",
    "april",
    "may",
    "june",
    "july",
    "august",
    "september",
    "october",
    "november",
    "december",
  ];
  let hits = 0;
  for (const word of dateWords) {
    const matches = lowered.match(new RegExp(`\\b${word}\\b`, "g")) || [];
    hits += matches.length;
  }
  const yearHits = (lowered.match(/\b20\d\d\b/g) || []).length;
  return hits >= 2 || yearHits >= 2;
}

function analyzeTurns(turns, source = "") {
  turns = turns.map((turn, index) => ({
    index,
    role: normalizeRole(turn.role),
    text: getText(turn).trim(),
    endCall: Boolean(turn.endCall || turn.end_call),
  })).filter((turn) => turn.text);
  const issues = [];
  for (const turn of turns) {
    if (turn.role !== "assistant") continue;
    if (countQuestions(turn.text) > 1) {
      issues.push(issue(
        "multi_question_response",
        "medium",
        turn,
        turn.text,
        "Keep the assistant to one question per turn.",
      ));
    }
    if (turn.text.length > 18 && !/[.!?]"?$/.test(turn.text.trim())) {
      issues.push(issue(
        "incomplete_response",
        "medium",
        turn,
        turn.text,
        "Check whether the TTS response was cut off or generated without terminal punctuation.",
      ));
    }
    if (/^\s*(here are|the following|options include)/i.test(turn.text) || /\n\s*(?:[-*]|\d+\.)\s+/.test(turn.text)) {
      issues.push(issue(
        "robotic_kb_answer",
        "medium",
        turn,
        turn.text,
        "Answer as a spoken summary instead of exposing KB/list structure.",
      ));
    }
    if (/is that right\?/i.test(turn.text) && duplicateDateReadback(turn.text)) {
      issues.push(issue(
        "duplicate_callback_readback",
        "high",
        turn,
        turn.text,
        "Render callback confirmation from one canonical callback slot.",
      ));
    }
  }

  for (let index = 0; index < turns.length; index += 1) {
    const turn = turns[index];
    if (turn.role !== "caller") continue;
    const callerAskedRelativeDate = containsAny(turn.text, [
      "tomorrow",
      "after tomorrow",
      "two weeks",
      "next week",
      "monday",
      "tuesday",
      "wednesday",
      "thursday",
      "friday",
    ]);
    if (callerAskedRelativeDate) {
      const nextAssistant = turns.slice(index + 1).find((candidate) => candidate.role === "assistant");
      if (nextAssistant && /what (day|date|time)/i.test(nextAssistant.text) && !hasResolvedDateReadback(nextAssistant.text)) {
        issues.push(issue(
          "relative_date_not_resolved",
          "high",
          nextAssistant,
          `caller=${turn.text} | assistant=${nextAssistant.text}`,
          "Resolve relative date/time before asking again when the caller gave enough information.",
        ));
      }
    }
    if (countQuestions(turn.text) > 1) {
      const nextAssistant = turns.slice(index + 1).find((candidate) => candidate.role === "assistant");
      if (nextAssistant && !/other thing|other question|second thing/i.test(nextAssistant.text)) {
        issues.push(issue(
          "multi_question_caller_turn_unrecovered",
          "medium",
          nextAssistant,
          `caller=${turn.text} | assistant=${nextAssistant.text}`,
          "Answer one approved question, then ask what the other thing was.",
        ));
      }
    }
  }

  for (let index = 0; index < turns.length; index += 1) {
    const turn = turns[index];
    if (turn.role !== "assistant" || !looksLikeFinalCloseQuestion(turn.text)) continue;
    const caller = turns.slice(index + 1).find((candidate) => candidate.role === "caller");
    if (!caller || !looksLikeAffirmativeClose(caller.text)) continue;
    const followingAssistant = turns.slice(caller.index + 1).find((candidate) => candidate.role === "assistant");
    const endCallTurn = turns.slice(caller.index + 1).find((candidate) => candidate.role === "assistant" && candidate.endCall);
    if (!endCallTurn) {
      issues.push(issue(
        "call_end_missing_end_call",
        "high",
        caller,
        `assistant=${turn.text} | caller=${caller.text}`,
        "Final close acceptance must produce a Retell response with end_call: true.",
      ));
    }
    if (followingAssistant && asksNewIntakeQuestion(followingAssistant.text)) {
      issues.push(issue(
        "call_end_restarted_intake",
        "high",
        followingAssistant,
        followingAssistant.text,
        "After final close acceptance, do not ask another intake question.",
      ));
    }
  }

  return {
    source,
    turns: turns.length,
    issueCount: issues.length,
    issues,
  };
}

function printTextReport(results) {
  let total = 0;
  for (const result of results) {
    total += result.issueCount;
    console.log(`\n${result.source}`);
    console.log(`turns=${result.turns} issues=${result.issueCount}`);
    if (result.issues.length === 0) {
      console.log("ok");
      continue;
    }
    for (const found of result.issues) {
      console.log(`- [${found.severity}] ${found.type} turn=${found.turn}`);
      console.log(`  evidence: ${found.evidence}`);
      console.log(`  next: ${found.recommendation}`);
    }
  }
  console.log(`\nfiles=${results.length} total_issues=${total}`);
}

export { analyzeTurns, loadTranscript };

if (import.meta.url === `file://${process.argv[1]}`) {
  const json = process.argv.includes("--json");
  const files = process.argv.filter((arg) => !arg.startsWith("--")).slice(2);
  if (files.length === 0) {
    console.error("Usage: node scripts/transcript_analysis.mjs [--json] transcript.txt transcript.json ...");
    process.exit(2);
  }
  const results = files.map((file) => analyzeTurns(loadTranscript(file), file));
  if (json) {
    console.log(JSON.stringify(results, null, 2));
  } else {
    printTextReport(results);
  }
}
