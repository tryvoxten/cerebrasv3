#define main retell_server_main
#include "../src/main.cpp"
#undef main

static int failures = 0;

static void expect_text(const char* actual, const char* expected, const char* message)
{
  if ((actual == 0) || (expected == 0) || (std::strcmp(actual, expected) != 0))
  {
    std::fprintf(stderr, "FAIL: %s\n", message);
    failures += 1;
  }
}

static void websocket_path_skips_full_length_secret(void)
{
  Config config;
  cerebras_v3::State state;
  char request[512];
  const char* secret = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  clear_buffer(request, 512);
  load_config(0, &config);
  cerebras_v3::copy_text(config.shared_secret, secret, 128);
  cerebras_v3::init_state(&state);
  std::snprintf(
    request,
    sizeof(request),
    "GET /llm-websocket/%s/retell-call-123 HTTP/1.1\r\nHost: localhost\r\n\r\n",
    secret);

  set_call_id_from_websocket_path(&state, request, &config);

  expect_text(state.call_id, "retell-call-123", "64-character secret segment is skipped");
}

static void websocket_path_uses_first_non_secret_segment(void)
{
  Config config;
  cerebras_v3::State state;
  const char* request =
    "GET /llm-websocket/short-secret/retell-call-456 HTTP/1.1\r\n"
    "Host: localhost\r\n\r\n";
  load_config(0, &config);
  cerebras_v3::copy_text(config.shared_secret, "short-secret", 128);
  cerebras_v3::init_state(&state);

  set_call_id_from_websocket_path(&state, request, &config);

  expect_text(state.call_id, "retell-call-456", "short secret segment is skipped");
}

static void websocket_path_uses_last_segment_after_static_prefix(void)
{
  Config config;
  cerebras_v3::State state;
  const char* request =
    "GET /llm-websocket/retell/retell-call-789?secret=hidden HTTP/1.1\r\n"
    "Host: localhost\r\n\r\n";
  load_config(0, &config);
  cerebras_v3::copy_text(config.shared_secret, "hidden", 128);
  cerebras_v3::init_state(&state);

  set_call_id_from_websocket_path(&state, request, &config);

  expect_text(state.call_id, "retell-call-789", "last path segment becomes the Retell call ID");
}

int main(void)
{
  websocket_path_skips_full_length_secret();
  websocket_path_uses_first_non_secret_segment();
  websocket_path_uses_last_segment_after_static_prefix();
  if (failures == 0)
  {
    std::printf("main_tests: PASS\n");
    return 0;
  }
  return 1;
}
