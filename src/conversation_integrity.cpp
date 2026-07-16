#include <conversation_integrity.h>
#include <cctype>
#include <cstring>

namespace cerebras_v3
{
static void append_issue(char* output, int capacity, const char* issue)
{
  if ((output == 0) || (capacity <= 0) || (issue == 0) || (issue[0] == '\0'))
  {
    return;
  }
  if (output[0] != '\0')
  {
    copy_text(&output[std::strlen(output)], ",", capacity - static_cast<int>(std::strlen(output)));
  }
  copy_text(&output[std::strlen(output)], issue, capacity - static_cast<int>(std::strlen(output)));
}

static void lowercase(char* output, const char* input, int capacity)
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

static bool contains_text(const char* text, const char* pattern)
{
  return (text != 0) && (pattern != 0) && (std::strstr(text, pattern) != 0);
}

static int digit_count(const char* text)
{
  int index = 0;
  int count = 0;
  while ((text != 0) && (text[index] != '\0'))
  {
    if (std::isdigit(static_cast<unsigned char>(text[index])) != 0)
    {
      count += 1;
    }
    index += 1;
  }
  return count;
}

static int alpha_word_count(const char* text)
{
  int index = 0;
  int count = 0;
  bool in_word = false;
  while ((text != 0) && (text[index] != '\0'))
  {
    if (std::isalpha(static_cast<unsigned char>(text[index])) != 0)
    {
      in_word = true;
    }
    else
    {
      if (in_word)
      {
        count += 1;
      }
      in_word = false;
    }
    index += 1;
  }
  if (in_word)
  {
    count += 1;
  }
  return count;
}

static int spoken_number_word_count(const char* lowered)
{
  int count = 0;
  if (lowered == 0)
  {
    return 0;
  }
  if (contains_text(lowered, "zero")) { count += 1; }
  if (contains_text(lowered, "one")) { count += 1; }
  if (contains_text(lowered, "two")) { count += 1; }
  if (contains_text(lowered, "three")) { count += 1; }
  if (contains_text(lowered, "four")) { count += 1; }
  if (contains_text(lowered, "five")) { count += 1; }
  if (contains_text(lowered, "six")) { count += 1; }
  if (contains_text(lowered, "seven")) { count += 1; }
  if (contains_text(lowered, "eight")) { count += 1; }
  if (contains_text(lowered, "nine")) { count += 1; }
  return count;
}

static bool has_callback_day_signal(const char* lowered)
{
  return
    contains_text(lowered, "today") ||
    contains_text(lowered, "tomorrow") ||
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
    contains_text(lowered, "may") ||
    contains_text(lowered, "june") ||
    contains_text(lowered, "july") ||
    contains_text(lowered, "august") ||
    contains_text(lowered, "september") ||
    contains_text(lowered, "october") ||
    contains_text(lowered, "november") ||
    contains_text(lowered, "december");
}

static bool has_callback_time_signal(const char* lowered)
{
  return
    contains_text(lowered, "morning") ||
    contains_text(lowered, "afternoon") ||
    contains_text(lowered, "noon") ||
    contains_text(lowered, "am") ||
    contains_text(lowered, "pm") ||
    contains_text(lowered, " at 9") ||
    contains_text(lowered, " at 10") ||
    contains_text(lowered, " at 11") ||
    contains_text(lowered, " at 12") ||
    contains_text(lowered, " at 1") ||
    contains_text(lowered, " at 2") ||
    contains_text(lowered, " at 3") ||
    contains_text(lowered, " at 4") ||
    contains_text(lowered, " at 5") ||
    contains_text(lowered, "around 9") ||
    contains_text(lowered, "around 10") ||
    contains_text(lowered, "around 11") ||
    contains_text(lowered, "around 12") ||
    contains_text(lowered, "around 1") ||
    contains_text(lowered, "around 2") ||
    contains_text(lowered, "around 3") ||
    contains_text(lowered, "around 4") ||
    contains_text(lowered, "around 5");
}

int conversation_question_count(const char* text)
{
  int index = 0;
  int count = 0;
  while ((text != 0) && (text[index] != '\0'))
  {
    if (text[index] == '?')
    {
      count += 1;
    }
    index += 1;
  }
  return count;
}

bool caller_turn_has_multiple_questions(const char* text)
{
  char lowered[max_text * 2];
  int signals = 0;
  lowercase(lowered, text, max_text * 2);
  if (conversation_question_count(text) > 1)
  {
    return true;
  }
  if (contains_text(lowered, " and "))
  {
    signals += 1;
  }
  if (contains_text(lowered, "also"))
  {
    signals += 1;
  }
  if (contains_text(lowered, "what time") || contains_text(lowered, "hours"))
  {
    signals += 1;
  }
  if (contains_text(lowered, "loaner") || contains_text(lowered, "shuttle"))
  {
    signals += 1;
  }
  if (contains_text(lowered, "part") || contains_text(lowered, "availability"))
  {
    signals += 1;
  }
  if (contains_text(lowered, "inventory") || contains_text(lowered, "financing"))
  {
    signals += 1;
  }
  return signals >= 2;
}

bool caller_probably_answered_field(Field_id field, const char* caller_text)
{
  char lowered[max_text * 2];
  lowercase(lowered, caller_text, max_text * 2);
  switch (field)
  {
    case field_department:
      return contains_text(lowered, "service") || contains_text(lowered, "parts") || contains_text(lowered, "sales");
    case field_caller_name:
      return alpha_word_count(caller_text) >= 2;
    case field_last_name_spelling:
      return alpha_word_count(caller_text) >= 2 || contains_text(lowered, "spell");
    case field_vehicle:
      return (alpha_word_count(caller_text) >= 1) && (digit_count(caller_text) >= 2);
    case field_request:
      return alpha_word_count(caller_text) >= 3;
    case field_callback_date:
      return has_callback_day_signal(lowered);
    case field_callback_time:
      return has_callback_day_signal(lowered) || has_callback_time_signal(lowered);
    case field_phone:
      return (digit_count(caller_text) >= 7) || (spoken_number_word_count(lowered) >= 4);
    default:
      break;
  }
  return false;
}

bool conversation_response_is_incomplete(const char* response_text)
{
  int length = 0;
  if ((response_text == 0) || (response_text[0] == '\0'))
  {
    return true;
  }
  length = static_cast<int>(std::strlen(response_text));
  while ((length > 0) &&
         ((response_text[length - 1] == ' ') ||
          (response_text[length - 1] == '\t') ||
          (response_text[length - 1] == '\n')))
  {
    length -= 1;
  }
  if (length <= 0)
  {
    return true;
  }
  return
    (response_text[length - 1] != '.') &&
    (response_text[length - 1] != '?') &&
    (response_text[length - 1] != '!');
}

bool conversation_response_has_multiple_questions(const char* response_text)
{
  return conversation_question_count(response_text) > 1;
}

bool should_repair_missed_answer(
  const State* state,
  Field_id previous_requested,
  const Interpretation* interpretation,
  const char* caller_text)
{
  if ((state == 0) ||
      (previous_requested < field_department) ||
      (previous_requested >= field_none) ||
      (interpretation == 0))
  {
    return false;
  }
  if ((std::strcmp(interpretation->turn_type, "caller_question") == 0) ||
      (std::strcmp(interpretation->turn_type, "customer_confusion") == 0) ||
      (std::strcmp(interpretation->turn_type, "off_topic") == 0) ||
      (std::strcmp(interpretation->turn_type, "correction") == 0))
  {
    return false;
  }
  return
    (state->fields[previous_requested].status != status_captured) &&
    caller_probably_answered_field(previous_requested, caller_text);
}

void conversation_integrity_issues(
  const State* state,
  Field_id previous_requested,
  const Interpretation* interpretation,
  const Plan* plan,
  const char* caller_text,
  const char* response_text,
  bool end_call,
  char* output,
  int capacity)
{
  if ((output == 0) || (capacity <= 0))
  {
    return;
  }
  output[0] = '\0';
  if (caller_turn_has_multiple_questions(caller_text))
  {
    append_issue(output, capacity, "multi_question_caller_turn");
  }
  if (should_repair_missed_answer(state, previous_requested, interpretation, caller_text))
  {
    append_issue(output, capacity, "possible_missed_answer");
  }
  if (conversation_response_has_multiple_questions(response_text))
  {
    append_issue(output, capacity, "multi_question_response");
  }
  if (conversation_response_is_incomplete(response_text))
  {
    append_issue(output, capacity, "incomplete_response");
  }
  if ((end_call || ((plan != 0) && plan->complete)) &&
      (state != 0) &&
      !state->fields[field_final_confirmed].confirmed)
  {
    append_issue(output, capacity, "ended_without_final_confirmation");
  }
}

}
