# Fly Redeploy Notes

Use this when moving the v3 app to a new Fly account.

## Current Local App

- Repo: `retell-cerebras-v3`
- Existing Fly app name in `fly.toml`: `voxten-cerebras-v3-2`
- Region: `yyz`
- Internal port: `8080`
- Health check: `GET /health`
- Dockerfile: `Dockerfile`

## Important

Fly secrets cannot be copied back out of Fly. Re-enter them from the original
source when creating the new app.

Required secrets:

```bash
CEREBRAS_API_KEY=...
RETELL_SHARED_SECRET=...
```

Optional secrets:

```bash
CEREBRAS_MODEL=gpt-oss-120b
CEREBRAS_BASE_URL=https://api.cerebras.ai/v1/chat/completions
EMPLOYEE_DELIVERY_WEBHOOK_URL=https://your-n8n-webhook-url
EMPLOYEE_DELIVERY_WEBHOOK_SECRET=...
```

## Recreate On A New Fly Account

Log in:

```bash
flyctl auth logout
flyctl auth login
```

Create a new app. Use a new app name if the old one is unavailable:

```bash
flyctl apps create voxten-cerebras-v3
```

Update `fly.toml`:

```toml
app = "voxten-cerebras-v3"
primary_region = "yyz"
```

Set secrets:

```bash
flyctl secrets set CEREBRAS_API_KEY="..." RETELL_SHARED_SECRET="..." --app voxten-cerebras-v3
```

If using n8n delivery:

```bash
flyctl secrets set EMPLOYEE_DELIVERY_WEBHOOK_URL="..." EMPLOYEE_DELIVERY_WEBHOOK_SECRET="..." --app voxten-cerebras-v3
```

Deploy:

```bash
flyctl deploy --app voxten-cerebras-v3
```

Verify:

```bash
curl -sS https://voxten-cerebras-v3.fly.dev/health
```

Expected:

```json
{"ok":true,"service":"retell-cerebras-v3"}
```
