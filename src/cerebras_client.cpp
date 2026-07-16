#include <cerebras_client.h>
#include <planner.h>
#include <curl/curl.h>
#include <cstdio>
#include <cstring>

struct Buffer
{
  char data[cerebras_capacity];
  int length;
};

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

static void append_limited(char* output, int capacity, const char* text, int max_chars)
{
  int index = 0;
  int length = 0;
  if ((output == 0) || (capacity <= 0) || (text == 0))
  {
    return;
  }
  length = static_cast<int>(std::strlen(output));
  while ((text[index] != '\0') &&
         (length < (capacity - 1)) &&
         (index < max_chars))
  {
    output[length] = text[index];
    length += 1;
    output[length] = '\0';
    index += 1;
  }
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

bool call_cerebras(
  const Config* config,
  const char* system,
  const char* user,
  int max_tokens,
  bool json_mode,
  char* output,
  int capacity)
{
  CURL* curl = 0;
  CURLcode code = CURLE_OK;
  struct curl_slist* headers = 0;
  Buffer buffer;
  char authorization[320];
  char curl_error[CURL_ERROR_SIZE];
  char payload[cerebras_payload_capacity];
  long http_code = 0;
  bool ok = false;
  clear_buffer(output, capacity);
  if ((config == 0) || (config->cerebras_key[0] == '\0') || (system == 0) || (user == 0))
  {
    if ((config != 0) && config->cerebras_debug)
    {
      std::fprintf(stderr, "CEREBRAS_CALL_SKIPPED missing_config_or_key\n");
    }
    return false;
  }
  buffer.length = 0;
  clear_buffer(buffer.data, cerebras_capacity);
  clear_buffer(authorization, 320);
  clear_buffer(curl_error, CURL_ERROR_SIZE);
  clear_buffer(payload, cerebras_payload_capacity);
  append_text(authorization, 320, "authorization: Bearer ");
  append_text(authorization, 320, config->cerebras_key);
  append_text(payload, cerebras_payload_capacity, "{\"model\":\"");
  json_escape_append(payload, cerebras_payload_capacity, config->cerebras_model);
  append_text(payload, cerebras_payload_capacity, "\",\"stream\":false,\"temperature\":0,\"reasoning_effort\":\"low\",\"max_completion_tokens\":");
  if (max_tokens <= 120) { append_text(payload, cerebras_payload_capacity, "120"); }
  else if (max_tokens <= 256) { append_text(payload, cerebras_payload_capacity, "256"); }
  else { append_text(payload, cerebras_payload_capacity, "512"); }
  if (json_mode)
  {
    append_text(payload, cerebras_payload_capacity, ",\"response_format\":{\"type\":\"json_object\"}");
  }
  append_text(payload, cerebras_payload_capacity, ",\"messages\":[{\"role\":\"system\",\"content\":\"");
  json_escape_append(payload, cerebras_payload_capacity, system);
  append_text(payload, cerebras_payload_capacity, "\"},{\"role\":\"user\",\"content\":\"");
  json_escape_append(payload, cerebras_payload_capacity, user);
  append_text(payload, cerebras_payload_capacity, "\"}]}");
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
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 4000L);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
  code = curl_easy_perform(curl);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  if (code == CURLE_OK)
  {
    extract_model_content(buffer.data, output, capacity);
    ok = (output[0] != '\0');
  }
  if ((config != 0) && config->cerebras_debug)
  {
    char response_sample[512];
    clear_buffer(response_sample, 512);
    append_limited(response_sample, 512, buffer.data, 500);
    std::fprintf(
      stderr,
      "CEREBRAS_CALL_DEBUG curl_code=%d http_code=%ld output_len=%lu body=\"%s\" error=\"%s\"\n",
      static_cast<int>(code),
      http_code,
      static_cast<unsigned long>(std::strlen(output)),
      response_sample,
      curl_error);
  }
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return ok;
}
