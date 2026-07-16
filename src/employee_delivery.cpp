#include <employee_delivery.h>
#include <curl/curl.h>
#include <cstdio>
#include <cstring>

namespace
{
const int employee_delivery_curl_capacity = 8192;

struct Buffer
{
  char data[employee_delivery_curl_capacity];
  int length;
};

static void employee_clear_buffer(char* buffer, int capacity)
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

static bool json_value(const char* json, const char* key, char* output, int capacity)
{
  const char* found = 0;
  int index = 0;
  int out = 0;
  employee_clear_buffer(output, capacity);
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
  append_text(output, capacity, value ? "true" : "false");
}

static void append_json_int_field(char* output, int capacity, const char* name, long value)
{
  char number[32];
  employee_clear_buffer(number, 32);
  std::snprintf(number, sizeof(number), "%ld", value);
  append_text(output, capacity, ",\"");
  append_text(output, capacity, name);
  append_text(output, capacity, "\":");
  append_text(output, capacity, number);
}

static void log_json_line(const char* event, const char* call_id, const char* extra)
{
  char line[2048];
  employee_clear_buffer(line, 2048);
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

static size_t curl_write(char* pointer, size_t size, size_t nmemb, void* userdata)
{
  Buffer* buffer = static_cast<Buffer*>(userdata);
  const size_t total = size * nmemb;
  size_t index = 0U;
  if (buffer == 0)
  {
    return 0U;
  }
  while ((index < total) && (buffer->length < (employee_delivery_curl_capacity - 1)))
  {
    buffer->data[buffer->length] = pointer[index];
    buffer->length += 1;
    index += 1U;
  }
  buffer->data[buffer->length] = '\0';
  return total;
}
}

void build_employee_summary_json(
  const cerebras_v3::State* state,
  char* output,
  int capacity)
{
  char callback_datetime[cerebras_v3::max_text * 2];
  employee_clear_buffer(output, capacity);
  employee_clear_buffer(callback_datetime, cerebras_v3::max_text * 2);
  if (state != 0)
  {
    append_text(callback_datetime, cerebras_v3::max_text * 2, state->fields[cerebras_v3::field_callback_date].value);
    if ((state->fields[cerebras_v3::field_callback_date].value[0] != '\0') &&
        (state->fields[cerebras_v3::field_callback_time].value[0] != '\0') &&
        (std::strcmp(state->fields[cerebras_v3::field_callback_date].value, state->fields[cerebras_v3::field_callback_time].value) != 0))
    {
      append_text(callback_datetime, cerebras_v3::max_text * 2, " ");
    }
    if ((state->fields[cerebras_v3::field_callback_time].value[0] != '\0') &&
        (std::strcmp(state->fields[cerebras_v3::field_callback_date].value, state->fields[cerebras_v3::field_callback_time].value) != 0))
    {
      append_text(callback_datetime, cerebras_v3::max_text * 2, state->fields[cerebras_v3::field_callback_time].value);
    }
  }
  append_text(output, capacity, "{\"event\":\"call_summary_ready\"");
  append_text(output, capacity, ",\"call_id\":\"");
  json_escape_append(output, capacity, (state != 0) ? state->call_id : "");
  append_text(output, capacity, "\"");
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
  json_escape_append(output, capacity, callback_datetime);
  append_text(output, capacity, "\",\"callback_date\":\"");
  json_escape_append(output, capacity, (state != 0) ? state->fields[cerebras_v3::field_callback_date].value : "");
  append_text(output, capacity, "\",\"phone\":\"");
  json_escape_append(output, capacity, (state != 0) ? state->fields[cerebras_v3::field_phone].value : "");
  append_text(output, capacity, "\",\"phone_confirmed\":");
  append_text(output, capacity, ((state != 0) && state->fields[cerebras_v3::field_phone_confirmed].confirmed) ? "true" : "false");
  append_text(output, capacity, ",\"final_confirmed\":");
  append_text(output, capacity, ((state != 0) && state->fields[cerebras_v3::field_final_confirmed].confirmed) ? "true" : "false");
  append_text(output, capacity, "}");
}

bool deliver_employee_summary(const Config* config, const char* summary_json)
{
  CURL* curl = 0;
  CURLcode code = CURLE_OK;
  struct curl_slist* headers = 0;
  Buffer buffer;
  char call_id[64];
  char extra[512];
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
  employee_clear_buffer(buffer.data, employee_delivery_curl_capacity);
  employee_clear_buffer(call_id, 64);
  employee_clear_buffer(extra, 512);
  (void)json_value(summary_json, "\"call_id\"", call_id, 64);
  curl = curl_easy_init();
  if (curl == 0)
  {
    append_json_bool_field(extra, 512, "configured", true);
    append_json_bool_field(extra, 512, "sent", false);
    append_json_string_field(extra, 512, "error", "curl_init_failed");
    log_json_line("employee_delivery", call_id, extra);
    return false;
  }
  headers = curl_slist_append(headers, "content-type: application/json");
  if (config->delivery_webhook_secret[0] != '\0')
  {
    employee_clear_buffer(secret_header, 192);
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
  append_json_bool_field(extra, 512, "configured", true);
  append_json_int_field(extra, 512, "http_status", status);
  append_json_int_field(extra, 512, "curl_code", static_cast<long>(code));
  append_json_bool_field(extra, 512, "sent", ok);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  log_json_line("employee_delivery", call_id, extra);
  return ok;
}
