#!/usr/bin/env node

const targetUrl = process.env.PILOT_TEST_URL || "http://127.0.0.1:8098/test-chat";
const sharedSecret = process.env.RETELL_SHARED_SECRET || "therealtestingsecretforcodex";
const experimentalSanitize = process.env.PILOT_SANITIZE_RESPONSES === "1";
const maxTurns = 12;

const bannedPhrases = [
  "for our records",
  "assist you better",
  "assist you further",
  "service appointment",
  "schedule an appointment",
  "mr.",
  "mr ",
  "ms.",
  "ms ",
  "mrs.",
  "mrs "
];

const genericVehicles = [
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
  "hyundai"
];

const scenarios = [
  {
    id: "service_check_engine_hot_smell",
    department: "service",
    opening: "my check engine light is on and the car smells hot after charging",
    answers: {
      department: "service",
      intent: "check engine light and hot smell",
      name: "Jordan Smith",
      last_name_spelling: "S M I T H",
      vehicle: "2022 Hyundai Ioniq 5",
      request: "check engine light and hot smell after charging",
      callback_time: "next Tuesday morning",
      phone: "416 555 0199",
      phone_confirmed: "yes that is correct",
      final_confirmed: "yes everything is correct"
    },
    expect: {
      department: "service",
      name: "Jordan Smith",
      spelling: "SMITH",
      vehicle: "Ioniq 5",
      phone: "4165550199",
      requestAny: ["check engine", "hot smell", "smells hot", "charging"]
    }
  },
  {
    id: "service_recall_letter",
    department: "service",
    opening: "I got a recall letter for my 2021 Hyundai Tucson",
    answers: {
      department: "service",
      name: "Morgan Lee",
      last_name_spelling: "L E E",
      callback_time: "Monday around 10",
      phone: "289 555 0144",
      phone_confirmed: "yes",
      final_confirmed: "yes that is right"
    },
    expect: {
      department: "service",
      name: "Morgan Lee",
      spelling: "LEE",
      vehicle: "2021 Hyundai Tucson",
      phone: "2895550144",
      requestAny: ["recall", "letter", "tucson"]
    }
  },
  {
    id: "service_maintenance_light",
    department: "service",
    opening: "my maintenance light came on in my Elantra",
    answers: {
      name: "Casey Williams",
      last_name_spelling: "W I L L I A M S",
      callback_time: "Wednesday morning",
      phone: "705 555 3030",
      phone_confirmed: "correct",
      final_confirmed: "yes correct"
    },
    expect: {
      department: "service",
      name: "Casey Williams",
      spelling: "WILLIAMS",
      vehicle: "Elantra",
      phone: "7055553030",
      requestAny: ["maintenance", "light", "elantra"]
    }
  },
  {
    id: "service_warranty_noise",
    department: "service",
    opening: "I have a warranty question about a noise in my 2020 Santa Fe",
    answers: {
      name: "Riley Brown",
      last_name_spelling: "B R O W N",
      callback_time: "Thursday afternoon",
      phone: "613 555 2222",
      phone_confirmed: "yep",
      final_confirmed: "all correct"
    },
    expect: {
      department: "service",
      name: "Riley Brown",
      spelling: "BROWN",
      vehicle: "2020 Santa Fe",
      phone: "6135552222",
      requestAny: ["warranty", "noise", "santa fe"]
    }
  },
  {
    id: "parts_charge_port_door",
    department: "parts",
    opening: "I need the plastic charge port door for an Ioniq 5",
    answers: {
      name: "Alex Rivera",
      last_name_spelling: "R I V E R A",
      callback_time: "tomorrow afternoon",
      phone: "647 555 0123",
      phone_confirmed: "correct",
      final_confirmed: "yes that is correct"
    },
    expect: {
      department: "parts",
      name: "Alex Rivera",
      spelling: "RIVERA",
      vehicle: "Ioniq 5",
      phone: "6475550123",
      requestAny: ["charge port", "door", "ioniq"]
    }
  },
  {
    id: "parts_cargo_mat",
    department: "parts",
    opening: "do you have a rubber cargo mat for a Palisade",
    answers: {
      name: "Taylor Nguyen",
      last_name_spelling: "N G U Y E N",
      callback_time: "Wednesday around 10",
      phone: "519 555 9911",
      phone_confirmed: "right",
      final_confirmed: "yup"
    },
    expect: {
      department: "parts",
      name: "Taylor Nguyen",
      spelling: "NGUYEN",
      vehicle: "Palisade",
      phone: "5195559911",
      requestAny: ["rubber", "cargo", "mat", "palisade"]
    }
  },
  {
    id: "parts_wiper_blades",
    department: "parts",
    opening: "need wiper blades for a 2020 Elantra",
    answers: {
      name: "Chris OBrien",
      last_name_spelling: "O B R I E N",
      callback_time: "tomorrow before 5",
      phone: "226 555 4567",
      phone_confirmed: "yes",
      final_confirmed: "correct"
    },
    expect: {
      department: "parts",
      name: "Chris OBrien",
      spelling: "OBRIEN",
      vehicle: "2020 Elantra",
      phone: "2265554567",
      requestAny: ["wiper", "blades", "elantra"]
    }
  },
  {
    id: "parts_key_fob_battery",
    department: "parts",
    opening: "I need a key fob battery for my Tucson",
    answers: {
      name: "Avery Martin",
      last_name_spelling: "M A R T I N",
      callback_time: "Friday afternoon",
      phone: "365 555 9876",
      phone_confirmed: "yes",
      final_confirmed: "correct"
    },
    expect: {
      department: "parts",
      name: "Avery Martin",
      spelling: "MARTIN",
      vehicle: "Tucson",
      phone: "3655559876",
      requestAny: ["key fob", "battery", "tucson"]
    }
  },
  {
    id: "sales_test_drive_trade",
    department: "sales",
    opening: "I want to test drive a Santa Fe hybrid and talk about trading in my old car",
    answers: {
      name: "Sam Patel",
      last_name_spelling: "P A T E L",
      callback_time: "Friday after lunch",
      phone: "905 555 7788",
      phone_confirmed: "yes correct",
      final_confirmed: "yes all correct"
    },
    expect: {
      department: "sales",
      name: "Sam Patel",
      spelling: "PATEL",
      vehicle: "Santa Fe hybrid",
      phone: "9055557788",
      requestAny: ["test drive", "santa fe", "trade"]
    }
  },
  {
    id: "sales_lease_tucson",
    department: "sales",
    opening: "I am looking for lease numbers on a new Tucson",
    answers: {
      name: "Jamie Thompson",
      last_name_spelling: "T H O M P S O N",
      callback_time: "Saturday morning",
      phone: "343 555 1010",
      phone_confirmed: "yep that is mine",
      final_confirmed: "yes"
    },
    expect: {
      department: "sales",
      name: "Jamie Thompson",
      spelling: "THOMPSON",
      vehicle: "Tucson",
      phone: "3435551010",
      requestAny: ["lease", "tucson"]
    }
  },
  {
    id: "sales_ev_interest",
    department: "sales",
    opening: "I want to test drive whatever electric Hyundai is not tiny",
    answers: {
      name: "Quinn Davis",
      last_name_spelling: "D A V I S",
      callback_time: "tomorrow morning",
      phone: "437 555 0155",
      phone_confirmed: "correct",
      final_confirmed: "yes correct"
    },
    expect: {
      department: "sales",
      name: "Quinn Davis",
      spelling: "DAVIS",
      vehicle: "electric Hyundai",
      phone: "4375550155",
      requestAny: ["test drive", "electric", "not tiny", "hyundai"]
    }
  },
  {
    id: "sales_used_palisade",
    department: "sales",
    opening: "do you have any used Palisades available",
    answers: {
      name: "Parker Wilson",
      last_name_spelling: "W I L S O N",
      callback_time: "next Monday afternoon",
      phone: "416 555 0188",
      phone_confirmed: "yes",
      final_confirmed: "yes that is right"
    },
    expect: {
      department: "sales",
      name: "Parker Wilson",
      spelling: "WILSON",
      vehicle: "Palisade",
      phone: "4165550188",
      requestAny: ["used", "palisade", "available", "inventory"]
    }
  }
];

function normalize(value) {
  return String(value || "").trim().toLowerCase();
}

function compactLetters(value) {
  return String(value || "").replace(/[^a-zA-Z]/g, "").toUpperCase();
}

function digits(value) {
  return String(value || "").replace(/\D/g, "");
}

function includesLoose(haystack, needle) {
  return normalize(haystack).includes(normalize(needle));
}

function hasAnyKeyword(state, keywords) {
  const text = `${state.intent || ""} ${state.request || ""} ${state.vehicle || ""}`;
  return keywords.some((keyword) => includesLoose(text, keyword));
}

function vehicleMatches(actual, expected) {
  if (includesLoose(actual, expected)) {
    return true;
  }
  const actualText = normalize(actual);
  const words = normalize(expected).split(/\s+/).filter((word) => word.length > 0);
  return words.length > 0 && words.every((word) => actualText.includes(word));
}

function isGenericVehicle(value) {
  const text = normalize(value);
  return genericVehicles.includes(text);
}

function hasBannedWording(content) {
  const text = normalize(content);
  return bannedPhrases.some((phrase) => text.includes(phrase));
}

function hasMultipleQuestions(content) {
  return (String(content || "").match(/\?/g) || []).length > 1;
}

function sanitizeResponse(content) {
  let text = String(content || "");
  text = text.replace(/\s*for our records\b/gi, "");
  text = text.replace(/\s*so I can assist you (better|further)\b/gi, "");
  text = text.replace(/\bMr\.?\s+/g, "");
  text = text.replace(/\bMs\.?\s+/g, "");
  text = text.replace(/\bMrs\.?\s+/g, "");
  text = text.replace(/\bservice appointment\b/gi, "service request");
  text = text.replace(/\s+,/g, ",");
  text = text.replace(/\s+\?/g, "?");
  text = text.replace(/\s{2,}/g, " ");
  return text.trim();
}

function recentContext(history) {
  return history.slice(-3).map((item) => `${item.role}: ${item.text}`).join("\n");
}

async function postTurn(message, state, lastAssistant, history) {
  const started = Date.now();
  const response = await fetch(targetUrl, {
    method: "POST",
    headers: {
      "content-type": "application/json",
      "x-retell-secret": sharedSecret
    },
    body: JSON.stringify({
      message,
      state,
      last_assistant: lastAssistant,
      recent_context: recentContext(history)
    })
  });
  const ms = Date.now() - started;
  const text = await response.text();
  let json;
  try {
    json = JSON.parse(text);
  } catch (error) {
    throw new Error(`Bad JSON from ${targetUrl}: ${text}`);
  }
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}: ${text}`);
  }
  return { ms, json };
}

function answerFor(scenario, nextField) {
  if (scenario.answers[nextField]) {
    return scenario.answers[nextField];
  }
  if (nextField === "caller_name") {
    return scenario.answers.name;
  }
  if (nextField === "none") {
    return "";
  }
  return scenario.answers.request || "yes";
}

function scoreScenario(scenario, turns, state, completed) {
  const failures = [];
  const finalState = state || {};
  const expect = scenario.expect;
  let score = 0;

  if (finalState.department === expect.department) {
    score += 1;
  } else {
    failures.push(`department expected ${expect.department}, got ${finalState.department || "<empty>"}`);
  }

  if (hasAnyKeyword(finalState, expect.requestAny)) {
    score += 1;
  } else {
    failures.push(`intent/request missing expected keywords: ${expect.requestAny.join(", ")}`);
  }

  if (includesLoose(finalState.name, expect.name)) {
    score += 1;
  } else {
    failures.push(`name expected ${expect.name}, got ${finalState.name || "<empty>"}`);
  }

  if (compactLetters(finalState.spelling) === expect.spelling) {
    score += 1;
  } else {
    failures.push(`spelling expected ${expect.spelling}, got ${finalState.spelling || "<empty>"}`);
  }

  if (!isGenericVehicle(finalState.vehicle) && vehicleMatches(finalState.vehicle, expect.vehicle)) {
    score += 1;
  } else {
    failures.push(`vehicle expected ${expect.vehicle}, got ${finalState.vehicle || "<empty>"}`);
  }

  if (finalState.callback_time && !includesLoose(finalState.callback_time, "callback time")) {
    score += 1;
  } else {
    failures.push(`callback_time missing or placeholder: ${finalState.callback_time || "<empty>"}`);
  }

  if (digits(finalState.phone) === expect.phone && finalState.phone_confirmed === true) {
    score += 1;
  } else {
    failures.push(`phone/confirmation expected ${expect.phone} confirmed, got ${finalState.phone || "<empty>"} / ${finalState.phone_confirmed}`);
  }

  if (completed && finalState.final_confirmed === true) {
    score += 1;
  } else {
    failures.push("final confirmation did not complete");
  }

  const bannedTurns = turns.filter((turn) => hasBannedWording(turn.assistant));
  if (bannedTurns.length === 0) {
    score += 1;
  } else {
    failures.push(`banned wording in ${bannedTurns.length} turn(s)`);
  }

  const multiQuestionTurns = turns.filter((turn) => hasMultipleQuestions(turn.assistant));
  if (multiQuestionTurns.length === 0) {
    score += 1;
  } else {
    failures.push(`multiple questions in ${multiQuestionTurns.length} turn(s)`);
  }

  return { score, failures };
}

async function runScenario(scenario) {
  let state = {};
  let lastAssistant = "";
  let caller = scenario.opening;
  const history = [];
  const turns = [];
  let completed = false;

  for (let index = 0; index < maxTurns; index += 1) {
    const { ms, json } = await postTurn(caller, state, lastAssistant, history);
    const rawAssistant = json.content || "";
    const assistant = experimentalSanitize ? sanitizeResponse(rawAssistant) : rawAssistant;
    const nextField = json.next_field || "none";
    state = json.state || {};
    turns.push({ caller, assistant, rawAssistant, nextField, ms });
    history.push({ role: "caller", text: caller });
    history.push({ role: "assistant", text: assistant });
    lastAssistant = assistant;
    if (nextField === "none") {
      completed = true;
      break;
    }
    caller = answerFor(scenario, nextField);
  }

  const result = scoreScenario(scenario, turns, state, completed);
  return { scenario, turns, state, completed, ...result };
}

function percentile(values, ratio) {
  if (values.length === 0) {
    return 0;
  }
  const index = Math.min(values.length - 1, Math.floor(values.length * ratio));
  return values[index];
}

function printTurn(turn) {
  console.log(`  caller: ${turn.caller}`);
  console.log(`  assistant (${turn.ms}ms, next=${turn.nextField}): ${turn.assistant}`);
}

const results = [];
const latencies = [];

console.log(`Pilot acceptance target: ${targetUrl}`);
console.log(`Scenarios: ${scenarios.length}`);
console.log(`experimental_sanitize_responses: ${experimentalSanitize ? "on" : "off"}`);

for (const scenario of scenarios) {
  const result = await runScenario(scenario);
  results.push(result);
  result.turns.forEach((turn) => latencies.push(turn.ms));
  const status = result.score === 10 ? "PASS" : "FAIL";
  console.log(`\n${status} ${scenario.id}: ${result.score}/10`);
  if (process.env.PILOT_VERBOSE === "1" || result.score !== 10) {
    result.turns.forEach(printTurn);
    if (result.failures.length > 0) {
      console.log("  failures:");
      result.failures.forEach((failure) => console.log(`  - ${failure}`));
    }
    console.log(`  final_state: ${JSON.stringify(result.state)}`);
  }
}

latencies.sort((left, right) => left - right);
const totalScore = results.reduce((sum, result) => sum + result.score, 0);
const completedCalls = results.filter((result) => result.completed).length;
const bannedFailures = results.filter((result) =>
  result.turns.some((turn) => hasBannedWording(turn.assistant))
).length;
const multiQuestionFailures = results.filter((result) =>
  result.turns.some((turn) => hasMultipleQuestions(turn.assistant))
).length;
const median = percentile(latencies, 0.5);
const p90 = percentile(latencies, 0.9);

console.log("\nSummary");
console.log(`score: ${totalScore}/120`);
console.log(`completed_calls: ${completedCalls}/12`);
console.log(`banned_wording_call_failures: ${bannedFailures}`);
console.log(`multi_question_call_failures: ${multiQuestionFailures}`);
console.log(`latency_median_ms: ${median}`);
console.log(`latency_p90_ms: ${p90}`);
console.log(`latency_max_ms: ${latencies[latencies.length - 1] || 0}`);

const passed =
  totalScore >= 108 &&
  completedCalls >= 11 &&
  bannedFailures === 0 &&
  multiQuestionFailures === 0 &&
  median < 800 &&
  p90 < 1200;

console.log(`result: ${passed ? "PASS" : "FAIL"}`);
process.exit(passed ? 0 : 1);
