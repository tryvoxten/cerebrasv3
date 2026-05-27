#include <planner.h>
#include <generated_kb.h>
#include <prompt_sections.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cctype>
#include <cstring>

namespace
{
const int request_capacity = 8192;
const int response_capacity = 8192;
const int text_capacity = 1024;
const int cerebras_capacity = 8192;
const int context_capacity = 768;
const int summary_capacity = 4096;
const int default_port = 8080;

struct Config
{
  int port;
  char shared_secret[128];
  char cerebras_key[256];
  char cerebras_model[128];
  char cerebras_url[256];
  char delivery_webhook_url[256];
  char delivery_webhook_secret[128];
};

struct Buffer
{
  char data[cerebras_capacity];
  int length;
};

static void clear_buffer(char* buffer, int capacity)
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

static size_t curl_write(char* pointer, size_t size, size_t nmemb, void* userdata)
{
  Buffer* buffer = static_cast<Buffer*>(userdata);
  const size_t total = size * nmemb;
  size_t index = 0U;
  if (buffer == 0)
  {
    return 0U;
  }
  while ((index < total) && (buffer->length < (cerebras_capacity - 1)))
  {
    buffer->data[buffer->length] = pointer[index];
    buffer->length += 1;
    index += 1U;
  }
  buffer->data[buffer->length] = '\0';
  return total;
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

static void load_config(char** envp, Config* config)
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
  cerebras_v3::copy_text(config->cerebras_model, "llama3.1-8b", 128);
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
}

static void extract_json_string_after(const char* text, const char* marker, char* output, int capacity)
{
  const char* found = 0;
  int index = 0;
  int out = 0;
  clear_buffer(output, capacity);
  if ((text == 0) || (marker == 0) || (output == 0))
  {
    return;
  }
  found = std::strstr(text, marker);
  if (found == 0) { return; }
  found += std::strlen(marker);
  while ((*found != '\0') && (*found != '"')) { found += 1; }
  if (*found == '"') { found += 1; }
  while ((found[index] != '\0') && (found[index] != '"') && (out < (capacity - 1)))
  {
    if ((found[index] == '\\') && (found[index + 1] != '\0'))
    {
      index += 1;
      if (found[index] == 'n')
      {
        output[out] = ' ';
      }
      else
      {
        output[out] = found[index];
      }
    }
    else
    {
      output[out] = found[index];
    }
    out += 1;
    index += 1;
  }
  output[out] = '\0';
}

static void extract_json_content_object(const char* text, char* output, int capacity)
{
  const char* start = 0;
  int depth = 0;
  int index = 0;
  int out = 0;
  bool in_string = false;
  bool escaped = false;
  clear_buffer(output, capacity);
  if ((text == 0) || (output == 0) || (capacity <= 0))
  {
    return;
  }
  start = std::strchr(text, '{');
  if (start == 0)
  {
    return;
  }
  while ((start[index] != '\0') && (out < (capacity - 1)))
  {
    const char c = start[index];
    output[out] = c;
    out += 1;
    if (escaped)
    {
      escaped = false;
    }
    else if (c == '\\')
    {
      escaped = true;
    }
    else if (c == '"')
    {
      in_string = !in_string;
    }
    else if (!in_string && (c == '{'))
    {
      depth += 1;
    }
    else if (!in_string && (c == '}'))
    {
      depth -= 1;
      if (depth <= 0)
      {
        break;
      }
    }
    index += 1;
  }
  output[out] = '\0';
}

static void extract_model_content(const char* response, char* output, int capacity)
{
  const char* choices = 0;
  const char* message = 0;
  const char* content = 0;
  clear_buffer(output, capacity);
  if (response == 0) { return; }
  choices = std::strstr(response, "\"choices\"");
  if (choices == 0) { return; }
  message = std::strstr(choices, "\"message\"");
  if (message == 0) { return; }
  content = std::strstr(message, "\"content\"");
  if (content == 0) { return; }
  extract_json_string_after(content, "\"content\"", output, capacity);
  if (output[0] == '{')
  {
    char object[text_capacity];
    extract_json_content_object(output, object, text_capacity);
    if (object[0] != '\0')
    {
      cerebras_v3::copy_text(output, object, capacity);
    }
  }
}

static bool call_cerebras(const Config* config, const char* system, const char* user, int max_tokens, char* output, int capacity)
{
  CURL* curl = 0;
  CURLcode code = CURLE_OK;
  struct curl_slist* headers = 0;
  Buffer buffer;
  char authorization[320];
  char payload[4096];
  bool ok = false;
  clear_buffer(output, capacity);
  if ((config == 0) || (config->cerebras_key[0] == '\0') || (system == 0) || (user == 0))
  {
    return false;
  }
  buffer.length = 0;
  clear_buffer(buffer.data, cerebras_capacity);
  clear_buffer(authorization, 320);
  clear_buffer(payload, 4096);
  append_text(authorization, 320, "authorization: Bearer ");
  append_text(authorization, 320, config->cerebras_key);
  append_text(payload, 4096, "{\"model\":\"");
  json_escape_append(payload, 4096, config->cerebras_model);
  append_text(payload, 4096, "\",\"stream\":false,\"temperature\":0,\"max_completion_tokens\":");
  if (max_tokens <= 60) { append_text(payload, 4096, "60"); }
  else if (max_tokens <= 80) { append_text(payload, 4096, "80"); }
  else { append_text(payload, 4096, "120"); }
  append_text(payload, 4096, ",\"messages\":[{\"role\":\"system\",\"content\":\"");
  json_escape_append(payload, 4096, system);
  append_text(payload, 4096, "\"},{\"role\":\"user\",\"content\":\"");
  json_escape_append(payload, 4096, user);
  append_text(payload, 4096, "\"}]}");
  curl = curl_easy_init();
  if (curl == 0)
  {
    return false;
  }
  headers = curl_slist_append(headers, authorization);
  headers = curl_slist_append(headers, "content-type: application/json");
  curl_easy_setopt(curl, CURLOPT_URL, config->cerebras_url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 1500L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
  code = curl_easy_perform(curl);
  if (code == CURLE_OK)
  {
    extract_model_content(buffer.data, output, capacity);
    ok = (output[0] != '\0');
  }
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return ok;
}

static bool interpret_with_cerebras(
  const Config* config,
  const char* state_json,
  const char* recent_context,
  const char* last_assistant,
  const char* caller,
  cerebras_v3::Interpretation* interpretation)
{
  char system[2048];
  char user[2048];
  char content[text_capacity];
  clear_buffer(system, 2048);
  append_text(system, 2048, cerebras_v3::prompt_sections::interpreter_role);
  append_text(system, 2048, " ");
  append_text(system, 2048, cerebras_v3::prompt_sections::interpreter_schema);
  append_text(system, 2048, " ");
  append_text(system, 2048, cerebras_v3::prompt_sections::interpreter_field_rules);
  append_text(system, 2048, " ");
  append_text(system, 2048, cerebras_v3::generated_kb::interpreter_faq_rules);
  append_text(system, 2048, " ");
  append_text(system, 2048, cerebras_v3::generated_kb::interpreter_affirmation_rules);
  append_text(system, 2048, " ");
  append_text(system, 2048, cerebras_v3::prompt_sections::interpreter_output_rules);
  clear_buffer(user, 2048);
  append_text(user, 2048, "Known state: ");
  append_text(user, 2048, state_json);
  append_text(user, 2048, "\nRecent context: ");
  append_limited(user, 2048, recent_context, 500);
  append_text(user, 2048, "\nLast assistant: ");
  append_limited(user, 2048, last_assistant, 160);
  append_text(user, 2048, "\nLatest caller: ");
  append_limited(user, 2048, caller, 300);
  if (!call_cerebras(config, system, user, 80, content, text_capacity))
  {
    return false;
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
  return call_cerebras(config, system, user, 60, output, capacity);
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
  return call_cerebras(config, system, user, 60, output, capacity);
}

static void build_employee_summary_json(const cerebras_v3::State* state, char* output, int capacity)
{
  clear_buffer(output, capacity);
  append_text(output, capacity, "{\"event\":\"call_summary_ready\"");
  append_text(output, capacity, ",\"department\":\"");
  json_escape_append(output, capacity, (state != 0) ? cerebras_v3::department_name(state->department) : "unknown");
  append_text(output, capacity, "\"");
  append_text(output, capacity, ",\"summary\":\"");
  if (state != 0)
  {
    append_text(output, capacity, "After-hours ");
    append_text(output, capacity, cerebras_v3::department_name(state->department));
    append_text(output, capacity, " callback request");
    if (state->fields[cerebras_v3::field_caller_name].value[0] != '\0')
    {
      append_text(output, capacity, " for ");
      json_escape_append(output, capacity, state->fields[cerebras_v3::field_caller_name].value);
    }
    if (state->fields[cerebras_v3::field_request].value[0] != '\0')
    {
      append_text(output, capacity, " about ");
      json_escape_append(output, capacity, state->fields[cerebras_v3::field_request].value);
    }
    append_text(output, capacity, ".");
  }
  append_text(output, capacity, "\"");
  append_text(output, capacity, ",\"caller_name\":\"");
  json_escape_append(output, capacity, (state != 0) ? state->fields[cerebras_v3::field_caller_name].value : "");
  append_text(output, capacity, "\",\"last_name_spelling\":\"");
  json_escape_append(output, capacity, (state != 0) ? state->fields[cerebras_v3::field_last_name_spelling].value : "");
  append_text(output, capacity, "\",\"vehicle\":\"");
  json_escape_append(output, capacity, (state != 0) ? state->fields[cerebras_v3::field_vehicle].value : "");
  append_text(output, capacity, "\",\"request\":\"");
  json_escape_append(output, capacity, (state != 0) ? state->fields[cerebras_v3::field_request].value : "");
  append_text(output, capacity, "\",\"intent\":\"");
  json_escape_append(output, capacity, (state != 0) ? state->fields[cerebras_v3::field_intent].value : "");
  append_text(output, capacity, "\",\"callback_time\":\"");
  json_escape_append(output, capacity, (state != 0) ? state->fields[cerebras_v3::field_callback_time].value : "");
  append_text(output, capacity, "\",\"phone\":\"");
  json_escape_append(output, capacity, (state != 0) ? state->fields[cerebras_v3::field_phone].value : "");
  append_text(output, capacity, "\",\"phone_confirmed\":");
  append_text(output, capacity, ((state != 0) && state->fields[cerebras_v3::field_phone_confirmed].confirmed) ? "true" : "false");
  append_text(output, capacity, ",\"final_confirmed\":");
  append_text(output, capacity, ((state != 0) && state->fields[cerebras_v3::field_final_confirmed].confirmed) ? "true" : "false");
  append_text(output, capacity, "}");
}

static bool deliver_employee_summary(const Config* config, const char* summary_json)
{
  CURL* curl = 0;
  CURLcode code = CURLE_OK;
  struct curl_slist* headers = 0;
  Buffer buffer;
  char secret_header[192];
  long status = 0L;
  bool ok = false;
  if ((config == 0) ||
      (config->delivery_webhook_url[0] == '\0') ||
      (summary_json == 0) ||
      (summary_json[0] == '\0'))
  {
    return false;
  }
  buffer.length = 0;
  clear_buffer(buffer.data, cerebras_capacity);
  curl = curl_easy_init();
  if (curl == 0)
  {
    return false;
  }
  headers = curl_slist_append(headers, "content-type: application/json");
  if (config->delivery_webhook_secret[0] != '\0')
  {
    clear_buffer(secret_header, 192);
    append_text(secret_header, 192, "x-voxten-secret: ");
    append_text(secret_header, 192, config->delivery_webhook_secret);
    headers = curl_slist_append(headers, secret_header);
  }
  curl_easy_setopt(curl, CURLOPT_URL, config->delivery_webhook_url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, summary_json);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 2000L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
  code = curl_easy_perform(curl);
  if (code == CURLE_OK)
  {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    ok = (status >= 200L) && (status < 300L);
  }
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return ok;
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

static void correct_faq_id_from_message(const char* message, cerebras_v3::Interpretation* interpretation)
{
  char lowered[text_capacity];
  if ((message == 0) || (interpretation == 0))
  {
    return;
  }
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
  else if (message_matches_faq_alias(lowered, "parts_availability"))
  {
    cerebras_v3::copy_text(interpretation->faq_id, "parts_availability", 64);
  }
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
      cerebras_v3::copy_text(output, "What should I note for the team?", capacity);
      return true;
    case cerebras_v3::field_callback_time:
      cerebras_v3::copy_text(output, "What day and time between 9 AM and 6 PM works best for a callback?", capacity);
      return true;
    case cerebras_v3::field_phone:
      cerebras_v3::copy_text(output, "What is the best callback number?", capacity);
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
      append_text(output, capacity, "I have you down for ");
      if (state != 0)
      {
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
          append_text(output, capacity, "a request");
        }
        if (state->fields[cerebras_v3::field_callback_time].value[0] != '\0')
        {
          append_text(output, capacity, " ");
          append_text(output, capacity, state->fields[cerebras_v3::field_callback_time].value);
        }
        if (state->fields[cerebras_v3::field_vehicle].value[0] != '\0')
        {
          append_text(output, capacity, " for your ");
          append_text(output, capacity, state->fields[cerebras_v3::field_vehicle].value);
        }
        if (state->fields[cerebras_v3::field_request].value[0] != '\0')
        {
          append_text(output, capacity, " about ");
          append_text(output, capacity, state->fields[cerebras_v3::field_request].value);
        }
      }
      else
      {
        append_text(output, capacity, "the request");
      }
      append_text(output, capacity, ". Is that correct?");
      return true;
    case cerebras_v3::field_none:
      cerebras_v3::copy_text(output, "Thanks, the team will follow up.", capacity);
      return true;
    default:
      break;
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
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  char message[text_capacity];
  char last_assistant[text_capacity];
  char recent_context[context_capacity];
  char state_input[2048];
  char state_json[2048];
  char response_text[text_capacity];
  char employee_summary[summary_capacity];
  char body[response_capacity];
  cerebras_v3::Field_id previous_requested = cerebras_v3::field_none;
  bool used_interpreter = false;
  bool used_generator = false;
  bool used_kb_answer = false;
  bool delivery_attempted = false;
  bool delivery_sent = false;
  cerebras_v3::init_state(&state);
  clear_buffer(message, text_capacity);
  clear_buffer(last_assistant, text_capacity);
  clear_buffer(recent_context, context_capacity);
  clear_buffer(state_input, 2048);
  clear_buffer(state_json, 2048);
  clear_buffer(response_text, text_capacity);
  clear_buffer(employee_summary, summary_capacity);
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
  cerebras_v3::state_to_json(&state, state_json, 2048);
  cerebras_v3::clear_interpretation(&interpretation);
  used_interpreter = interpret_with_cerebras(config, state_json, recent_context, last_assistant, message, &interpretation);
  if (used_interpreter)
  {
    correct_faq_id_from_message(message, &interpretation);
  }
  if (used_interpreter)
  {
    cerebras_v3::merge_interpretation(&state, &interpretation, message);
  }
  previous_requested = state.last_requested;
  plan = cerebras_v3::plan_next(&state);
  state.last_requested = plan.next_field;
  cerebras_v3::state_to_json(&state, state_json, 2048);
  used_generator = false;
  if ((interpretation.faq_id[0] != '\0') && latest_caller_looks_like_question(message, &interpretation))
  {
    const char* faq_answer = faq_answer_for_id(interpretation.faq_id);
    if (faq_answer[0] != '\0')
    {
      char question[text_capacity];
      clear_buffer(question, text_capacity);
      used_kb_answer = true;
      append_text(response_text, text_capacity, faq_answer);
      if ((plan.next_field != cerebras_v3::field_none) && template_response(&state, &plan, question, text_capacity))
      {
        append_text(response_text, text_capacity, " ");
        append_text(response_text, text_capacity, question);
      }
    }
  }
  if ((response_text[0] == '\0') && should_generate_opening_ack(&state, &plan, previous_requested))
  {
    used_generator = generate_opening_ack_with_cerebras(config, state_json, response_text, text_capacity);
    if (used_generator)
    {
      sanitize_response_text(response_text, text_capacity);
      if ((response_text[0] != '\0') &&
          (response_text[static_cast<int>(std::strlen(response_text)) - 1] != '.') &&
          (response_text[static_cast<int>(std::strlen(response_text)) - 1] != '!'))
      {
        append_text(response_text, text_capacity, ".");
      }
      append_text(response_text, text_capacity, " Can I please have your first and last name?");
    }
  }
  if ((response_text[0] == '\0') && !template_response(&state, &plan, response_text, text_capacity))
  {
    used_generator = generate_with_cerebras(config, state_json, &plan, response_text, text_capacity);
  }
  if (!used_generator)
  {
    if (response_text[0] == '\0')
    {
      cerebras_v3::copy_text(response_text, plan.fallback_sentence, text_capacity);
    }
  }
  if (plan.complete)
  {
    build_employee_summary_json(&state, employee_summary, summary_capacity);
    if (!state.delivery_sent && (config != 0) && (config->delivery_webhook_url[0] != '\0'))
    {
      delivery_attempted = true;
      delivery_sent = deliver_employee_summary(config, employee_summary);
      if (delivery_sent)
      {
        state.delivery_sent = true;
        cerebras_v3::state_to_json(&state, state_json, 2048);
      }
    }
  }
  sanitize_response_text(response_text, text_capacity);
  append_text(body, response_capacity, "{\"model\":\"cerebras-v3\",");
  append_text(body, response_capacity, "\"used_interpreter\":");
  append_text(body, response_capacity, used_interpreter ? "true" : "false");
  append_text(body, response_capacity, ",\"used_generator\":");
  append_text(body, response_capacity, used_generator ? "true" : "false");
  append_text(body, response_capacity, ",\"used_kb_answer\":");
  append_text(body, response_capacity, used_kb_answer ? "true" : "false");
  append_text(body, response_capacity, ",\"delivery_attempted\":");
  append_text(body, response_capacity, delivery_attempted ? "true" : "false");
  append_text(body, response_capacity, ",\"delivery_sent\":");
  append_text(body, response_capacity, ((state.delivery_sent || delivery_sent) ? "true" : "false"));
  append_text(body, response_capacity, ",\"faq_id\":\"");
  json_escape_append(body, response_capacity, interpretation.faq_id);
  append_text(body, response_capacity, "\"");
  append_text(body, response_capacity, ",\"affirmation\":\"");
  json_escape_append(body, response_capacity, interpretation.affirmation);
  append_text(body, response_capacity, "\"");
  append_text(body, response_capacity, ",\"next_field\":\"");
  append_text(body, response_capacity, cerebras_v3::field_label(plan.next_field));
  append_text(body, response_capacity, "\",\"content\":\"");
  json_escape_append(body, response_capacity, response_text);
  append_text(body, response_capacity, "\",\"state\":");
  append_text(body, response_capacity, state_json);
  if (employee_summary[0] != '\0')
  {
    append_text(body, response_capacity, ",\"employee_summary\":");
    append_text(body, response_capacity, employee_summary);
  }
  append_text(body, response_capacity, "}");
  http_json(fd, 200, body);
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
    http_json(fd, 404, "{\"error\":\"unauthorized\"}");
  }
  else if (starts_with(request, "POST /test-chat"))
  {
    handle_test_chat(fd, request, config);
  }
  else
  {
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
}

int main(int argument_count, char** argument_values, char** envp)
{
  Config config;
  int server_fd = -1;
  (void)argument_count;
  (void)argument_values;
  load_config(envp, &config);
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
      handle_connection(fd, &config);
    }
  }
  return 0;
}
