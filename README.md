# Retell Cerebras V3

Fresh v3 prototype for the dealership receptionist flow.

Goal:

```text
latest caller turn
-> Cerebras interprets compact JSON
-> code merges fields into state
-> code chooses exactly one next field
-> deterministic or Cerebras-generated sentence for Retell TTS
```

The model does not decide the flow. It interprets and words the sentence. The
planner decides what is missing next.

## Environment

```bash
export RETELL_SHARED_SECRET=therealtestingsecretforcodex
export CEREBRAS_API_KEY=...
export CEREBRAS_MODEL=gpt-oss-120b
export PORT=8098
```

Optional:

```bash
export CEREBRAS_BASE_URL=https://api.cerebras.ai/v1/chat/completions
export EMPLOYEE_DELIVERY_WEBHOOK_URL=https://example.com/voxten/call-summary
export EMPLOYEE_DELIVERY_WEBHOOK_SECRET=...
```

## Build

```bash
make check
./build/retell_cerebras_v3
```

## Test

```bash
curl -sS http://127.0.0.1:8098/test-chat \
  -H "authorization: Bearer therealtestingsecretforcodex" \
  -H "content-type: application/json" \
  -d '{"call_id":"manual-test-001","message":"my dash is yelling maintenance at me","state":{}}'
```

The response includes the updated state, chosen next field, whether Cerebras was
used for interpretation/response, the stable `call_id`, and the caller-facing
content. If the caller platform sends `call_id`, `callId`, `call_uuid`, or
`retell_call_id`, the app preserves it. Otherwise it generates a local call ID
and stores it in `state.call_id`.

Production turn endpoints:

```text
POST /retell
POST /call-turn
POST /test-chat
```

All three use the same handler. `/test-chat` is kept for compatibility with the
pilot scripts; `/retell` is the cleaner production path.

When a call reaches final confirmation, the response also includes an
`employee_summary` object with the department, caller, vehicle, request,
callback time, and phone number. If `EMPLOYEE_DELIVERY_WEBHOOK_URL` is set, the
service posts that summary to the webhook once and stores `delivery_sent` in the
returned state to prevent duplicate delivery.

## Debugging

The app writes low-PII JSON debug events to stderr, which show up in Fly logs.
Search by `call_id` to trace a full call:

```bash
flyctl logs --app voxten-cerebras-v3
```

Key events:

```text
websocket_open
websocket_response_required
turn_processed
websocket_response_sent
employee_delivery
websocket_close
http_unauthorized
http_not_found
```

`turn_processed` shows the source (`http` or `websocket`), chosen department,
next field, whether the interpreter/generator/KB answer ran, and whether
employee delivery was attempted. It does not log the raw caller transcript by
default.

`employee_delivery` shows the n8n handoff result, including `http_status`,
`curl_code`, and `sent`.

For temporary deeper model debugging:

```bash
flyctl secrets set CEREBRAS_DEBUG=1 --app voxten-cerebras-v3
```

Turn it back off after testing because it can log raw model/interpreter details:

```bash
flyctl secrets unset CEREBRAS_DEBUG --app voxten-cerebras-v3
```

## n8n Employee Email Delivery

The app connects to n8n with a regular HTTPS webhook. No websocket is required.

Set these two environment variables on Fly:

```bash
EMPLOYEE_DELIVERY_WEBHOOK_URL=https://YOUR-NGROK-OR-N8N-DOMAIN/webhook/voxten/v1/employee-delivery/secure/YOUR_PATH_SECRET/summary
EMPLOYEE_DELIVERY_WEBHOOK_SECRET=YOUR_HEADER_SECRET
```

The app sends:

```http
POST $EMPLOYEE_DELIVERY_WEBHOOK_URL
content-type: application/json
x-voxten-secret: $EMPLOYEE_DELIVERY_WEBHOOK_SECRET
```

With this JSON body:

```json
{
  "event": "call_summary_ready",
  "call_id": "call_abc123",
  "department": "service",
  "summary": "After-hours service callback request for Jordan Smith about truck warning light.",
  "caller_name": "Jordan Smith",
  "last_name_spelling": "S M I T H",
  "vehicle": "Ford F-150",
  "request": "truck warning light",
  "intent": "diagnostic",
  "callback_time": "next Tuesday morning",
  "phone": "416 555 0199",
  "phone_confirmed": true,
  "final_confirmed": true
}
```

In n8n, create a Webhook node with this path, without the leading
`/webhook/` or `/webhook-test/` prefix:

```text
voxten/v1/employee-delivery/secure/YOUR_PATH_SECRET/summary
```

Use the n8n test URL while editing:

```text
https://YOUR-NGROK-OR-N8N-DOMAIN/webhook-test/voxten/v1/employee-delivery/secure/YOUR_PATH_SECRET/summary
```

Use the production URL after activating the workflow:

```text
https://YOUR-NGROK-OR-N8N-DOMAIN/webhook/voxten/v1/employee-delivery/secure/YOUR_PATH_SECRET/summary
```

To set it on Fly:

```bash
flyctl secrets set \
  EMPLOYEE_DELIVERY_WEBHOOK_URL="https://YOUR-NGROK-OR-N8N-DOMAIN/webhook/voxten/v1/employee-delivery/secure/YOUR_PATH_SECRET/summary" \
  EMPLOYEE_DELIVERY_WEBHOOK_SECRET="YOUR_HEADER_SECRET" \
  --app YOUR_FLY_APP_NAME
```

## Pilot Acceptance Batch

The pilot standard lives in `PILOT_TEST_STANDARD.md`. Run the fixed 12-call
acceptance batch against a local server:

```bash
make pilot
```

Run it against Fly:

```bash
PILOT_TEST_URL=https://voxten-cerebras-v3.fly.dev/test-chat make pilot
```

Useful options:

```bash
PILOT_VERBOSE=1 make pilot
RETELL_SHARED_SECRET=... make pilot
```

The batch passes at `108/120` or higher only if there are no banned wording
failures, no multi-question failures, median latency is under `800 ms`, and P90
latency is under `1200 ms`.
