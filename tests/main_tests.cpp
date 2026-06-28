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

static void response_flags_default_off_and_parse_explicit_values(void)
{
  Config config;
  char structured[] = "STRUCTURED_RESPONSES_ENABLED=true";
  char ai_slots[] = "AI_RESPONSE_SLOTS_ENABLED=1";
  char* env[] = {structured, ai_slots, 0};
  load_config(0, &config);
  if (config.structured_responses || config.ai_response_slots)
  {
    std::fprintf(stderr, "FAIL: response feature flags default off\n");
    failures += 1;
  }
  load_config(env, &config);
  if (!config.structured_responses || !config.ai_response_slots)
  {
    std::fprintf(stderr, "FAIL: response feature flags parse enabled values\n");
    failures += 1;
  }
}

static void structured_opening_uses_after_hours_identity(void)
{
  Config config;
  cerebras_v3::State state;
  Turn_result result;
  load_config(0, &config);
  config.structured_responses = true;
  cerebras_v3::init_state(&state);
  process_chat_turn(&state, &config, "", "", "", &result);
  expect_text(
    result.response_text,
    "Thanks for calling. I'm the after-hours assistant. How can I help?",
    "structured opening identifies after-hours assistant");
}

static void structured_response_composes_without_ai(void)
{
  Config config;
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  Turn_result result;
  load_config(0, &config);
  config.structured_responses = true;
  config.ai_response_slots = false;
  cerebras_v3::init_state(&state);
  state.department = cerebras_v3::department_service;
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_department].value, "service", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_department].status = cerebras_v3::status_captured;
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_intent].value, "repair", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_intent].status = cerebras_v3::status_captured;
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_request].value, "clicking noise", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_request].status = cerebras_v3::status_captured;
  state.last_requested = cerebras_v3::field_none;
  cerebras_v3::clear_interpretation(&interpretation);
  plan = cerebras_v3::plan_next(&state);
  clear_turn_result(&result);
  if (!try_structured_response(&state, &config, &plan, &interpretation, cerebras_v3::field_none, "", &result))
  {
    std::fprintf(stderr, "FAIL: structured response composes without AI\n");
    failures += 1;
  }
  if ((std::strstr(result.response_text, "clicking noise") == 0) ||
      (std::strchr(result.response_text, '?') == 0) ||
      result.used_generator)
  {
    std::fprintf(stderr, "FAIL: structured preset response is grounded and asks once\n");
    failures += 1;
  }
  if (state.history.recent_structure_count != 1)
  {
    std::fprintf(stderr, "FAIL: structured response history persists\n");
    failures += 1;
  }
}

int main(void)
{
  websocket_path_skips_full_length_secret();
  websocket_path_uses_first_non_secret_segment();
  websocket_path_uses_last_segment_after_static_prefix();
  response_flags_default_off_and_parse_explicit_values();
  structured_opening_uses_after_hours_identity();
  structured_response_composes_without_ai();
  if (failures == 0)
  {
    std::printf("main_tests: PASS\n");
    return 0;
  }
  return 1;
}
