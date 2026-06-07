#!/usr/bin/env node
import readline from "node:readline/promises";
import { stdin as input, stdout as output } from "node:process";

const url = process.env.CHAT_URL || "https://voxten-cerebras-v3.fly.dev/test-chat";
const secret = process.env.RETELL_SHARED_SECRET;

if (!secret) {
  console.error("Set RETELL_SHARED_SECRET first.");
  console.error("Example: RETELL_SHARED_SECRET='...' node scripts/chat.mjs");
  process.exit(1);
}

let state = {};
let lastAssistant = "";
let recentContext = "";
const callId = `terminal-${Date.now()}`;

async function turn(message) {
  const response = await fetch(url, {
    method: "POST",
    headers: {
      authorization: `Bearer ${secret}`,
      "content-type": "application/json",
    },
    body: JSON.stringify({
      call_id: callId,
      message,
      state,
      last_assistant: lastAssistant,
      recent_context: recentContext,
    }),
  });

  const text = await response.text();
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}: ${text}`);
  }

  const json = JSON.parse(text);
  state = json.state || state;
  lastAssistant = json.content || "";
  recentContext = `${recentContext}\nuser: ${message}\nagent: ${lastAssistant}`.slice(-700);

  console.log(`\nAgent: ${lastAssistant}`);
  console.log(
    `debug: next=${json.next_field || ""} interpreter=${json.used_interpreter} turn=${json.turn_type || ""} faq=${json.faq_id || ""}`,
  );
}

const rl = readline.createInterface({ input, output });

console.log(`Chat URL: ${url}`);
console.log(`Call ID: ${callId}`);
console.log("Press enter once to make the agent speak first. Type /quit to exit.\n");

while (true) {
  const message = await rl.question("You: ");
  if (message.trim() === "/quit") {
    break;
  }
  try {
    await turn(message);
  } catch (error) {
    console.error(String(error.message || error));
  }
}

rl.close();
