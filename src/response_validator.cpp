#include <response_validator.h>
#include <cctype>
#include <cstring>

namespace cerebras_v3
{
static Response_validation_result validation_result(Response_validation_error error)
{
  Response_validation_result result;
  result.valid = error == response_validation_ok;
  result.error = error;
  return result;
}

static void lowercase_copy(char* output, int capacity, const char* input)
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
    output[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(input[index])));
    index += 1;
  }
  output[index] = '\0';
}

static bool contains_case_insensitive(const char* text, const char* pattern)
{
  char lowered_text[2048];
  char lowered_pattern[256];
  lowercase_copy(lowered_text, 2048, text);
  lowercase_copy(lowered_pattern, 256, pattern);
  return std::strstr(lowered_text, lowered_pattern) != 0;
}

static bool contains_banned_wording(const char* text)
{
  const char* banned[] =
  {
    "for our records",
    "assist you better",
    "assist you further",
    "service appointment",
    "schedule an appointment",
    "mr.",
    "ms.",
    "mrs."
  };
  int index = 0;
  while (index < 8)
  {
    if (contains_case_insensitive(text, banned[index]))
    {
      return true;
    }
    index += 1;
  }
  return false;
}

static bool contains_unsafe_claim(const char* text)
{
  const char* unsafe[] =
  {
    "guarantee",
    "definitely",
    "will cost",
    "it costs",
    "is available",
    "we have it",
    "you are booked",
    "you're booked",
    "appointment is scheduled",
    "diagnosis is",
    "$"
  };
  int index = 0;
  while (index < 11)
  {
    if (contains_case_insensitive(text, unsafe[index]))
    {
      return true;
    }
    index += 1;
  }
  return false;
}

static int character_count(const char* text, char character)
{
  int index = 0;
  int count = 0;
  if (text == 0)
  {
    return 0;
  }
  while (text[index] != '\0')
  {
    if (text[index] == character)
    {
      count += 1;
    }
    index += 1;
  }
  return count;
}

static int word_count(const char* text)
{
  int index = 0;
  int count = 0;
  bool in_word = false;
  if (text == 0)
  {
    return 0;
  }
  while (text[index] != '\0')
  {
    const bool word_character =
      (std::isalnum(static_cast<unsigned char>(text[index])) != 0) ||
      (text[index] == '\'');
    if (word_character && !in_word)
    {
      count += 1;
    }
    in_word = word_character;
    index += 1;
  }
  return count;
}

static bool contains_digit(const char* text)
{
  int index = 0;
  if (text == 0)
  {
    return false;
  }
  while (text[index] != '\0')
  {
    if (std::isdigit(static_cast<unsigned char>(text[index])) != 0)
    {
      return true;
    }
    index += 1;
  }
  return false;
}

static void normalize_text(char* output, int capacity, const char* input)
{
  int input_index = 0;
  int output_index = 0;
  bool previous_space = false;
  if ((output == 0) || (capacity <= 0))
  {
    return;
  }
  output[0] = '\0';
  if (input == 0)
  {
    return;
  }
  while ((input[input_index] != '\0') && (output_index < (capacity - 1)))
  {
    const unsigned char character = static_cast<unsigned char>(input[input_index]);
    if (std::isalnum(character) != 0)
    {
      output[output_index] = static_cast<char>(std::tolower(character));
      output_index += 1;
      previous_space = false;
    }
    else if (!previous_space && (output_index > 0))
    {
      output[output_index] = ' ';
      output_index += 1;
      previous_space = true;
    }
    input_index += 1;
  }
  if ((output_index > 0) && (output[output_index - 1] == ' '))
  {
    output_index -= 1;
  }
  output[output_index] = '\0';
}

static bool normalized_equal(const char* left, const char* right)
{
  char normalized_left[2048];
  char normalized_right[2048];
  normalize_text(normalized_left, 2048, left);
  normalize_text(normalized_right, 2048, right);
  return
    (normalized_left[0] != '\0') &&
    (std::strcmp(normalized_left, normalized_right) == 0);
}

static bool acknowledgement_is_grounded(const char* text, const State* state)
{
  if (state == 0)
  {
    return false;
  }
  return
    ((state->fields[field_request].value[0] != '\0') &&
     contains_case_insensitive(text, state->fields[field_request].value)) ||
    ((state->fields[field_vehicle].value[0] != '\0') &&
     contains_case_insensitive(text, state->fields[field_vehicle].value));
}

Response_validation_result validate_ai_slot(
  const char* text,
  Response_act act,
  const State* state)
{
  if (!ai_slot_act_allowed(act))
  {
    return validation_result(response_validation_disallowed_act);
  }
  if ((text == 0) || (text[0] == '\0'))
  {
    return validation_result(response_validation_empty);
  }
  if (character_count(text, '?') != 0)
  {
    return validation_result(response_validation_question_count);
  }
  if (contains_banned_wording(text))
  {
    return validation_result(response_validation_banned_wording);
  }
  if (contains_unsafe_claim(text) || contains_digit(text))
  {
    return validation_result(response_validation_unsafe_claim);
  }
  if (word_count(text) > ai_slot_word_limit(act))
  {
    return validation_result(response_validation_too_long);
  }
  if ((act == response_act_acknowledge) && !acknowledgement_is_grounded(text, state))
  {
    return validation_result(response_validation_ungrounded);
  }
  return validation_result(response_validation_ok);
}

static bool plan_contains_act(const Response_plan* plan, Response_act act)
{
  int index = 0;
  if (plan == 0)
  {
    return false;
  }
  while (index < plan->act_count)
  {
    if (plan->acts[index] == act)
    {
      return true;
    }
    index += 1;
  }
  return false;
}

static const char* required_readback(const Response_plan* plan, const State* state)
{
  if ((plan == 0) || (state == 0) || !plan_contains_act(plan, response_act_readback))
  {
    return "";
  }
  if (plan->target_field == field_callback_time)
  {
    return state->fields[field_callback_time].value;
  }
  if (plan->target_field == field_phone_confirmed)
  {
    return state->fields[field_phone].value;
  }
  return "";
}

Response_validation_result validate_composed_response(
  const char* text,
  const Response_plan* plan,
  const State* state,
  const char* previous_response)
{
  const char* readback = 0;
  const int questions = character_count(text, '?');
  if ((text == 0) || (text[0] == '\0') || (plan == 0))
  {
    return validation_result(response_validation_empty);
  }
  if ((plan->requires_question && (questions != 1)) ||
      (!plan->requires_question && (questions != 0)))
  {
    return validation_result(response_validation_question_count);
  }
  if (contains_banned_wording(text))
  {
    return validation_result(response_validation_banned_wording);
  }
  if (contains_unsafe_claim(text))
  {
    return validation_result(response_validation_unsafe_claim);
  }
  if (word_count(text) > 40)
  {
    return validation_result(response_validation_too_long);
  }
  readback = required_readback(plan, state);
  if ((readback != 0) && (readback[0] != '\0') &&
      !contains_case_insensitive(text, readback))
  {
    return validation_result(response_validation_missing_readback);
  }
  if ((previous_response != 0) && normalized_equal(text, previous_response))
  {
    return validation_result(response_validation_repeated);
  }
  return validation_result(response_validation_ok);
}

const char* response_validation_error_label(Response_validation_error error)
{
  switch (error)
  {
    case response_validation_ok: return "ok";
    case response_validation_empty: return "empty";
    case response_validation_question_count: return "question_count";
    case response_validation_banned_wording: return "banned_wording";
    case response_validation_unsafe_claim: return "unsafe_claim";
    case response_validation_too_long: return "too_long";
    case response_validation_missing_readback: return "missing_readback";
    case response_validation_repeated: return "repeated";
    case response_validation_ungrounded: return "ungrounded";
    case response_validation_disallowed_act: return "disallowed_act";
    default: return "unknown";
  }
}
}
