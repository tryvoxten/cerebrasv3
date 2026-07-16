#!/usr/bin/env node
import assert from "node:assert/strict";
import { analyzeTurns } from "../scripts/transcript_analysis.mjs";

function issueTypes(result) {
  return new Set(result.issues.map((issue) => issue.type));
}

{
  const result = analyzeTurns([
    { role: "assistant", text: "I'll pass this to the service team so they can call you back. Will that be all?", endCall: false },
    { role: "caller", text: "Yep, that's all.", endCall: false },
    { role: "assistant", text: "What day and time works best for a callback?", endCall: false },
  ], "bad-close");
  const types = issueTypes(result);
  assert.equal(types.has("call_end_missing_end_call"), true);
  assert.equal(types.has("call_end_restarted_intake"), true);
}

{
  const result = analyzeTurns([
    { role: "assistant", text: "I'll pass this to the service team so they can call you back. Will that be all?", endCall: false },
    { role: "caller", text: "No, that's all.", endCall: false },
    { role: "assistant", text: "Got it, the team will follow up.", endCall: true },
  ], "good-close");
  assert.equal(result.issueCount, 0);
}

{
  const result = analyzeTurns([
    { role: "caller", text: "Two weeks from now at 4." },
    { role: "assistant", text: "What day and time works best for a callback?" },
  ], "relative-date");
  assert.equal(issueTypes(result).has("relative_date_not_resolved"), true);
}

{
  const result = analyzeTurns([
    { role: "assistant", text: "Okay, July 15, 2026 Wednesday, July 22, 2026 at 4 PM. Is that right?" },
  ], "duplicate-date");
  assert.equal(issueTypes(result).has("duplicate_callback_readback"), true);
}

{
  const result = analyzeTurns([
    { role: "assistant", text: "Here are the loaner details:\n- Loaners may be available\n- Ask the service team" },
  ], "robotic-kb");
  assert.equal(issueTypes(result).has("robotic_kb_answer"), true);
}

console.log("transcript_analysis_tests: PASS");
