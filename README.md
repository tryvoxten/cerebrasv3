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
export CEREBRAS_MODEL=llama3.1-8b
export PORT=8098
```

Optional:

```bash
export CEREBRAS_BASE_URL=https://api.cerebras.ai/v1/chat/completions
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
  -d '{"message":"my dash is yelling maintenance at me","state":{}}'
```

The response includes the updated state, chosen next field, whether Cerebras was
used for interpretation/response, and the caller-facing content.

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
