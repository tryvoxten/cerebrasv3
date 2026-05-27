#include <planner.h>
#include <generated_kb.h>
#include <cctype>
#include <cstring>

namespace cerebras_v3
{
static void clear_text(char* text)
{
  if (text != 0)
  {
    text[0] = '\0';
  }
}

void copy_text(char* destination, const char* source, int capacity)
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
  while ((source[index] != '\0') && (index < (capacity - 1)))
  {
    destination[index] = source[index];
    index += 1;
  }
  destination[index] = '\0';
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

static bool starts_with_text(const char* text, const char* pattern)
{
  bool result = false;
  if ((text != 0) && (pattern != 0))
  {
    result = (std::strncmp(text, pattern, std::strlen(pattern)) == 0);
  }
  return result;
}

static void lowercase(char* output, const char* input, int capacity)
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

static bool is_yes(const char* caller_text)
{
  char lowered[max_text];
  bool result = false;
  int index = 0;
  lowercase(lowered, caller_text, max_text);
  while ((index < generated_kb::affirmation_yes_count) && !result)
  {
    result = contains_text(lowered, generated_kb::affirmation_yes_phrases[index]);
    index += 1;
  }
  return result;
}

static bool is_no(const char* caller_text)
{
  char lowered[max_text];
  bool result = false;
  int index = 0;
  lowercase(lowered, caller_text, max_text);
  result =
    (std::strcmp(lowered, "no") == 0) ||
    starts_with_text(lowered, "no ") ||
    starts_with_text(lowered, "no,") ||
    starts_with_text(lowered, "no.");
  while ((index < generated_kb::affirmation_no_count) && !result)
  {
    result = contains_text(lowered, generated_kb::affirmation_no_phrases[index]);
    index += 1;
  }
  return result;
}

static bool affirmation_is_no(const Interpretation* interpretation, const char* caller_text);

static bool affirmation_is_yes(const Interpretation* interpretation, const char* caller_text)
{
  bool result = false;
  if (affirmation_is_no(interpretation, caller_text))
  {
    return false;
  }
  result = is_yes(caller_text);
  if ((interpretation != 0) && (std::strcmp(interpretation->affirmation, "yes") == 0))
  {
    result = true;
  }
  return result;
}

static bool affirmation_is_no(const Interpretation* interpretation, const char* caller_text)
{
  bool result = is_no(caller_text);
  if ((interpretation != 0) && (std::strcmp(interpretation->affirmation, "no") == 0))
  {
    result = true;
  }
  return result;
}

static bool looks_like_spelled_letters(const char* text)
{
  int index = 0;
  int letter_groups = 0;
  int total_letters = 0;
  bool in_word = false;
  int group_length = 0;
  if (text == 0)
  {
    return false;
  }
  while (text[index] != '\0')
  {
    const unsigned char current = static_cast<unsigned char>(text[index]);
    if (std::isdigit(current) != 0)
    {
      return false;
    }
    if (std::isalpha(current) != 0)
    {
      in_word = true;
      group_length += 1;
      total_letters += 1;
    }
    else
    {
      if (in_word)
      {
        if (group_length != 1)
        {
          return false;
        }
        letter_groups += 1;
      }
      in_word = false;
      group_length = 0;
    }
    index += 1;
  }
  if (in_word)
  {
    if (group_length != 1)
    {
      return false;
    }
    letter_groups += 1;
  }
  return (total_letters >= 2) && (letter_groups >= 2);
}

static bool has_callback_day_signal(const char* lowered)
{
  return
    contains_text(lowered, "tomorrow") ||
    contains_text(lowered, "monday") ||
    contains_text(lowered, "tuesday") ||
    contains_text(lowered, "wednesday") ||
    contains_text(lowered, "thursday") ||
    contains_text(lowered, "friday") ||
    contains_text(lowered, "saturday") ||
    contains_text(lowered, "sunday") ||
    contains_text(lowered, "next ");
}

static bool has_callback_window_signal(const char* lowered)
{
  return
    contains_text(lowered, "morning") ||
    contains_text(lowered, "afternoon") ||
    contains_text(lowered, "lunch") ||
    contains_text(lowered, "after 9") ||
    contains_text(lowered, "after 10") ||
    contains_text(lowered, "after 11") ||
    contains_text(lowered, "after 12") ||
    contains_text(lowered, "after 1") ||
    contains_text(lowered, "after 2") ||
    contains_text(lowered, "after 3") ||
    contains_text(lowered, "after 4") ||
    contains_text(lowered, "before 6") ||
    contains_text(lowered, "before 5") ||
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

static bool has_callback_blocked_signal(const char* lowered)
{
  return
    contains_text(lowered, "later today") ||
    contains_text(lowered, "today later") ||
    contains_text(lowered, "tonight") ||
    contains_text(lowered, "evening") ||
    contains_text(lowered, "after 6") ||
    contains_text(lowered, "after 7") ||
    contains_text(lowered, "after 8") ||
    contains_text(lowered, "8pm") ||
    contains_text(lowered, "8 pm") ||
    contains_text(lowered, "7pm") ||
    contains_text(lowered, "7 pm") ||
    contains_text(lowered, "6pm") ||
    contains_text(lowered, "6 pm");
}

static bool valid_callback_time(const char* text)
{
  char lowered[max_text];
  if ((text == 0) || (text[0] == '\0'))
  {
    return false;
  }
  lowercase(lowered, text, max_text);
  if (has_callback_blocked_signal(lowered))
  {
    return false;
  }
  return has_callback_day_signal(lowered) && has_callback_window_signal(lowered);
}

static bool has_year(const char* text)
{
  int index = 0;
  int year = 0;
  if (text == 0)
  {
    return false;
  }
  while (text[index] != '\0')
  {
    if ((text[index] >= '0') &&
        (text[index] <= '9') &&
        (text[index + 1] >= '0') &&
        (text[index + 1] <= '9') &&
        (text[index + 2] >= '0') &&
        (text[index + 2] <= '9') &&
        (text[index + 3] >= '0') &&
        (text[index + 3] <= '9'))
    {
      year =
        ((text[index] - '0') * 1000) +
        ((text[index + 1] - '0') * 100) +
        ((text[index + 2] - '0') * 10) +
        (text[index + 3] - '0');
      if ((year >= 1995) && (year <= 2026))
      {
        return true;
      }
    }
    index += 1;
  }
  return false;
}

static void normalize_vehicle_text(char* output, const char* input, int capacity)
{
  int in = 0;
  int out = 0;
  if ((output == 0) || (capacity <= 0))
  {
    return;
  }
  if (input == 0)
  {
    output[0] = '\0';
    return;
  }
  while ((input[in] != '\0') && (out < (capacity - 1)))
  {
    const unsigned char current = static_cast<unsigned char>(input[in]);
    if (std::isalnum(current) != 0)
    {
      output[out] = static_cast<char>(std::tolower(current));
      out += 1;
    }
    in += 1;
  }
  output[out] = '\0';
}

static bool is_whitelisted_vehicle(const char* vehicle)
{
  char lowered[max_text];
  char model[max_text];
  char normalized_vehicle[max_text];
  char normalized_model[max_text];
  int index = 0;
  lowercase(lowered, vehicle, max_text);
  normalize_vehicle_text(normalized_vehicle, vehicle, max_text);
  if (has_year(lowered))
  {
    return true;
  }
  while (index < generated_kb::vehicle_model_count)
  {
    lowercase(model, generated_kb::vehicle_models[index], max_text);
    normalize_vehicle_text(normalized_model, generated_kb::vehicle_models[index], max_text);
    if (contains_text(lowered, model) ||
        ((normalized_model[0] != '\0') && contains_text(normalized_vehicle, normalized_model)))
    {
      return true;
    }
    index += 1;
  }
  index = 0;
  while (index < generated_kb::vehicle_alias_count)
  {
    lowercase(model, generated_kb::vehicle_aliases[index], max_text);
    normalize_vehicle_text(normalized_model, generated_kb::vehicle_aliases[index], max_text);
    if (contains_text(lowered, model) ||
        ((normalized_model[0] != '\0') && contains_text(normalized_vehicle, normalized_model)))
    {
      return true;
    }
    index += 1;
  }
  return false;
}

static bool has_name_precursor(const char* caller_text)
{
  char lowered[max_text];
  lowercase(lowered, caller_text, max_text);
  return
    contains_text(lowered, "my name is") ||
    contains_text(lowered, "name is") ||
    contains_text(lowered, "this is") ||
    contains_text(lowered, "it's") ||
    contains_text(lowered, "it is") ||
    contains_text(lowered, "i'm") ||
    contains_text(lowered, "i am") ||
    contains_text(lowered, "speaking");
}

static bool has_non_name_precursor(const char* caller_text)
{
  char lowered[max_text];
  lowercase(lowered, caller_text, max_text);
  return
    contains_text(lowered, "this is for") ||
    contains_text(lowered, "this is about") ||
    contains_text(lowered, "this is regarding") ||
    contains_text(lowered, "it's for") ||
    contains_text(lowered, "it is for") ||
    contains_text(lowered, "i am looking") ||
    contains_text(lowered, "i'm looking");
}

static bool looks_like_person_name(const char* text)
{
  int index = 0;
  int word_count = 0;
  int word_length = 0;
  bool in_word = false;
  bool any_letter = false;
  if ((text == 0) || (text[0] == '\0'))
  {
    return false;
  }
  while (text[index] != '\0')
  {
    const unsigned char c = static_cast<unsigned char>(text[index]);
    if (std::isalpha(c) != 0)
    {
      any_letter = true;
      in_word = true;
      word_length += 1;
    }
    else if ((text[index] == '\'') || (text[index] == '-'))
    {
      if (!in_word)
      {
        return false;
      }
    }
    else if (std::isspace(c) != 0)
    {
      if (in_word)
      {
        if (word_length < 2)
        {
          return false;
        }
        word_count += 1;
      }
      in_word = false;
      word_length = 0;
    }
    else
    {
      return false;
    }
    index += 1;
  }
  if (in_word)
  {
    if (word_length < 2)
    {
      return false;
    }
    word_count += 1;
  }
  return any_letter && (word_count >= 1) && (word_count <= 4);
}

static bool should_capture_name(const State* state, const Interpretation* interpretation, const char* caller_text)
{
  if ((state == 0) || (interpretation == 0) || (interpretation->name[0] == '\0'))
  {
    return false;
  }
  if (state->last_requested == field_caller_name)
  {
    return true;
  }
  if (has_non_name_precursor(caller_text))
  {
    return false;
  }
  if (has_name_precursor(caller_text) && looks_like_person_name(interpretation->name))
  {
    return true;
  }
  return looks_like_person_name(caller_text) && looks_like_person_name(interpretation->name);
}

static void capture(Field* field, const char* value, int confidence)
{
  if ((field == 0) || (value == 0) || (value[0] == '\0'))
  {
    return;
  }
  copy_text(field->value, value, max_text);
  field->confidence = confidence;
  field->status = (confidence >= 80) ? status_captured : status_unclear;
  field->confirmed = false;
}

static bool is_captured(const Field* field)
{
  bool result = false;
  if (field != 0)
  {
    result = (field->status == status_captured);
  }
  return result;
}

static void init_field(Field* field)
{
  if (field != 0)
  {
    clear_text(field->value);
    field->status = status_missing;
    field->confidence = -1;
    field->confirmed = false;
  }
}

void init_state(State* state)
{
  int index = 0;
  if (state == 0)
  {
    return;
  }
  state->department = department_unknown;
  state->last_requested = field_none;
  for (index = 0; index < 10; index += 1)
  {
    init_field(&state->fields[index]);
  }
}

void clear_interpretation(Interpretation* interpretation)
{
  if (interpretation != 0)
  {
    clear_text(interpretation->department);
    clear_text(interpretation->intent);
    clear_text(interpretation->vehicle);
    clear_text(interpretation->request);
    clear_text(interpretation->callback_time);
    clear_text(interpretation->phone);
    clear_text(interpretation->name);
    clear_text(interpretation->spelling);
    clear_text(interpretation->faq_question);
    clear_text(interpretation->faq_id);
    clear_text(interpretation->affirmation);
  }
}

static Department department_from_text(const char* text)
{
  Department department = department_unknown;
  if (text != 0)
  {
    if (contains_text(text, "service"))
    {
      department = department_service;
    }
    else if (contains_text(text, "parts") || contains_text(text, "part"))
    {
      department = department_parts;
    }
    else if (contains_text(text, "sales") || contains_text(text, "sale"))
    {
      department = department_sales;
    }
  }
  return department;
}

void merge_interpretation(State* state, const Interpretation* interpretation, const char* caller_text)
{
  Department department = department_unknown;
  if ((state == 0) || (interpretation == 0))
  {
    return;
  }
  if ((state->last_requested == field_phone_confirmed) && affirmation_is_no(interpretation, caller_text))
  {
    clear_text(state->fields[field_phone].value);
    state->fields[field_phone].status = status_missing;
    state->fields[field_phone].confirmed = false;
    state->fields[field_phone_confirmed].status = status_missing;
    state->fields[field_phone_confirmed].confirmed = false;
    return;
  }
  if ((state->last_requested == field_phone_confirmed) && affirmation_is_yes(interpretation, caller_text))
  {
    state->fields[field_phone_confirmed].status = status_captured;
    state->fields[field_phone_confirmed].confirmed = true;
    return;
  }
  if ((state->last_requested == field_final_confirmed) && affirmation_is_yes(interpretation, caller_text))
  {
    state->fields[field_final_confirmed].status = status_captured;
    state->fields[field_final_confirmed].confirmed = true;
    return;
  }
  department = department_from_text(interpretation->department);
  if (department != department_unknown)
  {
    state->department = department;
    capture(&state->fields[field_department], department_name(department), 100);
  }
  if (!is_captured(&state->fields[field_intent]))
  {
    capture(&state->fields[field_intent], interpretation->intent, 90);
  }
  if (!is_captured(&state->fields[field_vehicle]))
  {
    if (is_whitelisted_vehicle(interpretation->vehicle))
    {
      capture(&state->fields[field_vehicle], interpretation->vehicle, 88);
    }
  }
  if (!is_captured(&state->fields[field_request]))
  {
    capture(&state->fields[field_request], interpretation->request, 88);
  }
  if (!is_captured(&state->fields[field_callback_time]))
  {
    if (valid_callback_time(interpretation->callback_time))
    {
      capture(&state->fields[field_callback_time], interpretation->callback_time, 88);
    }
    else if ((state->last_requested == field_callback_time) && valid_callback_time(caller_text))
    {
      capture(&state->fields[field_callback_time], caller_text, 82);
    }
  }
  if (!is_captured(&state->fields[field_phone]))
  {
    capture(&state->fields[field_phone], interpretation->phone, 92);
  }
  if (!is_captured(&state->fields[field_caller_name]))
  {
    if (should_capture_name(state, interpretation, caller_text))
    {
      capture(&state->fields[field_caller_name], interpretation->name, 88);
    }
  }
  if (!is_captured(&state->fields[field_last_name_spelling]) &&
      ((state->last_requested == field_last_name_spelling) ||
       looks_like_spelled_letters(caller_text)))
  {
    capture(
      &state->fields[field_last_name_spelling],
      (interpretation->spelling[0] != '\0') ? interpretation->spelling : caller_text,
      looks_like_spelled_letters(caller_text) ? 94 : 84);
  }
}

Plan plan_next(const State* state)
{
  Plan plan;
  plan.next_field = field_none;
  plan.response_task = "Close politely and say the message will be passed to the team.";
  plan.fallback_sentence = "Thanks, I will pass this message to the team.";
  plan.complete = false;
  if (state == 0)
  {
    return plan;
  }
  if (!is_captured(&state->fields[field_department]))
  {
    plan.next_field = field_department;
    plan.response_task = "Ask whether the caller needs service, parts, or sales help.";
    plan.fallback_sentence = "Is this for service, parts, or sales?";
  }
  else if (!is_captured(&state->fields[field_intent]))
  {
    plan.next_field = field_intent;
    plan.response_task = "Ask what the caller needs help with in that department.";
    plan.fallback_sentence = "What can the team help you with?";
  }
  else if (!is_captured(&state->fields[field_caller_name]))
  {
    plan.next_field = field_caller_name;
    plan.response_task = "Ask for the caller's first and last name.";
    plan.fallback_sentence = "Who should the team ask for when they call back?";
  }
  else if (!is_captured(&state->fields[field_last_name_spelling]))
  {
    plan.next_field = field_last_name_spelling;
    plan.response_task = "Ask the caller to spell their last name.";
    plan.fallback_sentence = "Could you spell your last name for me?";
  }
  else if (!is_captured(&state->fields[field_vehicle]) &&
           (state->department != department_sales))
  {
    plan.next_field = field_vehicle;
    plan.response_task = "Ask for the vehicle year, make, and model.";
    plan.fallback_sentence = "What is the year, make, and model of the vehicle?";
  }
  else if (!is_captured(&state->fields[field_request]))
  {
    plan.next_field = field_request;
    plan.response_task = "Ask for one short description of the request or issue.";
    plan.fallback_sentence = "What should I note for the team?";
  }
  else if (!is_captured(&state->fields[field_callback_time]))
  {
    plan.next_field = field_callback_time;
    plan.response_task = "Ask what day and time between 9 AM and 6 PM works best for a callback.";
    plan.fallback_sentence = "What day and time between 9 AM and 6 PM works best for a callback?";
  }
  else if (!is_captured(&state->fields[field_phone]))
  {
    plan.next_field = field_phone;
    plan.response_task = "Ask for the best callback phone number.";
    plan.fallback_sentence = "What is the best callback number?";
  }
  else if (!state->fields[field_phone_confirmed].confirmed)
  {
    plan.next_field = field_phone_confirmed;
    plan.response_task = "Read back the callback number and ask if it is correct.";
    plan.fallback_sentence = "Just to confirm, is that the best callback number?";
  }
  else if (!state->fields[field_final_confirmed].confirmed)
  {
    plan.next_field = field_final_confirmed;
    plan.response_task = "Briefly confirm the collected details and ask if they are correct.";
    plan.fallback_sentence = "Just to confirm, are those details correct?";
  }
  else
  {
    plan.complete = true;
  }
  return plan;
}

const char* field_label(Field_id field)
{
  const char* result = "none";
  switch (field)
  {
    case field_department: result = "department"; break;
    case field_intent: result = "intent"; break;
    case field_caller_name: result = "name"; break;
    case field_last_name_spelling: result = "last_name_spelling"; break;
    case field_vehicle: result = "vehicle"; break;
    case field_request: result = "request"; break;
    case field_callback_time: result = "callback_time"; break;
    case field_phone: result = "phone"; break;
    case field_phone_confirmed: result = "phone_confirmed"; break;
    case field_final_confirmed: result = "final_confirmed"; break;
    default: result = "none"; break;
  }
  return result;
}

const char* department_name(Department department)
{
  const char* result = "unknown";
  switch (department)
  {
    case department_service: result = "service"; break;
    case department_parts: result = "parts"; break;
    case department_sales: result = "sales"; break;
    default: result = "unknown"; break;
  }
  return result;
}

static bool json_value(const char* json, const char* key, char* output, int capacity)
{
  const char* found = 0;
  int index = 0;
  int out = 0;
  clear_text(output);
  if ((json == 0) || (key == 0) || (output == 0) || (capacity <= 0))
  {
    return false;
  }
  found = std::strstr(json, key);
  if (found == 0)
  {
    return false;
  }
  found = std::strchr(found, ':');
  if (found == 0)
  {
    return false;
  }
  found += 1;
  while ((*found != '\0') && (*found != '"'))
  {
    found += 1;
  }
  if (*found == '"')
  {
    found += 1;
  }
  while ((found[index] != '\0') && (found[index] != '"') && (out < (capacity - 1)))
  {
    if ((found[index] == '\\') && (found[index + 1] != '\0'))
    {
      index += 1;
    }
    output[out] = found[index];
    out += 1;
    index += 1;
  }
  output[out] = '\0';
  return (output[0] != '\0');
}

static Field_id field_from_text(const char* text)
{
  Field_id field = field_none;
  if (text != 0)
  {
    if (contains_text(text, "department")) { field = field_department; }
    else if (contains_text(text, "intent")) { field = field_intent; }
    else if (contains_text(text, "name")) { field = field_caller_name; }
    else if (contains_text(text, "last_name_spelling") || contains_text(text, "spelling")) { field = field_last_name_spelling; }
    else if (contains_text(text, "vehicle")) { field = field_vehicle; }
    else if (contains_text(text, "request")) { field = field_request; }
    else if (contains_text(text, "callback_time")) { field = field_callback_time; }
    else if (contains_text(text, "phone_confirmed")) { field = field_phone_confirmed; }
    else if (contains_text(text, "final_confirmed")) { field = field_final_confirmed; }
    else if (contains_text(text, "phone")) { field = field_phone; }
  }
  return field;
}

static bool json_bool(const char* json, const char* key)
{
  const char* found = 0;
  bool result = false;
  if ((json == 0) || (key == 0))
  {
    return false;
  }
  found = std::strstr(json, key);
  if (found == 0)
  {
    return false;
  }
  found = std::strchr(found, ':');
  if (found == 0)
  {
    return false;
  }
  found += 1;
  while ((*found == ' ') || (*found == '\n') || (*found == '\r'))
  {
    found += 1;
  }
  result = (std::strncmp(found, "true", 4U) == 0);
  return result;
}

void extract_state_json_from_request(const char* request, char* output, int capacity)
{
  const char* found = 0;
  int index = 0;
  int out = 0;
  int depth = 0;
  bool in_string = false;
  bool escaped = false;
  clear_text(output);
  if ((request == 0) || (output == 0) || (capacity <= 0))
  {
    return;
  }
  found = std::strstr(request, "\"state\"");
  if (found == 0)
  {
    return;
  }
  found = std::strchr(found, ':');
  if (found == 0)
  {
    return;
  }
  found += 1;
  while ((*found == ' ') || (*found == '\n') || (*found == '\r'))
  {
    found += 1;
  }
  if (*found != '{')
  {
    return;
  }
  while ((found[index] != '\0') && (out < (capacity - 1)))
  {
    const char c = found[index];
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

bool parse_interpretation_json(const char* json, Interpretation* interpretation)
{
  bool found = false;
  bool has_department = false;
  bool has_intent = false;
  bool has_request = false;
  if (interpretation == 0)
  {
    return false;
  }
  clear_interpretation(interpretation);
  has_department = json_value(json, "\"d\"", interpretation->department, 32);
  has_intent = json_value(json, "\"i\"", interpretation->intent, max_text);
  has_request = json_value(json, "\"r\"", interpretation->request, max_text);
  found = has_department || has_intent || has_request;
  found = json_value(json, "\"v\"", interpretation->vehicle, max_text) || found;
  found = json_value(json, "\"cb\"", interpretation->callback_time, max_text) || found;
  found = json_value(json, "\"p\"", interpretation->phone, max_text) || found;
  found = json_value(json, "\"n\"", interpretation->name, max_text) || found;
  found = json_value(json, "\"s\"", interpretation->spelling, max_text) || found;
  found = json_value(json, "\"q\"", interpretation->faq_question, max_text) || found;
  found = json_value(json, "\"f\"", interpretation->faq_id, 64) || found;
  found = json_value(json, "\"a\"", interpretation->affirmation, 32) || found;
  return found;
}

static void append(char* output, int capacity, const char* text)
{
  int length = 0;
  int index = 0;
  if ((output == 0) || (text == 0) || (capacity <= 0))
  {
    return;
  }
  length = static_cast<int>(std::strlen(output));
  while ((text[index] != '\0') && (length < (capacity - 1)))
  {
    output[length] = text[index];
    length += 1;
    index += 1;
  }
  output[length] = '\0';
}

static void append_json_string(char* output, int capacity, const char* text)
{
  int index = 0;
  char pair[3];
  pair[2] = '\0';
  append(output, capacity, "\"");
  if (text != 0)
  {
    while (text[index] != '\0')
    {
      if ((text[index] == '"') || (text[index] == '\\'))
      {
        pair[0] = '\\';
        pair[1] = text[index];
        append(output, capacity, pair);
      }
      else
      {
        pair[0] = text[index];
        pair[1] = '\0';
        append(output, capacity, pair);
      }
      index += 1;
    }
  }
  append(output, capacity, "\"");
}

void state_to_json(const State* state, char* output, int capacity)
{
  clear_text(output);
  append(output, capacity, "{");
  append(output, capacity, "\"department\":");
  append_json_string(output, capacity, (state != 0) ? department_name(state->department) : "unknown");
  append(output, capacity, ",\"intent\":");
  append_json_string(output, capacity, (state != 0) ? state->fields[field_intent].value : "");
  append(output, capacity, ",\"name\":");
  append_json_string(output, capacity, (state != 0) ? state->fields[field_caller_name].value : "");
  append(output, capacity, ",\"spelling\":");
  append_json_string(output, capacity, (state != 0) ? state->fields[field_last_name_spelling].value : "");
  append(output, capacity, ",\"vehicle\":");
  append_json_string(output, capacity, (state != 0) ? state->fields[field_vehicle].value : "");
  append(output, capacity, ",\"request\":");
  append_json_string(output, capacity, (state != 0) ? state->fields[field_request].value : "");
  append(output, capacity, ",\"callback_time\":");
  append_json_string(output, capacity, (state != 0) ? state->fields[field_callback_time].value : "");
  append(output, capacity, ",\"phone\":");
  append_json_string(output, capacity, (state != 0) ? state->fields[field_phone].value : "");
  append(output, capacity, ",\"phone_confirmed\":");
  append(output, capacity, ((state != 0) && state->fields[field_phone_confirmed].confirmed) ? "true" : "false");
  append(output, capacity, ",\"final_confirmed\":");
  append(output, capacity, ((state != 0) && state->fields[field_final_confirmed].confirmed) ? "true" : "false");
  append(output, capacity, ",\"last_requested\":");
  append_json_string(output, capacity, (state != 0) ? field_label(state->last_requested) : "none");
  append(output, capacity, "}");
}

void load_state_from_json(State* state, const char* json)
{
  char value[max_text];
  Department department = department_unknown;
  if (state == 0)
  {
    return;
  }
  init_state(state);
  if (json == 0)
  {
    return;
  }
  if (json_value(json, "\"department\"", value, max_text))
  {
    department = department_from_text(value);
    if (department != department_unknown)
    {
      state->department = department;
      capture(&state->fields[field_department], department_name(department), 100);
    }
  }
  if (json_value(json, "\"intent\"", value, max_text)) { capture(&state->fields[field_intent], value, 90); }
  if (json_value(json, "\"name\"", value, max_text)) { capture(&state->fields[field_caller_name], value, 90); }
  if (json_value(json, "\"spelling\"", value, max_text)) { capture(&state->fields[field_last_name_spelling], value, 90); }
  if (json_value(json, "\"vehicle\"", value, max_text)) { capture(&state->fields[field_vehicle], value, 90); }
  if (json_value(json, "\"request\"", value, max_text)) { capture(&state->fields[field_request], value, 90); }
  if (json_value(json, "\"callback_time\"", value, max_text)) { capture(&state->fields[field_callback_time], value, 90); }
  if (json_value(json, "\"phone\"", value, max_text)) { capture(&state->fields[field_phone], value, 90); }
  if (json_bool(json, "\"phone_confirmed\""))
  {
    state->fields[field_phone_confirmed].status = status_captured;
    state->fields[field_phone_confirmed].confirmed = true;
  }
  if (json_bool(json, "\"final_confirmed\""))
  {
    state->fields[field_final_confirmed].status = status_captured;
    state->fields[field_final_confirmed].confirmed = true;
  }
  if (json_value(json, "\"last_requested\"", value, max_text))
  {
    state->last_requested = field_from_text(value);
  }
}

}
