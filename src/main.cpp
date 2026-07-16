#include <planner.h>
#include <caller_number.h>
#include <cerebras_client.h>
#include <conversation_integrity.h>
#include <employee_delivery.h>
#include <generated_kb.h>
#include <prompt_sections.h>
#include <relative_callback_time.h>
#include <response_ai.h>
#include <response_renderer.h>
#include <server_runtime.h>
#include <websocket_protocol.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>

static bool json_value(const char* json, const char* key, char* output, int capacity);

void clear_buffer(char* buffer, int capacity)
{
  if ((buffer != 0) && (capacity > 0))
  {
    buffer[0] = '\0';
  }
}

static int append_text(char* output, int capacity, const char* text)
{
  int length = 0;
  int index = 0;
  if ((output == 0) || (text == 0) || (capacity <= 0))
  {
    return 0;
  }
  length = static_cast<int>(std::strlen(output));
  while ((text[index] != '\0') && (length < (capacity - 1)))
  {
    output[length] = text[index];
    length += 1;
    index += 1;
  }
  output[length] = '\0';
  return length;
}

static void current_date_text(char* output, int capacity)
{
  std::time_t now = 0;
  std::tm local_time;
  std::tm* local_time_ptr = 0;
  if ((output == 0) || (capacity <= 0))
  {
    return;
  }
  output[0] = '\0';
  now = std::time(0);
  local_time_ptr = std::localtime(&now);
  if (local_time_ptr == 0)
  {
    return;
  }
  local_time = *local_time_ptr;
  (void)std::strftime(output, static_cast<unsigned long>(capacity), "%A, %B %d, %Y", &local_time);
}

static void lowercase_callback_text(char* output, const char* input, int capacity)
{
  int index = 0;
  if ((output == 0) || (capacity <= 0))
  {
    return;
  }
  output[0] = '\0';
  if (input == 0)
  {
    return;
  }
  while ((input[index] != '\0') && (index < (capacity - 1)))
  {
    output[index] = static_cast<char>(
      std::tolower(static_cast<unsigned char>(input[index])));
    index += 1;
  }
  output[index] = '\0';
}

static void apply_relative_callback_time_fallback(
  const cerebras_v3::State* state,
  const char* message,
  cerebras_v3::Interpretation* interpretation)
{
  char resolved[cerebras_v3::max_text];
  clear_buffer(resolved, cerebras_v3::max_text);
  if ((state == 0) || (interpretation == 0) ||
      ((state->last_requested != cerebras_v3::field_callback_date) &&
       (state->last_requested != cerebras_v3::field_callback_time)))
  {
    return;
  }
  if (!cerebras_v3::resolve_relative_callback_time(
        message,
        resolved,
        cerebras_v3::max_text))
  {
    return;
  }
  cerebras_v3::copy_text(
    interpretation->callback_date,
    resolved,
    cerebras_v3::max_text);
  cerebras_v3::copy_text(
    interpretation->callback_time,
    resolved,
    cerebras_v3::max_text);
  cerebras_v3::copy_text(
    interpretation->answered_field,
    "callback_time",
    64);
  cerebras_v3::copy_text(interpretation->turn_type, "field_answer", 64);
}

static bool starts_with(const char* text, const char* prefix)
{
  bool result = false;
  if ((text != 0) && (prefix != 0))
  {
    result = (std::strncmp(text, prefix, std::strlen(prefix)) == 0);
  }
  return result;
}

static bool contains_text(const char* text, const char* pattern)
{
  bool result = false;
  if ((text != 0) && (pattern != 0))
  {
    result = (std::strstr(text, pattern) != 0);
  }
  return result;
}

static void lowercase_text(char* output, const char* input, int capacity)
{
  int index = 0;
  if ((output == 0) || (capacity <= 0))
  {
    return;
  }
  if (input == 0)
  {
    output[0] = '\0';
    return;
  }
  while ((input[index] != '\0') && (index < (capacity - 1)))
  {
    output[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(input[index])));
    index += 1;
  }
  output[index] = '\0';
}

static void json_escape_append(char* output, int capacity, const char* text)
{
  int index = 0;
  char pair[3];
  pair[2] = '\0';
  if ((output == 0) || (text == 0))
  {
    return;
  }
  while (text[index] != '\0')
  {
    if ((text[index] == '"') || (text[index] == '\\'))
    {
      pair[0] = '\\';
      pair[1] = text[index];
      append_text(output, capacity, pair);
    }
    else if (text[index] == '\n')
    {
      append_text(output, capacity, "\\n");
    }
    else
    {
      pair[0] = text[index];
      pair[1] = '\0';
      append_text(output, capacity, pair);
    }
    index += 1;
  }
}

static const char* json_bool(bool value)
{
  return value ? "true" : "false";
}

static void append_json_string_field(char* output, int capacity, const char* name, const char* value)
{
  append_text(output, capacity, ",\"");
  append_text(output, capacity, name);
  append_text(output, capacity, "\":\"");
  json_escape_append(output, capacity, (value != 0) ? value : "");
  append_text(output, capacity, "\"");
}

static void append_json_bool_field(char* output, int capacity, const char* name, bool value)
{
  append_text(output, capacity, ",\"");
  append_text(output, capacity, name);
  append_text(output, capacity, "\":");
  append_text(output, capacity, json_bool(value));
}

static void append_json_int_field(char* output, int capacity, const char* name, long value)
{
  char number[32];
  clear_buffer(number, 32);
  snprintf(number, sizeof(number), "%ld", value);
  append_text(output, capacity, ",\"");
  append_text(output, capacity, name);
  append_text(output, capacity, "\":");
  append_text(output, capacity, number);
}

static void log_json_line(const char* event, const char* call_id, const char* extra)
{
  char line[2048];
  clear_buffer(line, 2048);
  append_text(line, 2048, "{\"event\":\"");
  json_escape_append(line, 2048, (event != 0) ? event : "unknown");
  append_text(line, 2048, "\"");
  append_json_string_field(line, 2048, "call_id", (call_id != 0) ? call_id : "");
  if ((extra != 0) && (extra[0] != '\0'))
  {
    append_text(line, 2048, extra);
  }
  append_text(line, 2048, "}");
  std::fprintf(stderr, "%s\n", line);
}

static void copy_limited(char* destination, const char* source, int capacity, int max_chars)
{
  int index = 0;
  if ((destination == 0) || (capacity <= 0))
  {
    return;
  }
  if (source == 0)
  {
    destination[0] = '\0';
    return;
  }
  while ((source[index] != '\0') && (index < (capacity - 1)) && (index < max_chars))
  {
    destination[index] = source[index];
    index += 1;
  }
  destination[index] = '\0';
}

static void append_limited(char* output, int capacity, const char* text, int max_chars)
{
  char limited[text_capacity];
  clear_buffer(limited, text_capacity);
  copy_limited(limited, text, text_capacity, max_chars);
  append_text(output, capacity, limited);
}

static void replace_all_text(char* text, int capacity, const char* pattern, const char* replacement)
{
  char original[text_capacity];
  int input = 0;
  int output = 0;
  const int pattern_length = static_cast<int>(std::strlen(pattern));
  if ((text == 0) || (pattern == 0) || (replacement == 0) || (capacity <= 0) || (pattern_length <= 0))
  {
    return;
  }
  copy_limited(original, text, text_capacity, text_capacity - 1);
  clear_buffer(text, capacity);
  while ((original[input] != '\0') && (output < (capacity - 1)))
  {
    if (std::strncmp(&original[input], pattern, static_cast<unsigned long>(pattern_length)) == 0)
    {
      int replacement_index = 0;
      while ((replacement[replacement_index] != '\0') && (output < (capacity - 1)))
      {
        text[output] = replacement[replacement_index];
        output += 1;
        replacement_index += 1;
      }
      input += pattern_length;
    }
    else
    {
      text[output] = original[input];
      output += 1;
      input += 1;
    }
  }
  text[output] = '\0';
}

static void normalize_response_spaces(char* text, int capacity)
{
  char original[text_capacity];
  int input = 0;
  int output = 0;
  bool last_space = false;
  if ((text == 0) || (capacity <= 0))
  {
    return;
  }
  copy_limited(original, text, text_capacity, text_capacity - 1);
  clear_buffer(text, capacity);
  while ((original[input] == ' ') || (original[input] == '\t'))
  {
    input += 1;
  }
  while ((original[input] != '\0') && (output < (capacity - 1)))
  {
    const char current = original[input];
    if ((current == ' ') || (current == '\t'))
    {
      if (!last_space)
      {
        text[output] = ' ';
        output += 1;
        last_space = true;
      }
    }
    else
    {
      if (((current == ',') || (current == '?') || (current == '.')) &&
          (output > 0) &&
          (text[output - 1] == ' '))
      {
        output -= 1;
      }
      text[output] = current;
      output += 1;
      last_space = false;
    }
    input += 1;
  }
  while ((output > 0) && (text[output - 1] == ' '))
  {
    output -= 1;
  }
  text[output] = '\0';
}

static void sanitize_response_text(char* text, int capacity)
{
  if ((text == 0) || (capacity <= 0))
  {
    return;
  }
  replace_all_text(text, capacity, " for our records", "");
  replace_all_text(text, capacity, " so I can assist you better", "");
  replace_all_text(text, capacity, " so I can assist you further", "");
  replace_all_text(text, capacity, "Mr. ", "");
  replace_all_text(text, capacity, "Mr ", "");
  replace_all_text(text, capacity, "Ms. ", "");
  replace_all_text(text, capacity, "Ms ", "");
  replace_all_text(text, capacity, "Mrs. ", "");
  replace_all_text(text, capacity, "Mrs ", "");
  replace_all_text(text, capacity, "service appointment", "service request");
  replace_all_text(text, capacity, "Service appointment", "Service request");
  normalize_response_spaces(text, capacity);
}

static const char* find_env(char** envp, const char* name)
{
  int index = 0;
  const int length = static_cast<int>(std::strlen(name));
  if ((envp == 0) || (name == 0))
  {
    return 0;
  }
  while (envp[index] != 0)
  {
    if ((std::strncmp(envp[index], name, static_cast<unsigned long>(length)) == 0) &&
        (envp[index][length] == '='))
    {
      return &envp[index][length + 1];
    }
    index += 1;
  }
  return 0;
}

static int parse_int(const char* text, int fallback)
{
  int value = 0;
  int index = 0;
  bool any = false;
  if (text == 0)
  {
    return fallback;
  }
  while ((text[index] >= '0') && (text[index] <= '9'))
  {
    value = (value * 10) + (text[index] - '0');
    any = true;
    index += 1;
  }
  return any ? value : fallback;
}

static bool env_enabled(const char* value)
{
  return
    (value != 0) &&
    ((std::strcmp(value, "1") == 0) ||
     (std::strcmp(value, "true") == 0) ||
     (std::strcmp(value, "on") == 0));
}

void load_config(char** envp, Config* config)
{
  const char* value = 0;
  if (config == 0)
  {
    return;
  }
  config->port = default_port;
  clear_buffer(config->shared_secret, 128);
  clear_buffer(config->cerebras_key, 256);
  clear_buffer(config->delivery_webhook_url, 256);
  clear_buffer(config->delivery_webhook_secret, 128);
  config->cerebras_debug = false;
  config->structured_responses = false;
  config->ai_response_slots = false;
  cerebras_v3::copy_text(config->cerebras_model, "gpt-oss-120b", 128);
  cerebras_v3::copy_text(config->cerebras_url, "https://api.cerebras.ai/v1/chat/completions", 256);
  value = find_env(envp, "PORT");
  if (value != 0) { config->port = parse_int(value, default_port); }
  value = find_env(envp, "RETELL_SHARED_SECRET");
  if (value != 0) { cerebras_v3::copy_text(config->shared_secret, value, 128); }
  value = find_env(envp, "CEREBRAS_API_KEY");
  if (value != 0) { cerebras_v3::copy_text(config->cerebras_key, value, 256); }
  value = find_env(envp, "CEREBRAS_MODEL");
  if (value != 0) { cerebras_v3::copy_text(config->cerebras_model, value, 128); }
  value = find_env(envp, "CEREBRAS_BASE_URL");
  if (value != 0) { cerebras_v3::copy_text(config->cerebras_url, value, 256); }
  value = find_env(envp, "EMPLOYEE_DELIVERY_WEBHOOK_URL");
  if (value != 0) { cerebras_v3::copy_text(config->delivery_webhook_url, value, 256); }
  value = find_env(envp, "EMPLOYEE_DELIVERY_WEBHOOK_SECRET");
  if (value != 0) { cerebras_v3::copy_text(config->delivery_webhook_secret, value, 128); }
  value = find_env(envp, "CEREBRAS_DEBUG");
  if (value != 0) { config->cerebras_debug = env_enabled(value); }
  value = find_env(envp, "STRUCTURED_RESPONSES_ENABLED");
  if (value != 0) { config->structured_responses = env_enabled(value); }
  value = find_env(envp, "AI_RESPONSE_SLOTS_ENABLED");
  if (value != 0) { config->ai_response_slots = env_enabled(value); }
}

static bool interpret_with_cerebras(
  const Config* config,
  const char* state_json,
  const char* recent_context,
  const char* last_assistant,
  const char* caller,
  cerebras_v3::Interpretation* interpretation)
{
  char system[cerebras_system_capacity];
  char user[2048];
  char content[text_capacity];
  char today[96];
  clear_buffer(system, cerebras_system_capacity);
  clear_buffer(today, 96);
  append_text(system, cerebras_system_capacity, cerebras_v3::prompt_sections::interpreter_role);
  append_text(system, cerebras_system_capacity, " ");
  current_date_text(today, 96);
  if (today[0] != '\0')
  {
    append_text(system, cerebras_system_capacity, "Today is ");
    append_text(system, cerebras_system_capacity, today);
    append_text(system, cerebras_system_capacity, ". Resolve relative callback dates like tomorrow or two weeks from now into a concrete month, day, year, and time when the caller gave enough information. ");
  }
  append_text(system, cerebras_system_capacity, cerebras_v3::prompt_sections::interpreter_schema);
  append_text(system, cerebras_system_capacity, " ");
  append_text(system, cerebras_system_capacity, cerebras_v3::prompt_sections::interpreter_field_rules);
  append_text(system, cerebras_system_capacity, " ");
  append_text(system, cerebras_system_capacity, cerebras_v3::generated_kb::interpreter_faq_rules);
  append_text(system, cerebras_system_capacity, " ");
  append_text(system, cerebras_system_capacity, cerebras_v3::generated_kb::interpreter_affirmation_rules);
  append_text(system, cerebras_system_capacity, " ");
  append_text(system, cerebras_system_capacity, cerebras_v3::prompt_sections::interpreter_output_rules);
  clear_buffer(user, 2048);
  append_text(user, 2048, "Known state: ");
  append_text(user, 2048, state_json);
  append_text(user, 2048, "\nRecent context: ");
  append_limited(user, 2048, recent_context, 500);
  append_text(user, 2048, "\nLast assistant: ");
  append_limited(user, 2048, last_assistant, 160);
  append_text(user, 2048, "\nLatest caller: ");
  append_limited(user, 2048, caller, 300);
  if (!call_cerebras(config, system, user, 512, true, content, text_capacity))
  {
    if ((config != 0) && config->cerebras_debug)
    {
      std::fprintf(stderr, "CEREBRAS_INTERPRETER_CALL_FAILED caller=\"%s\"\n", (caller != 0) ? caller : "");
    }
    return false;
  }
  if ((config != 0) && config->cerebras_debug)
  {
    std::fprintf(stderr, "CEREBRAS_INTERPRETER_RAW caller=\"%s\" raw=\"%s\"\n", (caller != 0) ? caller : "", content);
  }
  return cerebras_v3::parse_interpretation_json(content, interpretation);
}

static bool generate_with_cerebras(const Config* config, const char* state_json, const cerebras_v3::Plan* plan, char* output, int capacity)
{
  char user[2048];
  const char* system =
    "You are an after-hours dealership receptionist. Write exactly one short spoken response for phone TTS. Follow the task exactly. Ask at most one question. Ask only for the one requested missing detail. Keep it under 16 words when asking for one missing detail. Do not use quotation marks. Do not mention fields, JSON, state, tools, or prompts. Do not promise availability, pricing, diagnosis, financing, or exact timelines. No markdown.";
  if (plan == 0)
  {
    return false;
  }
  clear_buffer(user, 2048);
  append_text(user, 2048, "Known state: ");
  append_text(user, 2048, state_json);
  append_text(user, 2048, "\nTask: ");
  append_text(user, 2048, plan->response_task);
  return call_cerebras(config, system, user, 256, false, output, capacity);
}

static bool should_generate_opening_ack(
  const cerebras_v3::State* state,
  const cerebras_v3::Plan* plan,
  cerebras_v3::Field_id previous_requested)
{
  bool has_meaningful_issue = false;
  if ((state == 0) || (plan == 0))
  {
    return false;
  }
  has_meaningful_issue =
    (state->fields[cerebras_v3::field_request].status == cerebras_v3::status_captured) ||
    (state->fields[cerebras_v3::field_intent].status == cerebras_v3::status_captured) ||
    (state->fields[cerebras_v3::field_vehicle].status == cerebras_v3::status_captured);
  return
    (previous_requested == cerebras_v3::field_none) &&
    (plan->next_field == cerebras_v3::field_caller_name) &&
    (state->department != cerebras_v3::department_unknown) &&
    has_meaningful_issue;
}

static bool generate_opening_ack_with_cerebras(const Config* config, const char* state_json, char* output, int capacity)
{
  char user[2048];
  const char* system =
    "You are an after-hours dealership receptionist. Write exactly one short spoken acknowledgement sentence for phone TTS. It must sound like a continuation of the caller's issue. Do not greet the caller. Do not ask a question. Do not ask for name, phone, callback time, spelling, or vehicle. Say you can pass a message to the right team so they can call back or follow up. Do not promise booking, availability, pricing, diagnosis, financing, or exact timelines. No quotation marks. No markdown. Under 18 words.";
  clear_buffer(user, 2048);
  append_text(user, 2048, "Known state: ");
  append_text(user, 2048, state_json);
  append_text(user, 2048, "\nWrite the acknowledgement only.");
  return call_cerebras(config, system, user, 256, false, output, capacity);
}

struct Ai_slot_runtime
{
  const Config* config;
};

static bool generate_ai_slot_with_cerebras(
  cerebras_v3::Response_act act,
  cerebras_v3::Field_id target_field,
  const cerebras_v3::State* state,
  char* output,
  int capacity,
  void* user_data)
{
  Ai_slot_runtime* runtime = static_cast<Ai_slot_runtime*>(user_data);
  char state_json[2048];
  char system[2048];
  char user[4096];
  if ((runtime == 0) || (runtime->config == 0) || (state == 0))
  {
    return false;
  }
  clear_buffer(state_json, 2048);
  clear_buffer(system, 2048);
  clear_buffer(user, 4096);
  cerebras_v3::state_to_json(state, state_json, 2048);
  if (!cerebras_v3::build_ai_slot_prompts(
        act,
        target_field,
        state_json,
        system,
        2048,
        user,
        4096))
  {
    return false;
  }
  return call_cerebras(runtime->config, system, user, 128, false, output, capacity);
}

static bool is_department_only_reply(const char* lowered)
{
  char letters[32];
  int input = 0;
  int output = 0;
  clear_buffer(letters, 32);
  if (lowered == 0)
  {
    return false;
  }
  while ((lowered[input] != '\0') && (output < 31))
  {
    if (std::isalpha(static_cast<unsigned char>(lowered[input])))
    {
      letters[output] = lowered[input];
      output += 1;
    }
    input += 1;
  }
  letters[output] = '\0';
  return
    (std::strcmp(letters, "service") == 0) ||
    (std::strcmp(letters, "parts") == 0) ||
    (std::strcmp(letters, "sales") == 0);
}

static bool copy_name_reply(const char* message, char* output, int capacity)
{
  char cleaned[cerebras_v3::max_text];
  int input = 0;
  int out = 0;
  int word_count = 0;
  int word_length = 0;
  bool in_word = false;
  bool any_letter = false;
  clear_buffer(cleaned, cerebras_v3::max_text);
  if ((message == 0) || (output == 0) || (capacity <= 0))
  {
    return false;
  }
  while ((message[input] != '\0') && (out < (cerebras_v3::max_text - 1)))
  {
    const unsigned char c = static_cast<unsigned char>(message[input]);
    if ((std::isalpha(c) != 0) || (std::isspace(c) != 0) || (message[input] == '\'') || (message[input] == '-'))
    {
      cleaned[out] = message[input];
      out += 1;
    }
    input += 1;
  }
  while ((out > 0) && std::isspace(static_cast<unsigned char>(cleaned[out - 1])) != 0)
  {
    out -= 1;
  }
  cleaned[out] = '\0';
  input = 0;
  while (cleaned[input] != '\0')
  {
    const unsigned char c = static_cast<unsigned char>(cleaned[input]);
    if (std::isalpha(c) != 0)
    {
      any_letter = true;
      in_word = true;
      word_length += 1;
    }
    else if (std::isspace(c) != 0)
    {
      if (in_word)
      {
        if (word_length < 2) { return false; }
        word_count += 1;
      }
      in_word = false;
      word_length = 0;
    }
    input += 1;
  }
  if (in_word)
  {
    if (word_length < 2) { return false; }
    word_count += 1;
  }
  if (!any_letter || (word_count < 2) || (word_count > 4))
  {
    return false;
  }
  cerebras_v3::copy_text(output, cleaned, capacity);
  return true;
}

static bool copy_spelling_reply(const char* message, char* output, int capacity)
{
  int input = 0;
  int out = 0;
  clear_buffer(output, capacity);
  if ((message == 0) || (output == 0) || (capacity <= 0))
  {
    return false;
  }
  while ((message[input] != '\0') && (out < (capacity - 1)))
  {
    const unsigned char c = static_cast<unsigned char>(message[input]);
    if (std::isalpha(c) != 0)
    {
      output[out] = static_cast<char>(std::toupper(c));
      out += 1;
    }
    input += 1;
  }
  output[out] = '\0';
  return out >= 2;
}

static bool copy_phone_reply(const char* message, char* output, int capacity)
{
  int input = 0;
  int out = 0;
  clear_buffer(output, capacity);
  if ((message == 0) || (output == 0) || (capacity <= 0))
  {
    return false;
  }
  while ((message[input] != '\0') && (out < (capacity - 1)))
  {
    const unsigned char c = static_cast<unsigned char>(message[input]);
    if (std::isdigit(c) != 0)
    {
      output[out] = message[input];
      out += 1;
    }
    input += 1;
  }
  output[out] = '\0';
  return out >= 7;
}

static bool caller_text_looks_like_correction(const char* lowered)
{
  if (lowered == 0)
  {
    return false;
  }
  return
    contains_text(lowered, "actually") ||
    contains_text(lowered, "sorry") ||
    contains_text(lowered, "i meant") ||
    contains_text(lowered, "instead") ||
    contains_text(lowered, "change that") ||
    contains_text(lowered, "make that") ||
    contains_text(lowered, "not ") ||
    contains_text(lowered, "no,") ||
    contains_text(lowered, "wrong");
}

static void apply_local_correction_type(
  const cerebras_v3::State* state,
  const char* message,
  const char* lowered,
  cerebras_v3::Interpretation* interpretation)
{
  if ((state == 0) ||
      (interpretation == 0) ||
      !caller_text_looks_like_correction(lowered) ||
      (std::strcmp(interpretation->turn_type, "caller_question") == 0) ||
      (std::strcmp(interpretation->turn_type, "customer_confusion") == 0) ||
      (std::strcmp(interpretation->turn_type, "off_topic") == 0) ||
      (std::strcmp(interpretation->turn_type, "unclear_audio") == 0))
  {
    return;
  }
  if (std::strcmp(interpretation->turn_type, "correction") != 0)
  {
    cerebras_v3::copy_text(interpretation->turn_type, "correction", 64);
  }
  if (interpretation->answered_field[0] == '\0')
  {
    if ((state->last_requested == cerebras_v3::field_last_name_spelling) ||
        contains_text(lowered, "spell") ||
        contains_text(lowered, "last name"))
    {
      cerebras_v3::copy_text(interpretation->answered_field, "last_name_spelling", 64);
      if (interpretation->spelling[0] == '\0')
      {
        copy_spelling_reply(message, interpretation->spelling, cerebras_v3::max_text);
      }
    }
    else if ((state->last_requested == cerebras_v3::field_phone) ||
             (state->last_requested == cerebras_v3::field_phone_confirmed) ||
             contains_text(lowered, "number") ||
             (copy_phone_reply(message, interpretation->phone, cerebras_v3::max_text) &&
              (interpretation->phone[0] != '\0')))
    {
      cerebras_v3::copy_text(interpretation->answered_field, "phone", 64);
    }
    else if ((state->last_requested == cerebras_v3::field_callback_date) ||
             (state->last_requested == cerebras_v3::field_callback_time) ||
             contains_text(lowered, "tomorrow") ||
             contains_text(lowered, "monday") ||
             contains_text(lowered, "tuesday") ||
             contains_text(lowered, "wednesday") ||
             contains_text(lowered, "thursday") ||
             contains_text(lowered, "friday") ||
             contains_text(lowered, " at "))
    {
      cerebras_v3::copy_text(interpretation->answered_field, "callback_time", 64);
    }
    else if (state->last_requested == cerebras_v3::field_vehicle)
    {
      cerebras_v3::copy_text(interpretation->answered_field, "vehicle", 64);
    }
    else if (state->last_requested == cerebras_v3::field_request)
    {
      cerebras_v3::copy_text(interpretation->answered_field, "request", 64);
    }
  }
}

static void apply_local_interpretation_fallback(
  const cerebras_v3::State* state,
  const char* message,
  cerebras_v3::Interpretation* interpretation)
{
  char lowered[text_capacity];
  if ((message == 0) || (interpretation == 0))
  {
    return;
  }
  lowercase_text(lowered, message, text_capacity);
  apply_local_correction_type(state, message, lowered, interpretation);
  if ((state != 0) &&
      (std::strcmp(interpretation->turn_type, "customer_confusion") != 0) &&
      (std::strcmp(interpretation->turn_type, "caller_question") != 0) &&
      (std::strcmp(interpretation->turn_type, "off_topic") != 0) &&
      (std::strcmp(interpretation->turn_type, "unclear_audio") != 0))
  {
    if ((state->last_requested == cerebras_v3::field_caller_name) && (interpretation->name[0] == '\0'))
    {
      copy_name_reply(message, interpretation->name, cerebras_v3::max_text);
    }
    else if ((state->last_requested == cerebras_v3::field_last_name_spelling) && (interpretation->spelling[0] == '\0'))
    {
      copy_spelling_reply(message, interpretation->spelling, cerebras_v3::max_text);
    }
    else if ((state->last_requested == cerebras_v3::field_phone) && (interpretation->phone[0] == '\0'))
    {
      copy_phone_reply(message, interpretation->phone, cerebras_v3::max_text);
    }
  }
  if (interpretation->department[0] == '\0')
  {
    if (contains_text(lowered, "service") ||
        contains_text(lowered, "repair") ||
        contains_text(lowered, "maintenance") ||
        contains_text(lowered, "diagnostic") ||
        contains_text(lowered, "recall") ||
        contains_text(lowered, "warranty") ||
        contains_text(lowered, "noise") ||
        contains_text(lowered, "warning") ||
        contains_text(lowered, "check engine") ||
        contains_text(lowered, "my car") ||
        contains_text(lowered, "my vehicle"))
    {
      cerebras_v3::copy_text(interpretation->department, "service", 32);
    }
    else if (contains_text(lowered, "parts") ||
             contains_text(lowered, "part ") ||
             contains_text(lowered, "accessory") ||
             contains_text(lowered, "wiper") ||
             contains_text(lowered, "battery") ||
             contains_text(lowered, "cargo mat") ||
             contains_text(lowered, "key fob"))
    {
      cerebras_v3::copy_text(interpretation->department, "parts", 32);
    }
    else if (contains_text(lowered, "sales") ||
             contains_text(lowered, "buy") ||
             contains_text(lowered, "lease") ||
             contains_text(lowered, "test drive") ||
             contains_text(lowered, "trade in") ||
             contains_text(lowered, "trade-in") ||
             contains_text(lowered, "inventory") ||
             contains_text(lowered, "looking for a car") ||
             contains_text(lowered, "looking for car") ||
             contains_text(lowered, "looking for a vehicle") ||
             contains_text(lowered, "looking for vehicle"))
    {
      cerebras_v3::copy_text(interpretation->department, "sales", 32);
    }
  }
  if ((interpretation->intent[0] == '\0') && (interpretation->department[0] != '\0'))
  {
    if (contains_text(lowered, "noise"))
    {
      cerebras_v3::copy_text(interpretation->intent, "vehicle noise", cerebras_v3::max_text);
    }
    else if (contains_text(lowered, "maintenance"))
    {
      cerebras_v3::copy_text(interpretation->intent, "maintenance", cerebras_v3::max_text);
    }
    else if (contains_text(lowered, "recall"))
    {
      cerebras_v3::copy_text(interpretation->intent, "recall", cerebras_v3::max_text);
    }
    else if (contains_text(lowered, "warranty"))
    {
      cerebras_v3::copy_text(interpretation->intent, "warranty question", cerebras_v3::max_text);
    }
    else if (contains_text(lowered, "service"))
    {
      cerebras_v3::copy_text(interpretation->intent, "service request", cerebras_v3::max_text);
    }
    else if (contains_text(lowered, "parts") || contains_text(lowered, "part "))
    {
      cerebras_v3::copy_text(interpretation->intent, "parts request", cerebras_v3::max_text);
    }
    else if (contains_text(lowered, "sales") ||
             contains_text(lowered, "buy") ||
             contains_text(lowered, "lease") ||
             contains_text(lowered, "looking for a car") ||
             contains_text(lowered, "looking for car") ||
             contains_text(lowered, "looking for a vehicle") ||
             contains_text(lowered, "looking for vehicle"))
    {
      cerebras_v3::copy_text(interpretation->intent, "sales request", cerebras_v3::max_text);
    }
  }
  if ((interpretation->request[0] == '\0') &&
      (interpretation->department[0] != '\0') &&
      ((state == 0) ||
       (state->last_requested == cerebras_v3::field_none) ||
       (state->last_requested == cerebras_v3::field_request)) &&
      (std::strlen(message) > 3U) &&
      !is_department_only_reply(lowered))
  {
    cerebras_v3::copy_text(interpretation->request, message, cerebras_v3::max_text);
  }
}

static void ensure_call_id(cerebras_v3::State* state)
{
  static int counter = 0;
  char generated[64];
  if ((state == 0) || (state->call_id[0] != '\0'))
  {
    return;
  }
  clear_buffer(generated, 64);
  counter += 1;
  snprintf(
    generated,
    sizeof(generated),
    "local-%ld-%d",
    static_cast<long>(std::time(0)),
    counter);
  cerebras_v3::copy_text(state->call_id, generated, 64);
}

static void set_call_id_if_present(cerebras_v3::State* state, const char* source)
{
  char call_id[64];
  if ((state == 0) || (source == 0) || (state->call_id[0] != '\0'))
  {
    return;
  }
  clear_buffer(call_id, 64);
  if (json_value(source, "\"call_id\"", call_id, 64) ||
      json_value(source, "\"callId\"", call_id, 64) ||
      json_value(source, "\"call_uuid\"", call_id, 64) ||
      json_value(source, "\"retell_call_id\"", call_id, 64))
  {
    cerebras_v3::copy_text(state->call_id, call_id, 64);
  }
}

void set_call_id_from_websocket_path(cerebras_v3::State* state, const char* request, const Config* config)
{
  const char* path = 0;
  const char* end = 0;
  const char* segment = 0;
  const char* next = 0;
  char candidate[64];
  int out = 0;
  bool is_secret_segment = false;
  if ((state == 0) || (request == 0) || (state->call_id[0] != '\0'))
  {
    return;
  }
  path = std::strstr(request, "GET /llm-websocket/");
  if (path == 0)
  {
    return;
  }
  path += 19;
  end = std::strchr(path, ' ');
  if (end == 0)
  {
    return;
  }
  segment = path;
  while ((segment < end) && (*segment != '?'))
  {
    next = segment;
    while ((next < end) && (*next != '/') && (*next != '?'))
    {
      next += 1;
    }
    is_secret_segment =
      (config != 0) &&
      (config->shared_secret[0] != '\0') &&
      (static_cast<std::size_t>(next - segment) == std::strlen(config->shared_secret)) &&
      (std::strncmp(segment, config->shared_secret, static_cast<std::size_t>(next - segment)) == 0);
    clear_buffer(candidate, 64);
    out = 0;
    while (((segment + out) < next) && (out < 63))
    {
      candidate[out] = segment[out];
      out += 1;
    }
    candidate[out] = '\0';
    if ((candidate[0] != '\0') && !is_secret_segment)
    {
      cerebras_v3::copy_text(state->call_id, candidate, 64);
    }
    if ((next < end) && (*next == '/'))
    {
      segment = next + 1;
    }
    else
    {
      return;
    }
  }
}

static const char* faq_answer_for_id(const char* faq_id)
{
  const char* answer = "";
  int index = 0;
  if (faq_id == 0)
  {
    return answer;
  }
  while (index < cerebras_v3::generated_kb::faq_entry_count)
  {
    if (std::strcmp(cerebras_v3::generated_kb::faq_entries[index].id, faq_id) == 0)
    {
      answer = cerebras_v3::generated_kb::faq_entries[index].answer;
      break;
    }
    index += 1;
  }
  return answer;
}

static bool latest_caller_looks_like_question(const char* message, const cerebras_v3::Interpretation* interpretation)
{
  char lowered[text_capacity];
  bool result = false;
  lowercase_text(lowered, message, text_capacity);
  result =
    contains_text(message, "?") ||
    contains_text(lowered, "do you") ||
    contains_text(lowered, "do y'all") ||
    contains_text(lowered, "can you") ||
    contains_text(lowered, "can the") ||
    contains_text(lowered, "are you") ||
    contains_text(lowered, "are y'all") ||
    contains_text(lowered, "is there") ||
    contains_text(lowered, "what are") ||
    contains_text(lowered, "what is") ||
    contains_text(lowered, "when are") ||
    contains_text(lowered, "how do") ||
    contains_text(lowered, "how much") ||
    contains_text(lowered, "how long") ||
    contains_text(lowered, "before that") ||
    contains_text(lowered, "quick question") ||
    contains_text(lowered, "one question");
  if ((interpretation != 0) && (interpretation->faq_question[0] != '\0'))
  {
    result = result || contains_text(lowered, "do") || contains_text(lowered, "can") || contains_text(lowered, "are");
  }
  return result;
}

static bool caller_asks_for_collected_details(const char* message, const cerebras_v3::Interpretation* interpretation)
{
  char lowered[text_capacity];
  if (message == 0)
  {
    return false;
  }
  lowercase_text(lowered, message, text_capacity);
  return
    contains_text(lowered, "what are they") ||
    contains_text(lowered, "what are the details") ||
    contains_text(lowered, "what details") ||
    contains_text(lowered, "what did i provide") ||
    contains_text(lowered, "what have i provided") ||
    contains_text(lowered, "what information") ||
    contains_text(lowered, "what info") ||
    contains_text(lowered, "what do you have") ||
    contains_text(lowered, "what did you get") ||
    contains_text(lowered, "can you repeat") ||
    contains_text(lowered, "repeat the details") ||
    ((interpretation != 0) &&
     (interpretation->faq_question[0] != '\0') &&
     (contains_text(interpretation->faq_question, "details") ||
      contains_text(interpretation->faq_question, "provided")));
}

static bool caller_asks_what_number_was_captured(const char* message)
{
  char lowered[text_capacity];
  if (message == 0)
  {
    return false;
  }
  lowercase_text(lowered, message, text_capacity);
  return
    contains_text(lowered, "what did you get") ||
    contains_text(lowered, "what did you got") ||
    contains_text(lowered, "what number did you get") ||
    contains_text(lowered, "what number do you have") ||
    contains_text(lowered, "what did you write") ||
    contains_text(lowered, "repeat the number");
}

static bool caller_rejects_phone_readback(const char* message, const cerebras_v3::Interpretation* interpretation)
{
  char lowered[text_capacity];
  if ((interpretation != 0) && (std::strcmp(interpretation->affirmation, "no") == 0))
  {
    return true;
  }
  if (message == 0)
  {
    return false;
  }
  lowercase_text(lowered, message, text_capacity);
  return
    contains_text(lowered, "no") ||
    contains_text(lowered, "wrong") ||
    contains_text(lowered, "not right") ||
    contains_text(lowered, "not correct");
}

static bool message_matches_faq_alias(const char* lowered_message, const char* faq_id)
{
  int index = 0;
  if ((lowered_message == 0) || (faq_id == 0))
  {
    return false;
  }
  while (index < cerebras_v3::generated_kb::faq_alias_count)
  {
    if ((std::strcmp(cerebras_v3::generated_kb::faq_aliases[index].faq_id, faq_id) == 0) &&
        contains_text(lowered_message, cerebras_v3::generated_kb::faq_aliases[index].phrase))
    {
      return true;
    }
    index += 1;
  }
  return false;
}

static bool message_has_parts_availability_context(const char* lowered_message)
{
  if (lowered_message == 0)
  {
    return false;
  }
  return
    contains_text(lowered_message, "part") ||
    contains_text(lowered_message, "parts") ||
    contains_text(lowered_message, "accessory") ||
    contains_text(lowered_message, "key fob") ||
    contains_text(lowered_message, "cargo mat") ||
    contains_text(lowered_message, "wiper blade") ||
    contains_text(lowered_message, "brake pad") ||
    contains_text(lowered_message, "filter") ||
    contains_text(lowered_message, "sensor") ||
    contains_text(lowered_message, "rim") ||
    contains_text(lowered_message, "tire");
}

static bool message_asks_about_hours(const char* lowered_message)
{
  if (lowered_message == 0)
  {
    return false;
  }
  return
    message_matches_faq_alias(lowered_message, "service_hours") ||
    contains_text(lowered_message, "what time do you close") ||
    contains_text(lowered_message, "what time do you open") ||
    contains_text(lowered_message, "when do you close") ||
    contains_text(lowered_message, "when do you open") ||
    contains_text(lowered_message, "how late are you open") ||
    contains_text(lowered_message, "hours");
}

static bool faq_alias_allowed_for_message(const char* lowered_message, const char* faq_id)
{
  if ((faq_id != 0) && (std::strcmp(faq_id, "parts_availability") == 0))
  {
    return message_has_parts_availability_context(lowered_message);
  }
  return true;
}

static bool best_matching_faq_id(const char* lowered_message, char* output, int capacity)
{
  int index = 0;
  int best_length = 0;
  clear_buffer(output, capacity);
  if ((lowered_message == 0) || (output == 0) || (capacity <= 0))
  {
    return false;
  }
  while (index < cerebras_v3::generated_kb::faq_alias_count)
  {
    const char* phrase = cerebras_v3::generated_kb::faq_aliases[index].phrase;
    const char* faq_id = cerebras_v3::generated_kb::faq_aliases[index].faq_id;
    const int phrase_length = static_cast<int>(std::strlen(phrase));
    if ((phrase_length > best_length) &&
        faq_alias_allowed_for_message(lowered_message, faq_id) &&
        contains_text(lowered_message, phrase))
    {
      best_length = phrase_length;
      cerebras_v3::copy_text(output, faq_id, capacity);
    }
    index += 1;
  }
  return output[0] != '\0';
}

void correct_faq_id_from_message(const char* message, cerebras_v3::Interpretation* interpretation)
{
  char lowered[text_capacity];
  char best_faq_id[64];
  if ((message == 0) || (interpretation == 0))
  {
    return;
  }
  clear_buffer(best_faq_id, 64);
  lowercase_text(lowered, message, text_capacity);
  if (!latest_caller_looks_like_question(message, interpretation))
  {
    return;
  }
  if (message_matches_faq_alias(lowered, "ev_battery_service"))
  {
    cerebras_v3::copy_text(interpretation->faq_id, "ev_battery_service", 64);
  }
  else if (message_matches_faq_alias(lowered, "financing"))
  {
    cerebras_v3::copy_text(interpretation->faq_id, "financing", 64);
  }
  else if (message_matches_faq_alias(lowered, "trade_in"))
  {
    cerebras_v3::copy_text(interpretation->faq_id, "trade_in", 64);
  }
  else if (message_matches_faq_alias(lowered, "parts_order_status"))
  {
    cerebras_v3::copy_text(interpretation->faq_id, "parts_order_status", 64);
  }
  else if (message_matches_faq_alias(lowered, "service-loaner-vehicle"))
  {
    cerebras_v3::copy_text(interpretation->faq_id, "service-loaner-vehicle", 64);
  }
  else if (message_matches_faq_alias(lowered, "service-shuttle-service"))
  {
    cerebras_v3::copy_text(interpretation->faq_id, "service-shuttle-service", 64);
  }
  else if (message_asks_about_hours(lowered))
  {
    cerebras_v3::copy_text(interpretation->faq_id, "service_hours", 64);
  }
  else if (message_matches_faq_alias(lowered, "sales_inventory") &&
           (contains_text(lowered, "electric hyundai") ||
            contains_text(lowered, "electric hyundais") ||
            contains_text(lowered, "ioniq") ||
            contains_text(lowered, "tucson hybrid") ||
            contains_text(lowered, "santa fe") ||
            contains_text(lowered, "palisade") ||
            contains_text(lowered, "vehicle") ||
            contains_text(lowered, "vehicles") ||
            contains_text(lowered, "car") ||
            contains_text(lowered, "cars")) &&
           (contains_text(lowered, "on the lot") ||
            contains_text(lowered, "available") ||
            contains_text(lowered, "in stock") ||
            contains_text(lowered, "inventory") ||
            contains_text(lowered, "options")))
  {
    cerebras_v3::copy_text(interpretation->faq_id, "sales_inventory", 64);
  }
  else if (message_has_parts_availability_context(lowered) &&
           (message_matches_faq_alias(lowered, "parts_availability") ||
            contains_text(lowered, "availability")))
  {
    cerebras_v3::copy_text(interpretation->faq_id, "parts_availability", 64);
  }
  else if (best_matching_faq_id(lowered, best_faq_id, 64))
  {
    cerebras_v3::copy_text(interpretation->faq_id, best_faq_id, 64);
  }
}

static void append_clean_field(char* output, int capacity, const char* value)
{
  char cleaned[text_capacity];
  int length = 0;
  clear_buffer(cleaned, text_capacity);
  if ((value == 0) || (value[0] == '\0'))
  {
    return;
  }
  cerebras_v3::copy_text(cleaned, value, text_capacity);
  length = static_cast<int>(std::strlen(cleaned));
  while ((length > 0) &&
         ((cleaned[length - 1] == '.') ||
          (cleaned[length - 1] == ',') ||
          (cleaned[length - 1] == ';') ||
          (cleaned[length - 1] == ':') ||
          (cleaned[length - 1] == ' ') ||
          (cleaned[length - 1] == '\t')))
  {
    cleaned[length - 1] = '\0';
    length -= 1;
  }
  append_text(output, capacity, cleaned);
}

static bool callback_text_looks_complete(const char* value)
{
  char lowered[text_capacity];
  lowercase_callback_text(lowered, value, text_capacity);
  return
    contains_text(lowered, " at ") ||
    contains_text(lowered, "monday") ||
    contains_text(lowered, "tuesday") ||
    contains_text(lowered, "wednesday") ||
    contains_text(lowered, "thursday") ||
    contains_text(lowered, "friday") ||
    contains_text(lowered, "saturday") ||
    contains_text(lowered, "sunday") ||
    contains_text(lowered, "january") ||
    contains_text(lowered, "february") ||
    contains_text(lowered, "march") ||
    contains_text(lowered, "april") ||
    contains_text(lowered, "june") ||
    contains_text(lowered, "july") ||
    contains_text(lowered, "august") ||
    contains_text(lowered, "september") ||
    contains_text(lowered, "october") ||
    contains_text(lowered, "november") ||
    contains_text(lowered, "december");
}

static void append_capped_words(char* output, int capacity, const char* value, int max_words)
{
  int input = 0;
  int words = 0;
  bool in_word = false;
  char one[2];
  if ((output == 0) || (value == 0) || (capacity <= 0) || (max_words <= 0))
  {
    return;
  }
  one[1] = '\0';
  while (value[input] != '\0')
  {
    const bool separator =
      (value[input] == ' ') ||
      (value[input] == '\t') ||
      (value[input] == '\n');
    if (separator)
    {
      if (in_word)
      {
        words += 1;
        if (words >= max_words)
        {
          break;
        }
      }
      in_word = false;
    }
    else
    {
      in_word = true;
    }
    one[0] = value[input];
    append_text(output, capacity, one);
    if (static_cast<int>(std::strlen(output)) >= (capacity - 1))
    {
      break;
    }
    input += 1;
  }
}

static void append_callback_datetime_clean(const cerebras_v3::State* state, char* output, int capacity)
{
  const char* date = 0;
  const char* time = 0;
  char combined[cerebras_v3::max_text * 2];
  if (state == 0)
  {
    return;
  }
  clear_buffer(combined, cerebras_v3::max_text * 2);
  date = state->fields[cerebras_v3::field_callback_date].value;
  time = state->fields[cerebras_v3::field_callback_time].value;
  if ((time[0] != '\0') &&
      ((std::strcmp(date, time) == 0) ||
       callback_text_looks_complete(time)))
  {
    append_capped_words(output, capacity, time, 10);
    return;
  }
  if ((date[0] != '\0') && (time[0] == '\0') && callback_text_looks_complete(date))
  {
    append_capped_words(output, capacity, date, 10);
    return;
  }
  if (date[0] != '\0')
  {
    append_clean_field(combined, cerebras_v3::max_text * 2, date);
  }
  if ((time[0] != '\0') && (std::strcmp(date, time) != 0))
  {
    if (combined[0] != '\0')
    {
      append_text(combined, cerebras_v3::max_text * 2, " ");
    }
    append_clean_field(combined, cerebras_v3::max_text * 2, time);
  }
  append_capped_words(output, capacity, combined, 10);
}

static bool template_response(const cerebras_v3::State* state, const cerebras_v3::Plan* plan, char* output, int capacity)
{
  clear_buffer(output, capacity);
  if ((plan == 0) || (output == 0) || (capacity <= 0))
  {
    return false;
  }
  switch (plan->next_field)
  {
    case cerebras_v3::field_department:
      cerebras_v3::copy_text(output, "Is this for service, parts, or sales?", capacity);
      return true;
    case cerebras_v3::field_intent:
      cerebras_v3::copy_text(output, "What can the team help you with?", capacity);
      return true;
    case cerebras_v3::field_caller_name:
      cerebras_v3::copy_text(output, "Can I please have your first and last name?", capacity);
      return true;
    case cerebras_v3::field_last_name_spelling:
      cerebras_v3::copy_text(output, "Can you please spell your last name?", capacity);
      return true;
    case cerebras_v3::field_vehicle:
      if ((state != 0) && (state->department == cerebras_v3::department_parts))
      {
        cerebras_v3::copy_text(output, "What is the year, make, and model for the part?", capacity);
      }
      else
      {
        cerebras_v3::copy_text(output, "What is the year, make, and model?", capacity);
      }
      return true;
    case cerebras_v3::field_request:
      if ((state != 0) && (state->department == cerebras_v3::department_parts))
      {
        cerebras_v3::copy_text(output, "Which specific part should the parts team check?", capacity);
      }
      else if ((state != 0) && (state->department == cerebras_v3::department_sales))
      {
        cerebras_v3::copy_text(output, "Any specific model or type of car?", capacity);
      }
      else
      {
        cerebras_v3::copy_text(output, "What should I note for the team?", capacity);
      }
      return true;
    case cerebras_v3::field_callback_date:
      cerebras_v3::copy_text(output, "What date or day works best for a callback?", capacity);
      return true;
    case cerebras_v3::field_callback_time:
      if ((state != 0) &&
          (state->fields[cerebras_v3::field_callback_time].value[0] != '\0') &&
          !state->fields[cerebras_v3::field_callback_time].confirmed)
      {
        append_text(output, capacity, "Okay, ");
        append_callback_datetime_clean(state, output, capacity);
        append_text(output, capacity, ". Is that right?");
      }
      else
      {
        cerebras_v3::copy_text(output, "What time between 9 AM and 5 PM works best that day?", capacity);
      }
      return true;
    case cerebras_v3::field_phone:
      cerebras_v3::copy_text(output, "Please say the full ten-digit callback number.", capacity);
      return true;
    case cerebras_v3::field_phone_confirmed:
      append_text(output, capacity, "I have ");
      if (state != 0)
      {
        append_text(output, capacity, state->fields[cerebras_v3::field_phone].value);
      }
      append_text(output, capacity, ". Is that correct?");
      return true;
    case cerebras_v3::field_final_confirmed:
      append_text(output, capacity, "I'll pass this to the ");
      append_text(output, capacity, cerebras_v3::department_name(
        (state != 0) ? state->department : cerebras_v3::department_unknown));
      append_text(output, capacity, " team so they can call you back. Will that be all?");
      return true;
    case cerebras_v3::field_none:
      cerebras_v3::copy_text(output, "Perfect, I'll pass this to the team so they can call you back.", capacity);
      return true;
    default:
      break;
  }
  return false;
}

static bool append_collected_details_readback(const cerebras_v3::State* state, char* output, int capacity)
{
  bool wrote_detail = false;
  if ((state == 0) || (output == 0) || (capacity <= 0))
  {
    return false;
  }
  append_text(output, capacity, "I have ");
  if (state->department == cerebras_v3::department_service)
  {
    append_text(output, capacity, "a service request");
  }
  else if (state->department == cerebras_v3::department_parts)
  {
    append_text(output, capacity, "a parts request");
  }
  else if (state->department == cerebras_v3::department_sales)
  {
    append_text(output, capacity, "a sales request");
  }
  else
  {
    append_text(output, capacity, "your request");
  }
  if (state->fields[cerebras_v3::field_request].value[0] != '\0')
  {
    append_text(output, capacity, " about ");
    append_clean_field(output, capacity, state->fields[cerebras_v3::field_request].value);
    wrote_detail = true;
  }
  if (state->fields[cerebras_v3::field_vehicle].value[0] != '\0')
  {
    append_text(output, capacity, " for your ");
    append_clean_field(output, capacity, state->fields[cerebras_v3::field_vehicle].value);
    wrote_detail = true;
  }
  if ((state->fields[cerebras_v3::field_callback_date].value[0] != '\0') ||
      (state->fields[cerebras_v3::field_callback_time].value[0] != '\0'))
  {
    append_text(output, capacity, ", with a callback ");
    append_callback_datetime_clean(state, output, capacity);
    wrote_detail = true;
  }
  if (state->fields[cerebras_v3::field_phone].value[0] != '\0')
  {
    append_text(output, capacity, ", at ");
    append_clean_field(output, capacity, state->fields[cerebras_v3::field_phone].value);
    wrote_detail = true;
  }
  append_text(output, capacity, ". ");
  append_text(output, capacity, "I'll pass that to the ");
  append_text(output, capacity, cerebras_v3::department_name(state->department));
  append_text(output, capacity, " team so they can call you back.");
  return wrote_detail;
}

static void append_transitioned_question(char* output, int capacity, const char* question)
{
  char adjusted[text_capacity];
  if ((output == 0) || (question == 0) || (question[0] == '\0'))
  {
    return;
  }
  clear_buffer(adjusted, text_capacity);
  cerebras_v3::copy_text(adjusted, question, text_capacity);
  if (std::strncmp(adjusted, "What", 4U) == 0)
  {
    adjusted[0] = 'w';
  }
  else if (std::strncmp(adjusted, "Which", 5U) == 0)
  {
    adjusted[0] = 'w';
  }
  append_text(output, capacity, " For the follow-up, ");
  append_text(output, capacity, adjusted);
}

static const char* missed_answer_field_name(cerebras_v3::Field_id field)
{
  switch (field)
  {
    case cerebras_v3::field_department: return "the team";
    case cerebras_v3::field_caller_name: return "your full name";
    case cerebras_v3::field_last_name_spelling: return "the spelling";
    case cerebras_v3::field_vehicle: return "the vehicle";
    case cerebras_v3::field_request: return "the request";
    case cerebras_v3::field_callback_date: return "the callback date";
    case cerebras_v3::field_callback_time: return "the callback time";
    case cerebras_v3::field_phone: return "the callback number";
    default: break;
  }
  return "that";
}

static bool build_missed_answer_response(
  const cerebras_v3::State* state,
  const cerebras_v3::Plan* plan,
  cerebras_v3::Field_id previous_requested,
  const cerebras_v3::Interpretation* interpretation,
  const char* message,
  char* output,
  int capacity)
{
  char question[text_capacity];
  if ((plan == 0) || (output == 0) || (capacity <= 0))
  {
    return false;
  }
  if ((previous_requested == cerebras_v3::field_none) ||
      (previous_requested != plan->next_field) ||
      !cerebras_v3::should_repair_missed_answer(state, previous_requested, interpretation, message))
  {
    return false;
  }
  clear_buffer(question, text_capacity);
  if (!template_response(state, plan, question, text_capacity))
  {
    return false;
  }
  clear_buffer(output, capacity);
  append_text(output, capacity, "Sorry, I did not get ");
  append_text(output, capacity, missed_answer_field_name(previous_requested));
  append_text(output, capacity, ". ");
  append_text(output, capacity, question);
  return true;
}

static bool build_interruption_response(
  const cerebras_v3::State* state,
  const cerebras_v3::Plan* plan,
  const cerebras_v3::Interpretation* interpretation,
  const char* message,
  char* output,
  int capacity)
{
  char question[text_capacity];
  clear_buffer(output, capacity);
  clear_buffer(question, text_capacity);
  if ((plan == 0) || (interpretation == 0) || (output == 0))
  {
    return false;
  }
  if (!template_response(state, plan, question, text_capacity))
  {
    return false;
  }
  if (std::strcmp(interpretation->turn_type, "customer_confusion") == 0)
  {
    append_text(output, capacity, "No problem. ");
    switch (plan->next_field)
    {
      case cerebras_v3::field_department:
        append_text(output, capacity, "I am asking which team should follow up. ");
        break;
      case cerebras_v3::field_caller_name:
        append_text(output, capacity, "I am asking for your first and last name. ");
        break;
      case cerebras_v3::field_last_name_spelling:
        append_text(output, capacity, "I am asking how to spell your last name. ");
        break;
      case cerebras_v3::field_vehicle:
        append_text(output, capacity, "I am asking which vehicle this is for. ");
        break;
      case cerebras_v3::field_request:
        append_text(output, capacity, "I am asking what the team should know. ");
        break;
      case cerebras_v3::field_callback_date:
        append_text(output, capacity, "I am asking what date or day works for the callback. ");
        break;
      case cerebras_v3::field_callback_time:
        append_text(output, capacity, "I am asking when the team should call back. ");
        break;
      case cerebras_v3::field_phone:
        append_text(output, capacity, "I am asking for the best callback number. ");
        break;
      default:
        break;
    }
    append_text(output, capacity, question);
    return true;
  }
  if (std::strcmp(interpretation->turn_type, "caller_question") == 0)
  {
    const char* faq_answer = faq_answer_for_id(interpretation->faq_id);
    if (faq_answer[0] != '\0')
    {
      append_text(output, capacity, faq_answer);
    }
    else
    {
      append_text(output, capacity, "The ");
      if ((state != 0) &&
          (state->department != cerebras_v3::department_unknown))
      {
        append_text(
          output,
          capacity,
          cerebras_v3::department_name(state->department));
      }
      else
      {
        append_text(output, capacity, "dealership");
      }
      append_text(
        output,
        capacity,
        " team will call you back and can confirm that then.");
    }
    if (cerebras_v3::caller_turn_has_multiple_questions(message))
    {
      append_text(output, capacity, " What was the other thing you wanted to ask?");
    }
    else
    {
      append_transitioned_question(output, capacity, question);
    }
    return true;
  }
  if (std::strcmp(interpretation->turn_type, "off_topic") == 0)
  {
    append_text(output, capacity, "I can help pass the message to the right team. ");
    append_text(output, capacity, question);
    return true;
  }
  if (std::strcmp(interpretation->turn_type, "unclear_audio") == 0)
  {
    append_text(output, capacity, "Sorry, I did not catch that. ");
    append_text(output, capacity, question);
    return true;
  }
  if (std::strcmp(interpretation->turn_type, "correction") == 0)
  {
    append_text(output, capacity, "Got it");
    if (std::strcmp(interpretation->answered_field, "department") == 0)
    {
      append_text(output, capacity, ", ");
      append_text(output, capacity, cerebras_v3::department_name(state != 0 ? state->department : cerebras_v3::department_unknown));
    }
    else if ((std::strcmp(interpretation->answered_field, "vehicle") == 0) &&
             (state != 0) &&
             (state->fields[cerebras_v3::field_vehicle].value[0] != '\0'))
    {
      append_text(output, capacity, ", ");
      append_text(output, capacity, state->fields[cerebras_v3::field_vehicle].value);
    }
    else if ((std::strcmp(interpretation->answered_field, "phone") == 0) &&
             (state != 0) &&
             (state->fields[cerebras_v3::field_phone].value[0] != '\0'))
    {
      append_text(output, capacity, ", ");
      append_text(output, capacity, state->fields[cerebras_v3::field_phone].value);
    }
    append_text(output, capacity, ". ");
    append_text(output, capacity, question);
    return true;
  }
  return false;
}

static bool build_rejection_response(
  const cerebras_v3::State* state,
  const cerebras_v3::Plan* plan,
  cerebras_v3::Field_id previous_requested,
  const char* message,
  char* output,
  int capacity)
{
  char question[text_capacity];
  clear_buffer(output, capacity);
  clear_buffer(question, text_capacity);
  if ((plan == 0) || (output == 0) || (message == 0) || (message[0] == '\0'))
  {
    return false;
  }
  if ((previous_requested == cerebras_v3::field_none) ||
      (previous_requested != plan->next_field))
  {
    return false;
  }
  if (!template_response(state, plan, question, text_capacity))
  {
    return false;
  }
  switch (plan->next_field)
  {
    case cerebras_v3::field_caller_name:
      append_text(output, capacity, "I need both your first and last name. ");
      break;
    case cerebras_v3::field_last_name_spelling:
      append_text(output, capacity, "I need the letters of your last name. ");
      break;
    case cerebras_v3::field_vehicle:
      append_text(output, capacity, "I need the year, make, and model so the team knows which vehicle this is for. ");
      break;
    case cerebras_v3::field_request:
      append_text(output, capacity, "I need a short description of what the team should help with. ");
      break;
    case cerebras_v3::field_callback_date:
      append_text(output, capacity, "I need a specific callback date or day. ");
      break;
    case cerebras_v3::field_callback_time:
      append_text(output, capacity, "I need a callback time between 9 AM and 5 PM. ");
      break;
    case cerebras_v3::field_phone:
      append_text(output, capacity, "I need the full ten-digit callback number. ");
      break;
    default:
      return false;
  }
  append_text(output, capacity, question);
  return true;
}

static bool build_phone_recovery_response(
  const cerebras_v3::State* state,
  const cerebras_v3::Plan* plan,
  cerebras_v3::Field_id previous_requested,
  const char* message,
  const cerebras_v3::Interpretation* interpretation,
  char* output,
  int capacity)
{
  if ((plan == 0) || (output == 0) || (message == 0))
  {
    return false;
  }
  clear_buffer(output, capacity);
  if ((previous_requested == cerebras_v3::field_phone_confirmed) &&
      (plan->next_field == cerebras_v3::field_phone) &&
      (caller_rejects_phone_readback(message, interpretation) ||
       caller_asks_what_number_was_captured(message)))
  {
    cerebras_v3::copy_text(
      output,
      "Okay, I will fix the callback number. Please repeat the full ten-digit number.",
      capacity);
    return true;
  }
  if ((plan->next_field == cerebras_v3::field_phone) &&
      caller_asks_what_number_was_captured(message))
  {
    if ((state != 0) && (state->fields[cerebras_v3::field_phone].value[0] != '\0'))
    {
      append_text(output, capacity, "I had ");
      append_text(output, capacity, state->fields[cerebras_v3::field_phone].value);
      append_text(output, capacity, ". Please repeat the full ten-digit callback number.");
    }
    else
    {
      cerebras_v3::copy_text(
        output,
        "I do not have a confirmed number. Please repeat the full ten-digit callback number.",
        capacity);
    }
    return true;
  }
  return false;
}

static bool json_value(const char* json, const char* key, char* output, int capacity)
{
  const char* found = 0;
  int index = 0;
  int out = 0;
  clear_buffer(output, capacity);
  if ((json == 0) || (key == 0) || (output == 0))
  {
    return false;
  }
  found = std::strstr(json, key);
  if (found == 0) { return false; }
  found = std::strchr(found, ':');
  if (found == 0) { return false; }
  found += 1;
  while ((*found != '\0') && (*found != '"')) { found += 1; }
  if (*found == '"') { found += 1; }
  while ((found[index] != '\0') && (found[index] != '"') && (out < (capacity - 1)))
  {
    if ((found[index] == '\\') && (found[index + 1] != '\0')) { index += 1; }
    output[out] = found[index];
    out += 1;
    index += 1;
  }
  output[out] = '\0';
  return (output[0] != '\0');
}

static int json_int_value(const char* json, const char* key, int fallback)
{
  const char* found = 0;
  int value = 0;
  bool any = false;
  if ((json == 0) || (key == 0))
  {
    return fallback;
  }
  found = std::strstr(json, key);
  if (found == 0) { return fallback; }
  found = std::strchr(found, ':');
  if (found == 0) { return fallback; }
  found += 1;
  while ((*found == ' ') || (*found == '\n') || (*found == '\r')) { found += 1; }
  while ((*found >= '0') && (*found <= '9'))
  {
    value = (value * 10) + (*found - '0');
    any = true;
    found += 1;
  }
  return any ? value : fallback;
}

static long json_long_value(const char* json, const char* key, long fallback)
{
  const char* found = 0;
  long value = 0L;
  bool any = false;
  if ((json == 0) || (key == 0))
  {
    return fallback;
  }
  found = std::strstr(json, key);
  if (found == 0) { return fallback; }
  found = std::strchr(found, ':');
  if (found == 0) { return fallback; }
  found += 1;
  while ((*found == ' ') || (*found == '\n') || (*found == '\r')) { found += 1; }
  while ((*found >= '0') && (*found <= '9'))
  {
    value = (value * 10L) + static_cast<long>(*found - '0');
    any = true;
    found += 1;
  }
  return any ? value : fallback;
}

void clear_turn_result(Turn_result* result)
{
  if (result == 0)
  {
    return;
  }
  clear_buffer(result->response_text, text_capacity);
  clear_buffer(result->state_json, 2048);
  clear_buffer(result->employee_summary, summary_capacity);
  clear_buffer(result->next_field, 64);
  clear_buffer(result->turn_type, 64);
  clear_buffer(result->answered_field, 64);
  clear_buffer(result->faq_id, 64);
  clear_buffer(result->affirmation, 32);
  clear_buffer(result->integrity_issues, 256);
  result->used_interpreter = false;
  result->used_generator = false;
  result->used_kb_answer = false;
  result->delivery_attempted = false;
  result->delivery_sent = false;
  result->end_call = false;
}

static bool has_grounded_acknowledgement(
  const cerebras_v3::State* state,
  const cerebras_v3::Interpretation* interpretation,
  cerebras_v3::Field_id previous_requested)
{
  bool meaningful_opening = false;
  bool meaningful_answer = false;
  if ((state == 0) || (interpretation == 0))
  {
    return false;
  }
  meaningful_opening =
    (previous_requested == cerebras_v3::field_none) &&
    ((state->fields[cerebras_v3::field_request].value[0] != '\0') ||
     (state->fields[cerebras_v3::field_vehicle].value[0] != '\0'));
  meaningful_answer =
    (std::strcmp(interpretation->answered_field, "request") == 0) ||
    (std::strcmp(interpretation->answered_field, "vehicle") == 0) ||
    (std::strcmp(interpretation->answered_field, "intent") == 0);
  return meaningful_opening || meaningful_answer;
}

bool try_structured_response(
  cerebras_v3::State* state,
  const Config* config,
  const cerebras_v3::Plan* field_plan,
  const cerebras_v3::Interpretation* interpretation,
  cerebras_v3::Field_id previous_requested,
  const char* previous_response,
  Turn_result* result)
{
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_options options;
  cerebras_v3::Response_render_result rendered;
  Ai_slot_runtime ai_runtime;
  const char* kb_answer = "";
  if ((state == 0) || (config == 0) || !config->structured_responses ||
      (field_plan == 0) || (interpretation == 0) || (result == 0))
  {
    return false;
  }
  if (interpretation->faq_id[0] != '\0')
  {
    kb_answer = faq_answer_for_id(interpretation->faq_id);
  }
  cerebras_v3::init_response_context(&context);
  context.state = state;
  context.field_plan = field_plan;
  context.interpretation = interpretation;
  context.previous_requested = previous_requested;
  context.has_kb_answer = kb_answer[0] != '\0';
  context.has_grounded_acknowledgement =
    has_grounded_acknowledgement(state, interpretation, previous_requested);

  cerebras_v3::init_response_render_options(&options);
  options.kb_answer = kb_answer;
  options.previous_response = (previous_response != 0) ? previous_response : "";
  options.enable_ai_slots = config->ai_response_slots;
  ai_runtime.config = config;
  options.ai_generator = generate_ai_slot_with_cerebras;
  options.ai_user_data = &ai_runtime;
  if (!cerebras_v3::render_structured_response(state, &context, &options, &rendered))
  {
    return false;
  }
  cerebras_v3::copy_text(result->response_text, rendered.text, text_capacity);
  result->used_generator = rendered.used_ai;
  result->used_kb_answer =
    context.has_kb_answer &&
    ((rendered.plan.structure == cerebras_v3::response_structure_answer_ask) ||
     (rendered.plan.structure == cerebras_v3::response_structure_answer_transition_ask));
  cerebras_v3::state_to_json(state, result->state_json, 2048);
  return true;
}

static void log_turn_processed(
  const char* source,
  const cerebras_v3::State* state,
  const Turn_result* result,
  const char* message)
{
  char extra[1024];
  clear_buffer(extra, 1024);
  append_json_string_field(extra, 1024, "source", source);
  append_json_string_field(
    extra,
    1024,
    "department",
    (state != 0) ? cerebras_v3::department_name(state->department) : "unknown");
  append_json_string_field(extra, 1024, "next_field", (result != 0) ? result->next_field : "");
  append_json_string_field(extra, 1024, "turn_type", (result != 0) ? result->turn_type : "");
  append_json_string_field(extra, 1024, "answered_field", (result != 0) ? result->answered_field : "");
  append_json_string_field(extra, 1024, "faq_id", (result != 0) ? result->faq_id : "");
  append_json_string_field(extra, 1024, "integrity_issues", (result != 0) ? result->integrity_issues : "");
  append_json_bool_field(extra, 1024, "message_present", (message != 0) && (message[0] != '\0'));
  append_json_int_field(extra, 1024, "message_chars", (message != 0) ? static_cast<long>(std::strlen(message)) : 0L);
  append_json_bool_field(extra, 1024, "used_interpreter", (result != 0) && result->used_interpreter);
  append_json_bool_field(extra, 1024, "used_generator", (result != 0) && result->used_generator);
  append_json_bool_field(extra, 1024, "used_kb_answer", (result != 0) && result->used_kb_answer);
  append_json_bool_field(extra, 1024, "delivery_attempted", (result != 0) && result->delivery_attempted);
  append_json_bool_field(extra, 1024, "delivery_sent", ((state != 0) && state->delivery_sent) || ((result != 0) && result->delivery_sent));
  append_json_bool_field(extra, 1024, "has_employee_summary", (result != 0) && (result->employee_summary[0] != '\0'));
  log_json_line("turn_processed", (state != 0) ? state->call_id : "", extra);
  if ((result != 0) && (result->integrity_issues[0] != '\0'))
  {
    char integrity_extra[1024];
    clear_buffer(integrity_extra, 1024);
    append_json_string_field(integrity_extra, 1024, "source", source);
    append_json_string_field(integrity_extra, 1024, "issues", result->integrity_issues);
    append_json_string_field(integrity_extra, 1024, "next_field", result->next_field);
    append_json_string_field(integrity_extra, 1024, "turn_type", result->turn_type);
    append_json_string_field(integrity_extra, 1024, "answered_field", result->answered_field);
    log_json_line("conversation_integrity", (state != 0) ? state->call_id : "", integrity_extra);
  }
}

void process_chat_turn(
  cerebras_v3::State* state,
  const Config* config,
  const char* message,
  const char* last_assistant,
  const char* recent_context,
  Turn_result* result)
{
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::Field_id previous_requested = cerebras_v3::field_none;
  if ((state == 0) || (result == 0))
  {
    return;
  }
  clear_turn_result(result);
  if ((message == 0) || (message[0] == '\0'))
  {
    plan = cerebras_v3::plan_next(state);
    state->last_requested = plan.next_field;
    cerebras_v3::state_to_json(state, result->state_json, 2048);
    if (plan.next_field == cerebras_v3::field_department)
    {
      if ((config != 0) && config->structured_responses)
      {
        append_text(result->response_text, text_capacity, "Thanks for calling. I'm the after-hours assistant. How can I help?");
      }
      else
      {
        append_text(result->response_text, text_capacity, "Thanks for calling. How may I help you today?");
      }
    }
    else
    {
      append_text(result->response_text, text_capacity, "Thanks for calling. ");
      char question[text_capacity];
      clear_buffer(question, text_capacity);
      if (template_response(state, &plan, question, text_capacity))
      {
        append_text(result->response_text, text_capacity, question);
      }
    }
    cerebras_v3::copy_text(result->next_field, cerebras_v3::field_label(plan.next_field), 64);
    return;
  }
  cerebras_v3::state_to_json(state, result->state_json, 2048);
  cerebras_v3::clear_interpretation(&interpretation);
  result->used_interpreter = interpret_with_cerebras(config, result->state_json, recent_context, last_assistant, message, &interpretation);
  if (result->used_interpreter)
  {
    correct_faq_id_from_message(message, &interpretation);
  }
  apply_local_interpretation_fallback(state, message, &interpretation);
  apply_relative_callback_time_fallback(state, message, &interpretation);
  if ((config != 0) && config->cerebras_debug)
  {
    std::fprintf(
      stderr,
      "INTERPRETATION_FINAL caller=\"%s\" d=\"%s\" i=\"%s\" v=\"%s\" r=\"%s\" cb=\"%s\" p=\"%s\" n=\"%s\" s=\"%s\" a=\"%s\" f=\"%s\"\n",
      (message != 0) ? message : "",
      interpretation.department,
      interpretation.intent,
      interpretation.vehicle,
      interpretation.request,
      interpretation.callback_time,
      interpretation.phone,
      interpretation.name,
      interpretation.spelling,
      interpretation.affirmation,
      interpretation.faq_id);
  }
  cerebras_v3::merge_interpretation(state, &interpretation, message);
  previous_requested = state->last_requested;
  plan = cerebras_v3::plan_next(state);
  state->last_requested = plan.next_field;
  cerebras_v3::state_to_json(state, result->state_json, 2048);
  if (plan.complete && caller_asks_for_collected_details(message, &interpretation))
  {
    cerebras_v3::copy_text(interpretation.turn_type, "caller_question", 64);
    cerebras_v3::copy_text(interpretation.answered_field, "none", 64);
    (void)append_collected_details_readback(state, result->response_text, text_capacity);
  }
  if (result->response_text[0] == '\0')
  {
    (void)build_phone_recovery_response(
      state,
      &plan,
      previous_requested,
      message,
      &interpretation,
      result->response_text,
      text_capacity);
  }
  if (result->response_text[0] == '\0')
  {
    (void)build_missed_answer_response(
      state,
      &plan,
      previous_requested,
      &interpretation,
      message,
      result->response_text,
      text_capacity);
  }
  if (result->response_text[0] == '\0')
  {
    (void)try_structured_response(
      state,
      config,
      &plan,
      &interpretation,
      previous_requested,
      last_assistant,
      result);
  }
  if (result->response_text[0] == '\0')
  {
    (void)build_interruption_response(state, &plan, &interpretation, message, result->response_text, text_capacity);
  }
  if (result->response_text[0] == '\0')
  {
    (void)build_rejection_response(state, &plan, previous_requested, message, result->response_text, text_capacity);
  }
  if ((result->response_text[0] == '\0') &&
      (interpretation.faq_id[0] != '\0') &&
      latest_caller_looks_like_question(message, &interpretation))
  {
    const char* faq_answer = faq_answer_for_id(interpretation.faq_id);
    if (faq_answer[0] != '\0')
    {
      char question[text_capacity];
      clear_buffer(question, text_capacity);
      result->used_kb_answer = true;
      append_text(result->response_text, text_capacity, faq_answer);
      if (cerebras_v3::caller_turn_has_multiple_questions(message))
      {
        append_text(result->response_text, text_capacity, " What was the other thing you wanted to ask?");
      }
      else if ((plan.next_field != cerebras_v3::field_none) && template_response(state, &plan, question, text_capacity))
      {
        append_transitioned_question(result->response_text, text_capacity, question);
      }
    }
  }
  if ((result->response_text[0] == '\0') && should_generate_opening_ack(state, &plan, previous_requested))
  {
    result->used_generator = generate_opening_ack_with_cerebras(config, result->state_json, result->response_text, text_capacity);
    if (result->used_generator)
    {
      sanitize_response_text(result->response_text, text_capacity);
      if ((result->response_text[0] != '\0') &&
          (result->response_text[static_cast<int>(std::strlen(result->response_text)) - 1] != '.') &&
          (result->response_text[static_cast<int>(std::strlen(result->response_text)) - 1] != '!'))
      {
        append_text(result->response_text, text_capacity, ".");
      }
      append_text(result->response_text, text_capacity, " Can I please have your first and last name?");
    }
  }
  if ((result->response_text[0] == '\0') && !template_response(state, &plan, result->response_text, text_capacity))
  {
    result->used_generator = generate_with_cerebras(config, result->state_json, &plan, result->response_text, text_capacity);
  }
  if (!result->used_generator && (result->response_text[0] == '\0'))
  {
    cerebras_v3::copy_text(result->response_text, plan.fallback_sentence, text_capacity);
  }
  if (plan.complete)
  {
    result->end_call = true;
    build_employee_summary_json(state, result->employee_summary, summary_capacity);
    if (!state->delivery_sent && (config != 0) && (config->delivery_webhook_url[0] != '\0'))
    {
      result->delivery_attempted = true;
      result->delivery_sent = deliver_employee_summary(config, result->employee_summary);
      if (result->delivery_sent)
      {
        state->delivery_sent = true;
        cerebras_v3::state_to_json(state, result->state_json, 2048);
      }
    }
  }
  sanitize_response_text(result->response_text, text_capacity);
  cerebras_v3::state_to_json(state, result->state_json, 2048);
  cerebras_v3::copy_text(result->next_field, cerebras_v3::field_label(plan.next_field), 64);
  cerebras_v3::copy_text(result->turn_type, interpretation.turn_type, 64);
  cerebras_v3::copy_text(result->answered_field, interpretation.answered_field, 64);
  cerebras_v3::copy_text(result->faq_id, interpretation.faq_id, 64);
  cerebras_v3::copy_text(result->affirmation, interpretation.affirmation, 32);
  cerebras_v3::conversation_integrity_issues(
    state,
    previous_requested,
    &interpretation,
    &plan,
    message,
    result->response_text,
    result->end_call,
    result->integrity_issues,
    256);
}

static void http_json(int fd, int status, const char* body)
{
  char response[response_capacity];
  char length_text[32];
  int value = 0;
  int index = 30;
  clear_buffer(response, response_capacity);
  clear_buffer(length_text, 32);
  if (body == 0) { body = "{}"; }
  value = static_cast<int>(std::strlen(body));
  length_text[31] = '\0';
  if (value == 0) { length_text[index] = '0'; index -= 1; }
  while ((value > 0) && (index >= 0))
  {
    length_text[index] = static_cast<char>('0' + (value % 10));
    value = value / 10;
    index -= 1;
  }
  append_text(response, response_capacity, "HTTP/1.1 ");
  append_text(response, response_capacity, (status == 200) ? "200 OK\r\n" : "404 Not Found\r\n");
  append_text(response, response_capacity, "content-type: application/json\r\ncontent-length: ");
  append_text(response, response_capacity, &length_text[index + 1]);
  append_text(response, response_capacity, "\r\nconnection: close\r\n\r\n");
  append_text(response, response_capacity, body);
  (void)write(fd, response, std::strlen(response));
}

static void handle_test_chat(int fd, const char* request, const Config* config)
{
  cerebras_v3::State state;
  Turn_result result;
  char message[text_capacity];
  char last_assistant[text_capacity];
  char recent_context[context_capacity];
  char state_input[2048];
  char body[response_capacity];
  cerebras_v3::init_state(&state);
  clear_turn_result(&result);
  clear_buffer(message, text_capacity);
  clear_buffer(last_assistant, text_capacity);
  clear_buffer(recent_context, context_capacity);
  clear_buffer(state_input, 2048);
  clear_buffer(body, response_capacity);
  (void)json_value(request, "\"message\"", message, text_capacity);
  (void)json_value(request, "\"last_assistant\"", last_assistant, text_capacity);
  (void)json_value(request, "\"recent_context\"", recent_context, context_capacity);
  cerebras_v3::extract_state_json_from_request(request, state_input, 2048);
  if (state_input[0] == '\0')
  {
    (void)json_value(request, "\"state\"", state_input, 2048);
  }
  if (state_input[0] != '\0')
  {
    cerebras_v3::load_state_from_json(&state, state_input);
  }
  set_call_id_if_present(&state, request);
  ensure_call_id(&state);
  process_chat_turn(&state, config, message, last_assistant, recent_context, &result);
  log_turn_processed("http", &state, &result, message);
  append_text(body, response_capacity, "{\"model\":\"cerebras-v3\",");
  append_text(body, response_capacity, "\"call_id\":\"");
  json_escape_append(body, response_capacity, state.call_id);
  append_text(body, response_capacity, "\",");
  append_text(body, response_capacity, "\"used_interpreter\":");
  append_text(body, response_capacity, result.used_interpreter ? "true" : "false");
  append_text(body, response_capacity, ",\"used_generator\":");
  append_text(body, response_capacity, result.used_generator ? "true" : "false");
  append_text(body, response_capacity, ",\"used_kb_answer\":");
  append_text(body, response_capacity, result.used_kb_answer ? "true" : "false");
  append_text(body, response_capacity, ",\"delivery_attempted\":");
  append_text(body, response_capacity, result.delivery_attempted ? "true" : "false");
  append_text(body, response_capacity, ",\"delivery_sent\":");
  append_text(body, response_capacity, ((state.delivery_sent || result.delivery_sent) ? "true" : "false"));
  append_text(body, response_capacity, ",\"faq_id\":\"");
  json_escape_append(body, response_capacity, result.faq_id);
  append_text(body, response_capacity, "\"");
  append_text(body, response_capacity, ",\"affirmation\":\"");
  json_escape_append(body, response_capacity, result.affirmation);
  append_text(body, response_capacity, "\"");
  append_text(body, response_capacity, ",\"next_field\":\"");
  append_text(body, response_capacity, result.next_field);
  append_text(body, response_capacity, "\",\"turn_type\":\"");
  json_escape_append(body, response_capacity, result.turn_type);
  append_text(body, response_capacity, "\",\"answered_field\":\"");
  json_escape_append(body, response_capacity, result.answered_field);
  append_text(body, response_capacity, "\",\"integrity_issues\":\"");
  json_escape_append(body, response_capacity, result.integrity_issues);
  append_text(body, response_capacity, "\",\"content\":\"");
  json_escape_append(body, response_capacity, result.response_text);
  append_text(body, response_capacity, "\",\"state\":");
  append_text(body, response_capacity, result.state_json);
  if (result.employee_summary[0] != '\0')
  {
    append_text(body, response_capacity, ",\"employee_summary\":");
    append_text(body, response_capacity, result.employee_summary);
  }
  append_text(body, response_capacity, "}");
  http_json(fd, 200, body);
}

static void latest_user_from_retell_event(const char* event, char* output, int capacity)
{
  const char* found = event;
  clear_buffer(output, capacity);
  if ((event == 0) || (output == 0))
  {
    return;
  }
  while ((found = std::strstr(found, "\"content\"")) != 0)
  {
    const char* lookback = (found > event + 240) ? (found - 240) : event;
    char window[260];
    int index = 0;
    clear_buffer(window, 260);
    while ((&lookback[index] < found) && (index < 259))
    {
      window[index] = lookback[index];
      index += 1;
    }
    window[index] = '\0';
    if ((std::strstr(window, "\"role\":\"user\"") != 0) ||
        (std::strstr(window, "\"role\": \"user\"") != 0) ||
        (std::strstr(window, "\"speaker\":\"user\"") != 0) ||
        (std::strstr(window, "\"speaker\": \"user\"") != 0))
    {
      (void)json_value(found, "\"content\"", output, capacity);
    }
    found += 9;
  }
  if (output[0] == '\0')
  {
    (void)json_value(event, "\"message\"", output, capacity);
  }
}

void build_retell_response_json(
  char* response,
  int capacity,
  int response_id,
  const char* call_id,
  const char* content,
  bool end_call)
{
  char id_text[32];
  clear_buffer(response, capacity);
  clear_buffer(id_text, 32);
  snprintf(id_text, sizeof(id_text), "%d", response_id);
  append_text(response, capacity, "{\"response_type\":\"response\",\"response_id\":");
  append_text(response, capacity, id_text);
  append_text(response, capacity, ",\"call_id\":\"");
  json_escape_append(response, capacity, (call_id != 0) ? call_id : "");
  append_text(response, capacity, "\"");
  append_text(response, capacity, ",\"content\":\"");
  json_escape_append(response, capacity, content);
  append_text(response, capacity, "\",\"content_complete\":true,\"end_call\":");
  append_text(response, capacity, end_call ? "true}" : "false}");
}

static void websocket_send_retell_response(
  int fd,
  int response_id,
  const char* call_id,
  const char* content,
  bool end_call)
{
  char response[2048];
  build_retell_response_json(response, 2048, response_id, call_id, content, end_call);
  (void)websocket_send_frame(fd, 1, response);
}

static void websocket_send_ping_pong(int fd, const char* event)
{
  char response[128];
  char timestamp_text[32];
  long timestamp = 0L;
  clear_buffer(response, 128);
  clear_buffer(timestamp_text, 32);
  timestamp = json_long_value(event, "\"timestamp\"", 0L);
  if (timestamp <= 0L)
  {
    timestamp = static_cast<long>(std::time(0)) * 1000L;
  }
  snprintf(timestamp_text, sizeof(timestamp_text), "%ld", timestamp);
  append_text(response, 128, "{\"response_type\":\"ping_pong\",\"timestamp\":");
  append_text(response, 128, timestamp_text);
  append_text(response, 128, "}");
  (void)websocket_send_frame(fd, 1, response);
}

static void handle_llm_websocket(int fd, const char* request, const Config* config)
{
  cerebras_v3::State state;
  char event[websocket_capacity];
  char caller_text[text_capacity];
  char last_assistant[text_capacity];
  char retell_caller_number[text_capacity];
  char config_event[256];
  int opcode = 0;
  cerebras_v3::init_state(&state);
  clear_buffer(last_assistant, text_capacity);
  clear_buffer(retell_caller_number, text_capacity);
  set_call_id_from_websocket_path(&state, request, config);
  if (!websocket_handshake(fd, request))
  {
    log_json_line("websocket_handshake_failed", state.call_id, "");
    return;
  }
  clear_buffer(config_event, 256);
  append_text(config_event, 256, "{\"response_type\":\"config\",\"config\":{\"auto_reconnect\":true,\"call_details\":true}}");
  (void)websocket_send_frame(fd, 1, config_event);
  websocket_send_retell_response(
    fd,
    0,
    state.call_id,
    ((config != 0) && config->structured_responses)
      ? "Thanks for calling. I'm the after-hours assistant. How can I help?"
      : "Thanks for calling. How may I help you today?",
    false);
  log_json_line("websocket_open", state.call_id, "");
  while (websocket_read_text(fd, event, websocket_capacity, &opcode))
  {
    if (opcode == 8)
    {
      (void)websocket_send_frame(fd, 8, "");
      break;
    }
    if (opcode == 9)
    {
      (void)websocket_send_frame(fd, 10, "");
    }
    else if (opcode == 1)
    {
      if (contains_text(event, "\"call_details\""))
      {
        caller_number_from_retell_details(
          event,
          retell_caller_number,
          text_capacity);
      }
      else if (contains_text(event, "\"response_required\"") ||
          contains_text(event, "\"reminder_required\""))
      {
        Turn_result result;
        const int response_id = json_int_value(event, "\"response_id\"", 0);
        clear_buffer(caller_text, text_capacity);
        latest_user_from_retell_event(event, caller_text, text_capacity);
        set_call_id_if_present(&state, event);
        ensure_call_id(&state);
        {
          char extra[128];
          clear_buffer(extra, 128);
          append_json_int_field(extra, 128, "response_id", static_cast<long>(response_id));
          append_json_string_field(extra, 128, "trigger", contains_text(event, "\"reminder_required\"") ? "reminder_required" : "response_required");
          log_json_line("websocket_response_required", state.call_id, extra);
        }
        if (!handle_calling_number_request(
              &state,
              caller_text,
              retell_caller_number,
              &result))
        {
          process_chat_turn(&state, config, caller_text, last_assistant, "", &result);
        }
        log_turn_processed("websocket", &state, &result, caller_text);
        websocket_send_retell_response(
          fd,
          response_id,
          state.call_id,
          result.response_text,
          result.end_call);
        {
          char extra[128];
          clear_buffer(extra, 128);
          append_json_int_field(extra, 128, "response_id", static_cast<long>(response_id));
          append_json_int_field(extra, 128, "response_chars", static_cast<long>(std::strlen(result.response_text)));
          log_json_line("websocket_response_sent", state.call_id, extra);
        }
        cerebras_v3::copy_text(last_assistant, result.response_text, text_capacity);
      }
      else if (contains_text(event, "\"ping_pong\""))
      {
        websocket_send_ping_pong(fd, event);
      }
    }
  }
  log_json_line("websocket_close", state.call_id, "");
}

static bool authorize(const Config* config, const char* request)
{
  bool ok = true;
  if ((config != 0) && (config->shared_secret[0] != '\0'))
  {
    ok = contains_text(request, config->shared_secret);
  }
  return ok;
}

static void handle_connection(int fd, const Config* config)
{
  char request[request_capacity];
  const ssize_t count = read(fd, request, request_capacity - 1);
  if (count <= 0)
  {
    close(fd);
    return;
  }
  request[count] = '\0';
  if (starts_with(request, "GET / ") || starts_with(request, "GET /health"))
  {
    http_json(fd, 200, "{\"ok\":true,\"service\":\"retell-cerebras-v3\"}");
  }
  else if (!authorize(config, request))
  {
    log_json_line("http_unauthorized", "", "");
    http_json(fd, 404, "{\"error\":\"unauthorized\"}");
  }
  else if (starts_with(request, "POST /test-chat") ||
           starts_with(request, "POST /retell") ||
           starts_with(request, "POST /call-turn"))
  {
    handle_test_chat(fd, request, config);
  }
  else if (starts_with(request, "GET /llm-websocket"))
  {
    handle_llm_websocket(fd, request, config);
  }
  else
  {
    log_json_line("http_not_found", "", "");
    http_json(fd, 404, "{\"error\":\"not_found\"}");
  }
  close(fd);
}

static int create_server_socket(int port)
{
  int fd = -1;
  int option = 1;
  sockaddr_in address;
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) { return -1; }
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
  std::memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(static_cast<unsigned short>(port));
  if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
  {
    close(fd);
    return -1;
  }
  if (listen(fd, 16) != 0)
  {
    close(fd);
    return -1;
  }
  return fd;
}
#ifndef VOXTEN_TEST_BUILD
int main(int argument_count, char** argument_values, char** envp)
{
  Config config;
  int server_fd = -1;
  (void)argument_count;
  (void)argument_values;
  load_config(envp, &config);
  signal(SIGCHLD, SIG_IGN);
  server_fd = create_server_socket(config.port);
  if (server_fd < 0)
  {
    return 1;
  }
  while (true)
  {
    const int fd = accept(server_fd, 0, 0);
    if (fd >= 0)
    {
      const pid_t pid = fork();
      if (pid == 0)
      {
        close(server_fd);
        handle_connection(fd, &config);
        return 0;
      }
      if (pid < 0)
      {
        handle_connection(fd, &config);
      }
      else
      {
        close(fd);
      }
    }
  }
  return 0;
}
#endif
