#include <caller_number.h>
#include <cctype>
#include <cstring>

namespace
{

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

static bool json_value(const char* json, const char* key, char* output, int capacity)
{
  const char* found = 0;
  const char* cursor = 0;
  int index = 0;
  if ((json == 0) || (key == 0) || (output == 0) || (capacity <= 0))
  {
    return false;
  }
  output[0] = '\0';
  found = std::strstr(json, key);
  if (found == 0)
  {
    return false;
  }
  cursor = std::strchr(found + std::strlen(key), ':');
  if (cursor == 0)
  {
    return false;
  }
  cursor += 1;
  while ((*cursor == ' ') || (*cursor == '\t'))
  {
    cursor += 1;
  }
  if (*cursor != '"')
  {
    return false;
  }
  cursor += 1;
  while ((*cursor != '\0') && (*cursor != '"') && (index < (capacity - 1)))
  {
    if ((*cursor == '\\') && (cursor[1] != '\0'))
    {
      cursor += 1;
    }
    output[index] = *cursor;
    index += 1;
    cursor += 1;
  }
  output[index] = '\0';
  return true;
}

static bool caller_asks_for_calling_number(const char* message)
{
  char lowered[text_capacity];
  lowercase_text(lowered, message, text_capacity);
  return
    contains_text(lowered, "number") &&
    (contains_text(lowered, "what number") ||
     contains_text(lowered, "which number") ||
     contains_text(lowered, "check") ||
     contains_text(lowered, "calling from") ||
     contains_text(lowered, "this is"));
}

static void phone_digits_only(char* output, const char* input, int capacity)
{
  int input_index = 0;
  int output_index = 0;
  clear_buffer(output, capacity);
  if ((output == 0) || (input == 0) || (capacity <= 0))
  {
    return;
  }
  while ((input[input_index] != '\0') && (output_index < (capacity - 1)))
  {
    if ((input[input_index] >= '0') && (input[input_index] <= '9'))
    {
      output[output_index] = input[input_index];
      output_index += 1;
    }
    input_index += 1;
  }
  output[output_index] = '\0';
}

}

void caller_number_from_retell_details(
  const char* event,
  char* output,
  int capacity)
{
  char direction[32];
  char number[text_capacity];
  clear_buffer(output, capacity);
  clear_buffer(direction, 32);
  clear_buffer(number, text_capacity);
  if ((event == 0) || !contains_text(event, "\"call_details\""))
  {
    return;
  }
  (void)json_value(event, "\"direction\"", direction, 32);
  if (std::strcmp(direction, "outbound") == 0)
  {
    (void)json_value(event, "\"to_number\"", number, text_capacity);
  }
  else
  {
    (void)json_value(event, "\"from_number\"", number, text_capacity);
  }
  phone_digits_only(output, number, capacity);
}

bool handle_calling_number_request(
  cerebras_v3::State* state,
  const char* caller_text,
  const char* retell_caller_number,
  Turn_result* result)
{
  if ((state == 0) || (result == 0) ||
      (state->last_requested != cerebras_v3::field_phone) ||
      !caller_asks_for_calling_number(caller_text))
  {
    return false;
  }
  clear_turn_result(result);
  result->used_interpreter = false;
  cerebras_v3::copy_text(result->turn_type, "caller_question", 64);
  cerebras_v3::copy_text(result->answered_field, "none", 64);
  if ((retell_caller_number != 0) && (retell_caller_number[0] != '\0'))
  {
    cerebras_v3::copy_text(
      state->fields[cerebras_v3::field_phone].value,
      retell_caller_number,
      cerebras_v3::max_text);
    state->fields[cerebras_v3::field_phone].status = cerebras_v3::status_captured;
    state->fields[cerebras_v3::field_phone].confidence = 100;
    state->fields[cerebras_v3::field_phone].confirmed = false;
    state->last_requested = cerebras_v3::field_phone_confirmed;
    append_text(result->response_text, text_capacity, "The number showing for this call is ");
    append_text(result->response_text, text_capacity, retell_caller_number);
    append_text(result->response_text, text_capacity, ". Is that the best number for the team to call?");
    cerebras_v3::copy_text(result->next_field, "phone_confirmed", 64);
  }
  else
  {
    cerebras_v3::copy_text(
      result->response_text,
      "I can't see a phone number for this call. Please say the best number for the team to call.",
      text_capacity);
    cerebras_v3::copy_text(result->next_field, "phone", 64);
  }
  cerebras_v3::state_to_json(state, result->state_json, 2048);
  return true;
}
