import { spawn, spawnSync } from "node:child_process";
import { createServer } from "node:http";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const harness = resolve(repoRoot, "scripts/pilot_acceptance.mjs");

function expect(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

function runHarness(environment) {
  return new Promise((resolveRun, rejectRun) => {
    const child = spawn(process.execPath, [harness], {
      cwd: repoRoot,
      env: environment,
      stdio: ["ignore", "pipe", "pipe"]
    });
    let output = "";
    child.stdout.on("data", (chunk) => { output += chunk.toString(); });
    child.stderr.on("data", (chunk) => { output += chunk.toString(); });
    child.on("error", rejectRun);
    child.on("close", (status) => resolveRun({ status, output }));
  });
}

async function withResponse(statusCode, body, test) {
  const server = createServer((request, response) => {
    request.resume();
    response.writeHead(statusCode, { "content-type": "application/json" });
    response.end(JSON.stringify(body));
  });
  await new Promise((resolveListen) => server.listen(0, "127.0.0.1", resolveListen));
  try {
    const address = server.address();
    await test(`http://127.0.0.1:${address.port}/test-chat`);
  } finally {
    await new Promise((resolveClose) => server.close(resolveClose));
  }
}

const missingSecretEnvironment = { ...process.env };
delete missingSecretEnvironment.RETELL_SHARED_SECRET;
const missingSecret = spawnSync(process.execPath, [harness], {
  cwd: repoRoot,
  env: missingSecretEnvironment,
  encoding: "utf8"
});
expect(missingSecret.status === 2, "missing secret must exit with status 2");
expect(
  `${missingSecret.stdout}${missingSecret.stderr}`.includes("Set RETELL_SHARED_SECRET"),
  "missing secret must explain the required variable"
);

await withResponse(404, { error: "unauthorized" }, async (url) => {
  const result = await runHarness({
    ...process.env,
    RETELL_SHARED_SECRET: "test-secret",
    PILOT_TEST_URL: url
  });
  expect(result.status === 1, "authentication failure must exit with status 1");
  expect(result.output.includes("Authentication failed"), "authentication failure must be explicit");
});

await withResponse(200, {
  content: "What can the team help you with?",
  next_field: "intent",
  state: {},
  used_interpreter: false
}, async (url) => {
  const result = await runHarness({
    ...process.env,
    RETELL_SHARED_SECRET: "test-secret",
    PILOT_TEST_URL: url
  });
  expect(result.status === 1, "missing interpreter must exit with status 1");
  expect(
    result.output.includes("first turn did not use the Cerebras interpreter"),
    "missing interpreter must explain the target configuration problem"
  );
});

console.log("pilot_acceptance_config_tests: PASS");
