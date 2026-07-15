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
  clear_buffer(output, cerebras_v3::max_text);
  expect_true(
    resolve_relative_callback_time_from_date(
      "Tomorrow at 3.",
      2026,
      7,
      15,
      output,
      cerebras_v3::max_text),
    "bare business-hour callback resolves");
  expect_text(
    output,
    "Thursday, July 16, 2026 at 3 PM",
    "bare afternoon business hour becomes PM");
  clear_buffer(output, cerebras_v3::max_text);
  expect_true(
    resolve_relative_callback_time_from_date(
      "After tomorrow at 3.",
      2026,
      7,
      15,
      output,
      cerebras_v3::max_text),
    "after tomorrow callback resolves");
  expect_text(
    output,
    "Friday, July 17, 2026 at 3 PM",
    "after tomorrow becomes two days out");
  clear_buffer(output, cerebras_v3::max_text);
  expect_true(
    resolve_relative_callback_time_from_date(
      "The day after tomorrow around four.",
      2026,
      7,
      15,
      output,
      cerebras_v3::max_text),
    "day after tomorrow callback resolves");
  expect_text(
    output,
    "Friday, July 17, 2026 around 4 PM",
    "day after tomorrow keeps around wording");
  clear_buffer(output, cerebras_v3::max_text);
  expect_true(
    resolve_relative_callback_time_from_date(
      "Monday at 3.",
      2026,
      7,
      15,
      output,
      cerebras_v3::max_text),
    "weekday callback resolves");
  expect_text(
    output,
    "Monday, July 20, 2026 at 3 PM",
    "weekday becomes the next upcoming weekday");
  clear_buffer(output, cerebras_v3::max_text);
  expect_true(
    resolve_relative_callback_time_from_date(
      "Next Monday after 10.",
      2026,
      7,
      15,
      output,
      cerebras_v3::max_text),
    "next weekday callback resolves");
  expect_text(
    output,
    "Monday, July 20, 2026 after 10 AM",
    "next weekday uses next upcoming weekday with business hour inference");
  clear_buffer(output, cerebras_v3::max_text);
  expect_true(
    resolve_relative_callback_time_from_date(
      "In fourteen days at 4.",
      2026,
      7,
      15,
      output,
      cerebras_v3::max_text),
    "spoken teen day count resolves");
  expect_text(
    output,
    "Wednesday, July 29, 2026 at 4 PM",
    "fourteen days becomes exactly two weeks out");
  clear_buffer(output, cerebras_v3::max_text);
  expect_true(
    resolve_relative_callback_time_from_date(
      "In twenty one days at 4.",
      2026,
      7,
      15,
      output,
      cerebras_v3::max_text),
    "compound spoken day count resolves");
  expect_text(
    output,
    "Wednesday, August 05, 2026 at 4 PM",
    "twenty one days becomes three weeks out");
  clear_buffer(output, cerebras_v3::max_text);
  expect_true(
    resolve_relative_callback_time_from_date(
      "In thirteen weeks at 4.",
      2026,
      7,
      15,
      output,
      cerebras_v3::max_text),
    "spoken teen week count resolves");
  expect_text(
    output,
    "Wednesday, October 14, 2026 at 4 PM",
    "thirteen weeks becomes ninety one days out");
  expect_true(
    !resolve_relative_callback_time_from_date(
      "In fifty three weeks at 4.",
      2026,
      7,
      15,
      output,
      cerebras_v3::max_text),
    "spoken week count above supported range is rejected");
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

static void completed_intake_ends_retell_call(void)
{
  Config config;
  cerebras_v3::State state;
  Turn_result result;
  char response[2048];
  int field = cerebras_v3::field_department;
  load_config(0, &config);
  config.structured_responses = true;
  config.ai_response_slots = false;
  cerebras_v3::init_state(&state);
  state.department = cerebras_v3::department_service;
  while (field < cerebras_v3::field_none)
  {
    state.fields[field].status = cerebras_v3::status_captured;
    cerebras_v3::copy_text(state.fields[field].value, "captured", cerebras_v3::max_text);
    field += 1;
  }
  state.fields[cerebras_v3::field_callback_time].confirmed = true;
  state.fields[cerebras_v3::field_phone_confirmed].confirmed = true;
  state.fields[cerebras_v3::field_final_confirmed].confirmed = true;

  process_chat_turn(&state, &config, "Yes, that's correct.", "Do those details sound right?", "", &result);
  expect_true(result.end_call, "completed intake marks the Retell call for ending");

  build_retell_response_json(response, 2048, 12, "call-123", result.response_text, result.end_call);
  expect_true(
    std::strstr(response, "\"content_complete\":true,\"end_call\":true}") != 0,
    "completed intake sends Retell end_call true");
}

static void phone_confirmation_closes_without_final_reconfirmation(void)
{
  Config config;
  cerebras_v3::State state;
  Turn_result result;
  load_config(0, &config);
  config.structured_responses = true;
  config.ai_response_slots = false;
  cerebras_v3::init_state(&state);
  state.department = cerebras_v3::department_service;
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_department].value, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_intent].value, "service request", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_caller_name].value, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_last_name_spelling].value, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_vehicle].value, "2012 Acura MDX", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_request].value, "weird noise", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_date].value, "July 8, 2026", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_time].value, "3:00 PM", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_phone].value, "6472121234", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_department].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_intent].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_caller_name].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_vehicle].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_request].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_date].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_phone].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].confirmed = true;
  state.last_requested = cerebras_v3::field_phone_confirmed;

  process_chat_turn(&state, &config, "Yep.", "I have 6472121234. Is that the correct callback number?", "", &result);

  expect_true(result.end_call, "phone confirmation ends intake without final confirmation");
  expect_true(std::strchr(result.response_text, '?') == 0, "phone confirmation close asks no extra question");
  expect_true(
    std::strstr(result.response_text, "service team") != 0,
    "phone confirmation close names handoff team");
}

static void incomplete_phone_is_reasked_and_not_closed(void)
{
  Config config;
  cerebras_v3::State state;
  Turn_result result;
  load_config(0, &config);
  config.structured_responses = true;
  config.ai_response_slots = false;
  cerebras_v3::init_state(&state);
  state.department = cerebras_v3::department_sales;
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_department].value, "sales", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_intent].value, "inventory", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_caller_name].value, "Peter Russell", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_last_name_spelling].value, "R U S S E L L", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_request].value, "available inventory on the lot", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_date].value, "Wednesday, July 22, 2026 at 4 PM", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_time].value, "Wednesday, July 22, 2026 at 4 PM", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_department].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_intent].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_caller_name].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_request].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_date].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].confirmed = true;
  state.last_requested = cerebras_v3::field_phone;

  process_chat_turn(
    &state,
    &config,
    "Six four seven two nine two four three eight.",
    "Please say the full ten-digit callback number.",
    "",
    &result);

  expect_true(!result.end_call, "nine-digit phone does not close call");
  expect_true(
    state.fields[cerebras_v3::field_phone].status == cerebras_v3::status_missing,
    "nine-digit phone is not captured");
  expect_true(
    std::strstr(result.response_text, "ten-digit") != 0,
    "nine-digit phone asks for full ten-digit number");
}

static void rejected_phone_confirmation_forces_full_number_retry(void)
{
  Config config;
  cerebras_v3::State state;
  Turn_result result;
  load_config(0, &config);
  config.structured_responses = true;
  config.ai_response_slots = false;
  cerebras_v3::init_state(&state);
  state.department = cerebras_v3::department_sales;
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_department].value, "sales", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_intent].value, "inventory", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_caller_name].value, "Peter Russell", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_last_name_spelling].value, "R U S S E L L", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_request].value, "available inventory on the lot", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_date].value, "Wednesday, July 22, 2026 at 4 PM", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_time].value, "Wednesday, July 22, 2026 at 4 PM", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_phone].value, "6472924388", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_department].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_intent].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_caller_name].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_request].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_date].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_phone].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].confirmed = true;
  state.last_requested = cerebras_v3::field_phone_confirmed;

  process_chat_turn(
    &state,
    &config,
    "No.",
    "I have 6472924388. Is that the correct callback number?",
    "",
    &result);

  expect_true(!result.end_call, "rejected phone confirmation does not close call");
  expect_true(
    state.fields[cerebras_v3::field_phone].status == cerebras_v3::status_missing,
    "rejected phone clears callback number");
  expect_true(
    std::strstr(result.response_text, "repeat the full ten-digit") != 0,
    "rejected phone asks for full repeat");
}

static void phone_what_did_you_get_reasks_full_number(void)
{
  Config config;
  cerebras_v3::State state;
  Turn_result result;
  load_config(0, &config);
  config.structured_responses = true;
  config.ai_response_slots = false;
  cerebras_v3::init_state(&state);
  state.department = cerebras_v3::department_sales;
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_department].value, "sales", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_intent].value, "inventory", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_caller_name].value, "Peter Russell", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_last_name_spelling].value, "R U S S E L L", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_request].value, "available inventory on the lot", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_date].value, "Wednesday, July 22, 2026 at 4 PM", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_time].value, "Wednesday, July 22, 2026 at 4 PM", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_department].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_intent].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_caller_name].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_request].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_date].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].confirmed = true;
  state.last_requested = cerebras_v3::field_phone;

  process_chat_turn(
    &state,
    &config,
    "It's two nine two what did you got?",
    "Please say the full ten-digit callback number.",
    "",
    &result);

  expect_true(!result.end_call, "what-did-you-get phone question does not close call");
  expect_true(
    std::strstr(result.response_text, "repeat the full ten-digit") != 0,
    "what-did-you-get phone question asks for repeat");
}

static void relative_callback_readback_does_not_duplicate_dates(void)
{
  Config config;
  cerebras_v3::State state;
  Turn_result result;
  load_config(0, &config);
  config.structured_responses = true;
  config.ai_response_slots = false;
  cerebras_v3::init_state(&state);
  state.department = cerebras_v3::department_sales;
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_department].value, "sales", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_intent].value, "inventory", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_caller_name].value, "Peter Russell", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_last_name_spelling].value, "R U S S E L L", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_request].value, "available inventory on the lot", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_department].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_intent].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_caller_name].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_request].status = cerebras_v3::status_captured;
  state.last_requested = cerebras_v3::field_callback_date;

  process_chat_turn(
    &state,
    &config,
    "It's gonna be Wednesday after four PM.",
    "What date or day works best for a callback?",
    "",
    &result);

  expect_true(
    std::strstr(result.response_text, "Wednesday, July 22, 2026 at 4 PM") != 0,
    "relative callback readback uses resolved date once");
  expect_true(
    std::strstr(result.response_text, "July 15, 2026 Wednesday") == 0,
    "relative callback readback does not prepend current date");
}

static void completed_intake_reads_back_details_when_asked(void)
{
  Config config;
  cerebras_v3::State state;
  Turn_result result;
  int field = cerebras_v3::field_department;
  load_config(0, &config);
  config.structured_responses = true;
  config.ai_response_slots = false;
  cerebras_v3::init_state(&state);
  state.department = cerebras_v3::department_service;
  while (field < cerebras_v3::field_none)
  {
    state.fields[field].status = cerebras_v3::status_captured;
    cerebras_v3::copy_text(state.fields[field].value, "captured", cerebras_v3::max_text);
    field += 1;
  }
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_request].value, "weird noise", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_vehicle].value, "2012 Acura MDX", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_date].value, "July 8, 2026", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_time].value, "3:00 PM", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_phone].value, "6472121234", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_callback_time].confirmed = true;
  state.fields[cerebras_v3::field_phone_confirmed].confirmed = true;
  state.fields[cerebras_v3::field_final_confirmed].confirmed = true;

  process_chat_turn(&state, &config, "Well, what are they?", "Is everything correct?", "", &result);
  expect_true(result.end_call, "detail readback still marks the Retell call for ending");
  expect_true(std::strstr(result.response_text, "weird noise") != 0, "detail readback includes request");
  expect_true(std::strstr(result.response_text, "2012 Acura MDX") != 0, "detail readback includes vehicle");
  expect_true(std::strstr(result.response_text, "July 8, 2026 3:00 PM") != 0, "detail readback includes callback slot");
  expect_true(std::strstr(result.response_text, "6472121234") != 0, "detail readback includes callback number");
  expect_true(std::strchr(result.response_text, '?') == 0, "detail readback does not restart questioning");
}

static void retell_call_details_select_customer_number(void)
{
  char number[64];
  caller_number_from_retell_details(
    "{\"interaction_type\":\"call_details\",\"call\":{\"direction\":\"inbound\",\"from_number\":\"+16472121234\",\"to_number\":\"+14165550100\"}}",
    number,
    64);
  expect_text(number, "16472121234", "inbound call details select the caller number");

  caller_number_from_retell_details(
    "{\"interaction_type\":\"call_details\",\"call\":{\"direction\":\"outbound\",\"from_number\":\"+14165550100\",\"to_number\":\"+16472121234\"}}",
    number,
    64);
  expect_text(number, "16472121234", "outbound call details select the customer number");

  caller_number_from_retell_details(
    "{\"interaction_type\":\"call_details\",\"call\":{\"call_type\":\"web_call\"}}",
    number,
    64);
  expect_text(number, "", "web calls have no customer phone number");
}

static void calling_number_question_uses_metadata_or_requests_dictation(void)
{
  cerebras_v3::State state;
  Turn_result result;
  cerebras_v3::init_state(&state);
  state.last_requested = cerebras_v3::field_phone;

  expect_true(
    handle_calling_number_request(
      &state,
      "Could you check what number this is?",
      "16472121234",
      &result),
    "calling-number question is handled directly");
  expect_text(
    state.fields[cerebras_v3::field_phone].value,
    "16472121234",
    "Retell caller number becomes the proposed callback number");
  expect_true(
    state.last_requested == cerebras_v3::field_phone_confirmed,
    "metadata number advances to confirmation");
  expect_true(
    std::strstr(result.response_text, "number showing for this call") != 0,
    "phone call reads back the detected number");

  cerebras_v3::init_state(&state);
  state.last_requested = cerebras_v3::field_phone;
  expect_true(
    handle_calling_number_request(
      &state,
      "Can you check what number I'm calling from?",
      "",
      &result),
    "web-call number question is handled directly");
  expect_true(
    std::strstr(result.response_text, "can't see a phone number") != 0,
    "missing metadata asks the caller to dictate the number");
  expect_true(
    state.last_requested == cerebras_v3::field_phone,
    "missing metadata keeps the phone collection step active");
}

static void expect_corrected_faq(const char* message, const char* expected, const char* label)
{
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.faq_question, message, cerebras_v3::max_text);
  correct_faq_id_from_message(message, &interpretation);
  expect_text(interpretation.faq_id, expected, label);
}

static void faq_priority_handles_multi_question_turns(void)
{
  expect_corrected_faq(
    "Do you have loaners and what time does service close?",
    "service-loaner-vehicle",
    "loaner beats broad do-you-have parts alias");
  expect_corrected_faq(
    "What time do you close and do you have loaners?",
    "service-loaner-vehicle",
    "loaner still wins when hours comes first");
  expect_corrected_faq(
    "Do you have loaner vehicles and shuttle service?",
    "service-loaner-vehicle",
    "loaner and shuttle multi-question does not become parts");
  expect_corrected_faq(
    "Can you check part availability and what time do you close?",
    "service_hours",
    "hours beats generic parts availability in mixed FAQ");
  expect_corrected_faq(
    "Do you have brake pads in stock?",
    "parts_availability",
    "real part availability still routes to parts");
  expect_corrected_faq(
    "Do you have Ioniq inventory and financing options?",
    "financing",
    "financing priority still beats inventory");
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
  completed_intake_ends_retell_call();
  phone_confirmation_closes_without_final_reconfirmation();
  incomplete_phone_is_reasked_and_not_closed();
  rejected_phone_confirmation_forces_full_number_retry();
  phone_what_did_you_get_reasks_full_number();
  relative_callback_readback_does_not_duplicate_dates();
  completed_intake_reads_back_details_when_asked();
  retell_call_details_select_customer_number();
  calling_number_question_uses_metadata_or_requests_dictation();
  faq_priority_handles_multi_question_turns();
  if (failures == 0)
  {
    std::printf("main_tests: PASS\n");
    return 0;
  }
  return 1;
}
