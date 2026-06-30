#include <vector>
#include <sys/wait.h>

#define main retell_server_main
#include "../src/main.cpp"
#undef main

static int failures = 0;

static bool write_all_test(int fd, const unsigned char* data, int length)
{
  int offset = 0;
  while (offset < length)
  {
    const ssize_t count = write(
      fd,
      &data[offset],
      static_cast<unsigned long>(length - offset));
    if (count <= 0)
    {
      return false;
    }
    offset += static_cast<int>(count);
  }
  return true;
}

static void expect_true(bool actual, const char* message)
{
  if (!actual)
  {
    std::fprintf(stderr, "FAIL: %s\n", message);
    failures += 1;
  }
}

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

static void websocket_reader_accepts_payload(int payload_length, bool use_64_bit_length)
{
  int sockets[2];
  const int header_length = use_64_bit_length ? 14 : 8;
  std::vector<unsigned char> frame(
    static_cast<std::size_t>(header_length + payload_length));
  std::vector<char> output(static_cast<std::size_t>(websocket_capacity));
  const unsigned char mask[4] = {17U, 34U, 51U, 68U};
  int payload_offset = 0;
  int index = 0;
  int opcode = 0;
  int encoded_length = payload_length;
  pid_t writer = -1;
  int writer_status = 0;
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
  {
    expect_true(false, "large frame test creates socket pair");
    return;
  }
  frame[0] = 129U;
  if (use_64_bit_length)
  {
    frame[1] = 255U;
    index = 0;
    while (index < 8)
    {
      frame[static_cast<std::size_t>(9 - index)] =
        static_cast<unsigned char>(encoded_length & 255);
      encoded_length = encoded_length / 256;
      index += 1;
    }
    payload_offset = 10;
  }
  else
  {
    frame[1] = 254U;
    frame[2] = static_cast<unsigned char>((payload_length >> 8) & 255);
    frame[3] = static_cast<unsigned char>(payload_length & 255);
    payload_offset = 4;
  }
  index = 0;
  while (index < 4)
  {
    frame[static_cast<std::size_t>(payload_offset + index)] = mask[index];
    index += 1;
  }
  payload_offset += 4;
  index = 0;
  while (index < payload_length)
  {
    const unsigned char value = static_cast<unsigned char>('a' + (index % 26));
    frame[static_cast<std::size_t>(payload_offset + index)] =
      static_cast<unsigned char>(value ^ mask[index % 4]);
    index += 1;
  }
  writer = fork();
  if (writer == 0)
  {
    const bool wrote = write_all_test(
      sockets[0],
      &frame[0],
      header_length + payload_length);
    close(sockets[0]);
    close(sockets[1]);
    _exit(wrote ? 0 : 1);
  }
  if (writer < 0)
  {
    expect_true(false, "large frame test starts writer process");
    close(sockets[0]);
    close(sockets[1]);
    return;
  }
  close(sockets[0]);
  expect_true(
    websocket_read_text(sockets[1], &output[0], websocket_capacity, &opcode),
    "large WebSocket frame is accepted");
  expect_true(opcode == 1, "large WebSocket frame preserves text opcode");
  expect_true(output[0] == 'a', "large WebSocket frame unmasks first byte");
  expect_true(
    output[static_cast<std::size_t>(payload_length - 1)] ==
      static_cast<char>('a' + ((payload_length - 1) % 26)),
    "large WebSocket frame unmasks final byte");
  expect_true(
    output[static_cast<std::size_t>(payload_length)] == '\0',
    "large WebSocket frame is null terminated");
  close(sockets[1]);
  expect_true(
    waitpid(writer, &writer_status, 0) == writer,
    "large frame test waits for writer process");
  expect_true(
    WIFEXITED(writer_status) && (WEXITSTATUS(writer_status) == 0),
    "large WebSocket frame writes completely");
}

static void websocket_reader_accepts_large_transcript_frames(void)
{
  websocket_reader_accepts_payload(12000, false);
  websocket_reader_accepts_payload(70000, true);
}

static void relative_callback_time_resolves_to_concrete_date(void)
{
  char output[cerebras_v3::max_text];
  clear_buffer(output, cerebras_v3::max_text);
  expect_true(
    resolve_relative_callback_time_from_date(
      "Two weeks from now at 3 PM.",
      2026,
      6,
      30,
      output,
      cerebras_v3::max_text),
    "two weeks from now resolves");
  expect_text(
    output,
    "Tuesday, July 14, 2026 at 3 PM",
    "relative callback becomes concrete day and time");
  clear_buffer(output, cerebras_v3::max_text);
  expect_true(
    resolve_relative_callback_time_from_date(
      "How about three PM in two weeks today?",
      2026,
      6,
      30,
      output,
      cerebras_v3::max_text),
    "ASR reordered relative time resolves");
  expect_text(
    output,
    "Tuesday, July 14, 2026 at 3 PM",
    "ASR word-based hour becomes a validated numeric time");
  expect_true(
    !resolve_relative_callback_time_from_date(
      "Next week at 3 PM",
      2026,
      6,
      30,
      output,
      cerebras_v3::max_text),
    "next week without an exact offset remains vague");
}

static void relative_callback_fallback_drives_confirmation(void)
{
  Config config;
  cerebras_v3::State state;
  Turn_result result;
  load_config(0, &config);
  config.structured_responses = true;
  config.ai_response_slots = false;
  cerebras_v3::init_state(&state);
  state.department = cerebras_v3::department_service;
  cerebras_v3::copy_text(
    state.fields[cerebras_v3::field_department].value,
    "service",
    cerebras_v3::max_text);
  state.fields[cerebras_v3::field_department].status = cerebras_v3::status_captured;
  cerebras_v3::copy_text(
    state.fields[cerebras_v3::field_intent].value,
    "vehicle noise",
    cerebras_v3::max_text);
  state.fields[cerebras_v3::field_intent].status = cerebras_v3::status_captured;
  cerebras_v3::copy_text(
    state.fields[cerebras_v3::field_caller_name].value,
    "Jamal Maxberg",
    cerebras_v3::max_text);
  state.fields[cerebras_v3::field_caller_name].status = cerebras_v3::status_captured;
  cerebras_v3::copy_text(
    state.fields[cerebras_v3::field_last_name_spelling].value,
    "M A X B E R G",
    cerebras_v3::max_text);
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  cerebras_v3::copy_text(
    state.fields[cerebras_v3::field_vehicle].value,
    "2020 Acura MDX",
    cerebras_v3::max_text);
  state.fields[cerebras_v3::field_vehicle].status = cerebras_v3::status_captured;
  cerebras_v3::copy_text(
    state.fields[cerebras_v3::field_request].value,
    "car making weird noise",
    cerebras_v3::max_text);
  state.fields[cerebras_v3::field_request].status = cerebras_v3::status_captured;
  state.last_requested = cerebras_v3::field_callback_time;
  process_chat_turn(
    &state,
    &config,
    "How about two weeks from now at three PM?",
    "What day and time works for a callback?",
    "",
    &result);
  expect_true(
    state.fields[cerebras_v3::field_callback_time].status ==
      cerebras_v3::status_captured,
    "relative callback fallback captures callback time");
  expect_true(
    std::strstr(
      state.fields[cerebras_v3::field_callback_time].value,
      "at 3 PM") != 0,
    "relative callback fallback stores concrete time");
  expect_true(
    std::strstr(result.response_text, "at 3 PM") != 0,
    "relative callback fallback reads back concrete time");
  expect_true(
    std::strchr(result.response_text, '?') != 0,
    "relative callback fallback asks for confirmation");
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
  websocket_reader_accepts_large_transcript_frames();
  relative_callback_time_resolves_to_concrete_date();
  relative_callback_fallback_drives_confirmation();
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
