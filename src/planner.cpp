#include <planner.h>
#include <generated_kb.h>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace cerebras_v3
{
static void capture(Field* field, const char* value, int confidence);
static bool is_captured(const Field* field);
static void clear_confirmation(Field* field);

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
    contains_text(lowered, "business day") ||
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
    contains_text(lowered, "december") ||
    contains_text(lowered, "monday") ||
    contains_text(lowered, "tuesday") ||
    contains_text(lowered, "wednesday") ||
    contains_text(lowered, "thursday") ||
    contains_text(lowered, "friday") ||
    contains_text(lowered, "saturday") ||
    contains_text(lowered, "sunday");
}

static bool has_callback_window_signal(const char* lowered)
{
  return
    contains_text(lowered, "anytime") ||
    contains_text(lowered, "any time") ||
    contains_text(lowered, "morning") ||
    contains_text(lowered, "mid morning") ||
    contains_text(lowered, "first thing") ||
    contains_text(lowered, "when you open") ||
    contains_text(lowered, "when they open") ||
    contains_text(lowered, "afternoon") ||
    contains_text(lowered, "mid afternoon") ||
    contains_text(lowered, "lunch") ||
    contains_text(lowered, "lunchtime") ||
    contains_text(lowered, "noon") ||
    contains_text(lowered, "between 9") ||
    contains_text(lowered, "between 10") ||
    contains_text(lowered, "between 11") ||
    contains_text(lowered, "between 12") ||
    contains_text(lowered, "between 1") ||
    contains_text(lowered, "between 2") ||
    contains_text(lowered, "between 3") ||
    contains_text(lowered, "from 9") ||
    contains_text(lowered, "from 10") ||
    contains_text(lowered, "from 11") ||
    contains_text(lowered, "from 12") ||
    contains_text(lowered, "from 1") ||
    contains_text(lowered, "from 2") ||
    contains_text(lowered, "from 3") ||
    contains_text(lowered, "at 9") ||
    contains_text(lowered, "at 10") ||
    contains_text(lowered, "at 11") ||
    contains_text(lowered, "at 12") ||
    contains_text(lowered, "at 1") ||
    contains_text(lowered, "at 2") ||
    contains_text(lowered, "at 3") ||
    contains_text(lowered, "at 4") ||
    contains_text(lowered, "at 5") ||
    contains_text(lowered, " 9 am") ||
    contains_text(lowered, " 9:") ||
    contains_text(lowered, " 10 am") ||
    contains_text(lowered, " 10:") ||
    contains_text(lowered, " 11 am") ||
    contains_text(lowered, " 11:") ||
    contains_text(lowered, " 12 pm") ||
    contains_text(lowered, " 12:") ||
    contains_text(lowered, " 1 pm") ||
    contains_text(lowered, " 1:") ||
    contains_text(lowered, " 2 pm") ||
    contains_text(lowered, " 2:") ||
    contains_text(lowered, " 3 pm") ||
    contains_text(lowered, " 3:") ||
    contains_text(lowered, " 4 pm") ||
    contains_text(lowered, " 4:") ||
    contains_text(lowered, " 5 pm") ||
    contains_text(lowered, " 5:") ||
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
    contains_text(lowered, "night") ||
    contains_text(lowered, "evening") ||
    contains_text(lowered, "after work") ||
    contains_text(lowered, "end of day") ||
    contains_text(lowered, "eod") ||
    contains_text(lowered, "whenever") ||
    contains_text(lowered, "after 6") ||
    contains_text(lowered, "after 7") ||
    contains_text(lowered, "after 8") ||
    contains_text(lowered, "after 9 pm") ||
    contains_text(lowered, "9pm") ||
    contains_text(lowered, "9 pm") ||
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

static bool valid_callback_date(const char* text)
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
  return has_callback_day_signal(lowered);
}

static bool valid_callback_window(const char* text)
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
  return has_callback_window_signal(lowered);
}

static int phone_digit_count(const char* text)
{
  int index = 0;
  int count = 0;
  if (text == 0)
  {
    return 0;
  }
  while (text[index] != '\0')
  {
    if (std::isdigit(static_cast<unsigned char>(text[index])) != 0)
    {
      count += 1;
    }
    index += 1;
  }
  return count;
}

static bool valid_callback_phone(const char* text)
{
  const int digits = phone_digit_count(text);
  return (digits == 10) || (digits == 11);
}

static void clear_callback_fields(State* state)
{
  if (state == 0)
  {
    return;
  }
  clear_text(state->fields[field_callback_date].value);
  state->fields[field_callback_date].status = status_missing;
  state->fields[field_callback_date].confirmed = false;
  clear_text(state->fields[field_callback_time].value);
  state->fields[field_callback_time].status = status_missing;
  state->fields[field_callback_time].confirmed = false;
}

static bool capture_callback_parts(
  State* state,
  const char* date_text,
  const char* time_text,
  const char* combined_text,
  int confidence)
{
  bool changed = false;
  if (state == 0)
  {
    return false;
  }
  if (!is_captured(&state->fields[field_callback_date]) && valid_callback_date(date_text))
  {
    capture(&state->fields[field_callback_date], date_text, confidence);
    changed = true;
  }
  if (!is_captured(&state->fields[field_callback_time]) && valid_callback_window(time_text))
  {
    capture(&state->fields[field_callback_time], time_text, confidence);
    changed = true;
  }
  if (valid_callback_time(combined_text))
  {
    if (!is_captured(&state->fields[field_callback_date]))
    {
      capture(&state->fields[field_callback_date], combined_text, confidence);
      changed = true;
    }
    if (!is_captured(&state->fields[field_callback_time]))
    {
      capture(&state->fields[field_callback_time], combined_text, confidence);
      changed = true;
    }
  }
  if (changed)
  {
    clear_confirmation(&state->fields[field_final_confirmed]);
  }
  return changed;
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

static int vehicle_year(const char* text)
{
  int index = 0;
  int year = 0;
  if (text == 0)
  {
    return 0;
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
        return year;
      }
    }
    index += 1;
  }
  return 0;
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

static bool normalized_contains_any(
  const char* normalized,
  const char* a,
  const char* b,
  const char* c,
  const char* d,
  const char* e,
  const char* f,
  const char* g)
{
  return
    ((a != 0) && contains_text(normalized, a)) ||
    ((b != 0) && contains_text(normalized, b)) ||
    ((c != 0) && contains_text(normalized, c)) ||
    ((d != 0) && contains_text(normalized, d)) ||
    ((e != 0) && contains_text(normalized, e)) ||
    ((f != 0) && contains_text(normalized, f)) ||
    ((g != 0) && contains_text(normalized, g));
}

static bool looks_like_ford_f150(const char* vehicle)
{
  char normalized[max_text];
  normalize_vehicle_text(normalized, vehicle, max_text);
  return
    contains_text(normalized, "ford") &&
    normalized_contains_any(
      normalized,
      "f150",
      "fonefifty",
      "fdashonefifty",
      "fone50",
      "fwonfifty",
      "fonefivezero",
      "fonefiveoh");
}

static bool looks_like_bmw_3_series(const char* vehicle)
{
  char normalized[max_text];
  normalize_vehicle_text(normalized, vehicle, max_text);
  return
    contains_text(normalized, "bmw") &&
    (contains_text(normalized, "3series") ||
     contains_text(normalized, "threeseries") ||
     contains_text(normalized, "m340i") ||
     contains_text(normalized, "mthreefortyi") ||
     contains_text(normalized, "mthreefortyeye"));
}

static bool looks_like_mazda_cx5(const char* vehicle)
{
  char normalized[max_text];
  normalize_vehicle_text(normalized, vehicle, max_text);
  return
    contains_text(normalized, "mazda") &&
    (contains_text(normalized, "cx5") ||
     contains_text(normalized, "cxfive"));
}

static bool looks_like_hyundai_ioniq5(const char* vehicle)
{
  char normalized[max_text];
  normalize_vehicle_text(normalized, vehicle, max_text);
  return
    contains_text(normalized, "hyundai") &&
    (contains_text(normalized, "ioniq5") ||
     contains_text(normalized, "ioniqfive") ||
     contains_text(normalized, "ionic5") ||
     contains_text(normalized, "ionicfive"));
}

static bool looks_like_toyota_86(const char* vehicle)
{
  char normalized[max_text];
  normalize_vehicle_text(normalized, vehicle, max_text);
  return
    contains_text(normalized, "toyota") &&
    (contains_text(normalized, "86") ||
     contains_text(normalized, "eightysix"));
}

static bool looks_like_mercedes_c_class(const char* vehicle)
{
  char normalized[max_text];
  normalize_vehicle_text(normalized, vehicle, max_text);
  return
    (contains_text(normalized, "mercedes") || contains_text(normalized, "benz")) &&
    (contains_text(normalized, "cclass") ||
     contains_text(normalized, "c300") ||
     contains_text(normalized, "cthreehundred"));
}

static bool looks_like_known_spoken_model_code(const char* vehicle)
{
  return
    looks_like_ford_f150(vehicle) ||
    looks_like_bmw_3_series(vehicle) ||
    looks_like_mazda_cx5(vehicle) ||
    looks_like_hyundai_ioniq5(vehicle) ||
    looks_like_toyota_86(vehicle) ||
    looks_like_mercedes_c_class(vehicle);
}

static void copy_year_make_model(char* output, int capacity, int year, const char* make_model)
{
  char year_text[16];
  clear_text(output);
  clear_text(year_text);
  if ((output == 0) || (capacity <= 0) || (make_model == 0))
  {
    return;
  }
  if (year > 0)
  {
    std::snprintf(year_text, sizeof(year_text), "%d", year);
    copy_text(output, year_text, capacity);
    if (static_cast<int>(std::strlen(output)) < (capacity - 1))
    {
      std::strncat(output, " ", static_cast<unsigned long>((capacity - 1) - static_cast<int>(std::strlen(output))));
    }
    if (static_cast<int>(std::strlen(output)) < (capacity - 1))
    {
      std::strncat(output, make_model, static_cast<unsigned long>((capacity - 1) - static_cast<int>(std::strlen(output))));
    }
  }
  else
  {
    copy_text(output, make_model, capacity);
  }
}

static void canonical_vehicle_text(char* output, const char* vehicle, int capacity)
{
  const int year = vehicle_year(vehicle);
  clear_text(output);
  if ((output == 0) || (capacity <= 0))
  {
    return;
  }
  if (looks_like_ford_f150(vehicle))
  {
    copy_year_make_model(output, capacity, year, "Ford F-150");
    return;
  }
  if (looks_like_bmw_3_series(vehicle))
  {
    copy_year_make_model(output, capacity, year, "BMW 3 Series");
    return;
  }
  if (looks_like_mazda_cx5(vehicle))
  {
    copy_year_make_model(output, capacity, year, "Mazda CX-5");
    return;
  }
  if (looks_like_hyundai_ioniq5(vehicle))
  {
    copy_year_make_model(output, capacity, year, "Hyundai IONIQ 5");
    return;
  }
  if (looks_like_toyota_86(vehicle))
  {
    copy_year_make_model(output, capacity, year, "Toyota 86");
    return;
  }
  if (looks_like_mercedes_c_class(vehicle))
  {
    copy_year_make_model(output, capacity, year, "Mercedes-Benz C-Class");
    return;
  }
  copy_text(output, vehicle, capacity);
}

static bool has_alpha(const char* text)
{
  int index = 0;
  if (text == 0)
  {
    return false;
  }
  while (text[index] != '\0')
  {
    if (std::isalpha(static_cast<unsigned char>(text[index])) != 0)
    {
      return true;
    }
    index += 1;
  }
  return false;
}

static bool is_canonical_vehicle_record(const char* vehicle)
{
  char normalized_vehicle[max_text];
  char normalized_canonical[max_text];
  int index = 0;
  normalize_vehicle_text(normalized_vehicle, vehicle, max_text);
  if (normalized_vehicle[0] == '\0')
  {
    return false;
  }
  while (index < generated_kb::vehicle_record_count)
  {
    normalize_vehicle_text(normalized_canonical, generated_kb::vehicle_records[index].canonical, max_text);
    if ((normalized_canonical[0] != '\0') &&
        (std::strcmp(normalized_vehicle, normalized_canonical) == 0))
    {
      return true;
    }
    index += 1;
  }
  return false;
}

static bool mentions_vehicle_model_or_alias(const char* vehicle)
{
  char lowered[max_text];
  char model[max_text];
  char normalized_vehicle[max_text];
  char normalized_model[max_text];
  int index = 0;
  lowercase(lowered, vehicle, max_text);
  normalize_vehicle_text(normalized_vehicle, vehicle, max_text);
  while (index < generated_kb::vehicle_model_count)
  {
    lowercase(model, generated_kb::vehicle_models[index], max_text);
    if (!has_alpha(model))
    {
      index += 1;
      continue;
    }
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

static bool is_whitelisted_vehicle(const char* vehicle)
{
  if (is_canonical_vehicle_record(vehicle))
  {
    return true;
  }
  if (looks_like_known_spoken_model_code(vehicle))
  {
    return true;
  }
  if (has_year(vehicle) && mentions_vehicle_model_or_alias(vehicle))
  {
    return true;
  }
  return mentions_vehicle_model_or_alias(vehicle);
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

static int person_name_word_count(const char* text)
{
  int index = 0;
  int word_count = 0;
  bool in_word = false;
  if (text == 0)
  {
    return 0;
  }
  while (text[index] != '\0')
  {
    const unsigned char c = static_cast<unsigned char>(text[index]);
    if (std::isalpha(c) != 0)
    {
      in_word = true;
    }
    else
    {
      if (in_word)
      {
        word_count += 1;
      }
      in_word = false;
    }
    index += 1;
  }
  if (in_word)
  {
    word_count += 1;
  }
  return word_count;
}

static bool should_capture_name(const State* state, const Interpretation* interpretation, const char* caller_text)
{
  if ((state == 0) || (interpretation == 0) || (interpretation->name[0] == '\0'))
  {
    return false;
  }
  if (state->last_requested == field_caller_name)
  {
    return looks_like_person_name(interpretation->name) && (person_name_word_count(interpretation->name) >= 2);
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

static bool request_too_generic_for_sales(const char* request)
{
  char lowered[max_text];
  if ((request == 0) || (request[0] == '\0'))
  {
    return false;
  }
  lowercase(lowered, request, max_text);
  if (contains_text(lowered, "suv") ||
      contains_text(lowered, "truck") ||
      contains_text(lowered, "sedan") ||
      contains_text(lowered, "van") ||
      contains_text(lowered, "hybrid") ||
      contains_text(lowered, "electric") ||
      contains_text(lowered, " ev") ||
      contains_text(lowered, "used") ||
      contains_text(lowered, "new") ||
      contains_text(lowered, "trade") ||
      contains_text(lowered, "financ") ||
      contains_text(lowered, "payment") ||
      contains_text(lowered, "lease"))
  {
    return false;
  }
  return
    contains_text(lowered, "looking for a car") ||
    contains_text(lowered, "looking for car") ||
    contains_text(lowered, "looking for a vehicle") ||
    contains_text(lowered, "looking for vehicle") ||
    contains_text(lowered, "interested in a car") ||
    contains_text(lowered, "interested in a vehicle") ||
    contains_text(lowered, "need a car") ||
    contains_text(lowered, "need a vehicle") ||
    contains_text(lowered, "buy a car") ||
    contains_text(lowered, "buy a vehicle");
}

static bool request_too_generic_for_parts(const char* request)
{
  char lowered[max_text];
  if ((request == 0) || (request[0] == '\0'))
  {
    return false;
  }
  lowercase(lowered, request, max_text);
  if (contains_text(lowered, "brake") ||
      contains_text(lowered, "pad") ||
      contains_text(lowered, "rotor") ||
      contains_text(lowered, "battery") ||
      contains_text(lowered, "tire") ||
      contains_text(lowered, "key fob") ||
      contains_text(lowered, "cargo") ||
      contains_text(lowered, "mat") ||
      contains_text(lowered, "wiper") ||
      contains_text(lowered, "mirror") ||
      contains_text(lowered, "filter") ||
      contains_text(lowered, "sensor") ||
      contains_text(lowered, "bumper") ||
      contains_text(lowered, "headlight") ||
      contains_text(lowered, "tail light") ||
      contains_text(lowered, "taillight") ||
      contains_text(lowered, "spark") ||
      contains_text(lowered, "plug"))
  {
    return false;
  }
  return
    contains_text(lowered, "need a part") ||
    contains_text(lowered, "looking for a part") ||
    contains_text(lowered, "looking for parts") ||
    contains_text(lowered, "parts request") ||
    contains_text(lowered, "part for my car") ||
    contains_text(lowered, "part for my vehicle");
}

static bool request_too_generic_for_department(Department department, const char* request)
{
  if (department == department_sales)
  {
    return request_too_generic_for_sales(request);
  }
  if (department == department_parts)
  {
    return request_too_generic_for_parts(request);
  }
  return false;
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
  clear_text(state->call_id);
  state->department = department_unknown;
  state->last_requested = field_none;
  state->delivery_sent = false;
  state->history.turn_count = 0;
  state->history.recent_structure_count = 0;
  state->history.recent_phrase_count = 0;
  state->history.last_response_act = 0;
  state->history.phase = conversation_phase_opening;
  state->history.interrupted_field = field_none;
  state->history.caller_pace = caller_pace_unknown;
  state->history.caller_confused = false;
  for (index = 0; index < tracked_field_count; index += 1)
  {
    init_field(&state->fields[index]);
    state->history.retry_counts[index] = 0;
  }
  for (index = 0; index < max_recent_structures; index += 1)
  {
    state->history.recent_structure_ids[index] = 0;
  }
  for (index = 0; index < max_recent_phrase_ids; index += 1)
  {
    state->history.recent_phrase_ids[index] = 0;
  }
}

void clear_interpretation(Interpretation* interpretation)
{
  if (interpretation != 0)
  {
    clear_text(interpretation->turn_type);
    clear_text(interpretation->answered_field);
    clear_text(interpretation->meaning);
    clear_text(interpretation->department);
    clear_text(interpretation->intent);
    clear_text(interpretation->vehicle);
    clear_text(interpretation->request);
    clear_text(interpretation->callback_date);
    clear_text(interpretation->callback_time);
    clear_text(interpretation->phone);
    clear_text(interpretation->name);
    clear_text(interpretation->spelling);
    clear_text(interpretation->faq_question);
    clear_text(interpretation->faq_id);
    clear_text(interpretation->affirmation);
  }
}

static bool is_turn_type(const Interpretation* interpretation, const char* turn_type)
{
  return
    (interpretation != 0) &&
    (turn_type != 0) &&
    (std::strcmp(interpretation->turn_type, turn_type) == 0);
}

static bool should_skip_capture_for_turn(const Interpretation* interpretation)
{
  return
    is_turn_type(interpretation, "customer_confusion") ||
    is_turn_type(interpretation, "caller_question") ||
    is_turn_type(interpretation, "off_topic") ||
    is_turn_type(interpretation, "unclear_audio");
}

static void clear_confirmation(Field* field)
{
  if (field != 0)
  {
    clear_text(field->value);
    field->status = status_missing;
    field->confidence = -1;
    field->confirmed = false;
  }
}

static bool answered_field_is(const Interpretation* interpretation, const char* field)
{
  return
    (interpretation != 0) &&
    (field != 0) &&
    (std::strcmp(interpretation->answered_field, field) == 0);
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

static void merge_interpretation_fields(State* state, const Interpretation* interpretation, const char* caller_text)
{
  Department department = department_unknown;
  char canonical_vehicle[max_text];
  if ((state == 0) || (interpretation == 0))
  {
    return;
  }
  clear_text(canonical_vehicle);
  if ((state->last_requested == field_callback_time) &&
      is_captured(&state->fields[field_callback_time]) &&
      !state->fields[field_callback_time].confirmed &&
      affirmation_is_no(interpretation, caller_text))
  {
    clear_callback_fields(state);
    return;
  }
  if ((state->last_requested == field_callback_time) &&
      is_captured(&state->fields[field_callback_time]) &&
      !state->fields[field_callback_time].confirmed &&
      affirmation_is_yes(interpretation, caller_text))
  {
    state->fields[field_callback_time].confirmed = true;
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
    if (valid_callback_phone(state->fields[field_phone].value))
    {
      state->fields[field_phone_confirmed].status = status_captured;
      state->fields[field_phone_confirmed].confirmed = true;
      state->fields[field_final_confirmed].status = status_captured;
      state->fields[field_final_confirmed].confirmed = true;
    }
    else
    {
      clear_text(state->fields[field_phone].value);
      state->fields[field_phone].status = status_missing;
      state->fields[field_phone].confirmed = false;
      state->fields[field_phone_confirmed].status = status_missing;
      state->fields[field_phone_confirmed].confirmed = false;
    }
    return;
  }
  if ((state->last_requested == field_final_confirmed) && affirmation_is_yes(interpretation, caller_text))
  {
    state->fields[field_final_confirmed].status = status_captured;
    state->fields[field_final_confirmed].confirmed = true;
    return;
  }
  if (should_skip_capture_for_turn(interpretation))
  {
    return;
  }
  if (is_turn_type(interpretation, "correction"))
  {
    department = department_from_text(interpretation->department);
    if (answered_field_is(interpretation, "department") && (department != department_unknown))
    {
      state->department = department;
      capture(&state->fields[field_department], department_name(department), 100);
      clear_confirmation(&state->fields[field_final_confirmed]);
    }
    if (answered_field_is(interpretation, "vehicle") && is_whitelisted_vehicle(interpretation->vehicle))
    {
      canonical_vehicle_text(canonical_vehicle, interpretation->vehicle, max_text);
      capture(&state->fields[field_vehicle], canonical_vehicle, 92);
      clear_confirmation(&state->fields[field_final_confirmed]);
    }
    if (answered_field_is(interpretation, "request") && (interpretation->request[0] != '\0'))
    {
      capture(&state->fields[field_request], interpretation->request, 92);
      clear_confirmation(&state->fields[field_final_confirmed]);
    }
    if ((answered_field_is(interpretation, "callback_date") ||
         answered_field_is(interpretation, "callback_time") ||
         (state->last_requested == field_callback_date) ||
         (state->last_requested == field_callback_time)) &&
        (interpretation->callback_date[0] != '\0' || interpretation->callback_time[0] != '\0'))
    {
      clear_callback_fields(state);
      if (capture_callback_parts(state, interpretation->callback_date, interpretation->callback_time, interpretation->callback_time, 92))
      {
        clear_confirmation(&state->fields[field_final_confirmed]);
      }
    }
    if (answered_field_is(interpretation, "phone") && valid_callback_phone(interpretation->phone))
    {
      capture(&state->fields[field_phone], interpretation->phone, 94);
      clear_confirmation(&state->fields[field_phone_confirmed]);
      clear_confirmation(&state->fields[field_final_confirmed]);
    }
    if (answered_field_is(interpretation, "name") && should_capture_name(state, interpretation, caller_text))
    {
      capture(&state->fields[field_caller_name], interpretation->name, 92);
      clear_confirmation(&state->fields[field_last_name_spelling]);
      clear_confirmation(&state->fields[field_final_confirmed]);
    }
    if (answered_field_is(interpretation, "last_name_spelling") && (interpretation->spelling[0] != '\0'))
    {
      capture(&state->fields[field_last_name_spelling], interpretation->spelling, 92);
      clear_confirmation(&state->fields[field_final_confirmed]);
    }
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
      canonical_vehicle_text(canonical_vehicle, interpretation->vehicle, max_text);
      capture(&state->fields[field_vehicle], canonical_vehicle, 88);
    }
  }
  if (!is_captured(&state->fields[field_request]))
  {
    const Department effective_department =
      (department != department_unknown) ? department : state->department;
    if (!request_too_generic_for_department(effective_department, interpretation->request))
    {
      capture(&state->fields[field_request], interpretation->request, 88);
    }
  }
  if (!is_captured(&state->fields[field_callback_date]) ||
      !is_captured(&state->fields[field_callback_time]))
  {
    capture_callback_parts(
      state,
      interpretation->callback_date,
      interpretation->callback_time,
      interpretation->callback_time,
      88);
    if ((state->last_requested == field_callback_date) ||
        (state->last_requested == field_callback_time))
    {
      capture_callback_parts(state, caller_text, caller_text, caller_text, 82);
    }
  }
  if (!is_captured(&state->fields[field_phone]) && valid_callback_phone(interpretation->phone))
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

static int field_progress(const State* state, Field_id field)
{
  int progress = 0;
  if ((state == 0) || (field < field_department) || (field >= field_none))
  {
    return 0;
  }
  if (state->fields[field].status == status_captured)
  {
    progress = 1;
  }
  if (state->fields[field].confirmed)
  {
    progress += 1;
  }
  return progress;
}

static bool turn_counts_as_retry(const Interpretation* interpretation)
{
  if (interpretation == 0)
  {
    return false;
  }
  return
    !is_turn_type(interpretation, "caller_question") &&
    !is_turn_type(interpretation, "correction");
}

static Conversation_phase conversation_phase_for_state(const State* state)
{
  if (state == 0)
  {
    return conversation_phase_opening;
  }
  if (state->fields[field_final_confirmed].confirmed)
  {
    return conversation_phase_complete;
  }
  if (is_captured(&state->fields[field_callback_date]) ||
      is_captured(&state->fields[field_callback_time]) ||
      is_captured(&state->fields[field_phone]))
  {
    return conversation_phase_confirmation;
  }
  if (is_captured(&state->fields[field_request]))
  {
    return conversation_phase_contact;
  }
  if (is_captured(&state->fields[field_department]) ||
      is_captured(&state->fields[field_intent]))
  {
    return conversation_phase_discovery;
  }
  return conversation_phase_opening;
}

void merge_interpretation(State* state, const Interpretation* interpretation, const char* caller_text)
{
  Field_id requested = field_none;
  int before = 0;
  int after = 0;
  if ((state == 0) || (interpretation == 0))
  {
    return;
  }
  requested = state->last_requested;
  before = field_progress(state, requested);
  merge_interpretation_fields(state, interpretation, caller_text);
  after = field_progress(state, requested);
  state->history.turn_count += 1;
  state->history.caller_confused = is_turn_type(interpretation, "customer_confusion");
  if (is_turn_type(interpretation, "caller_question") ||
      is_turn_type(interpretation, "customer_confusion") ||
      is_turn_type(interpretation, "unclear_audio") ||
      is_turn_type(interpretation, "off_topic"))
  {
    state->history.interrupted_field = requested;
  }
  else
  {
    state->history.interrupted_field = field_none;
  }
  if ((requested >= field_department) && (requested < field_none))
  {
    if (after > before)
    {
      state->history.retry_counts[requested] = 0;
    }
    else if (turn_counts_as_retry(interpretation) &&
             (state->history.retry_counts[requested] < 9))
    {
      state->history.retry_counts[requested] += 1;
    }
  }
  state->history.phase = conversation_phase_for_state(state);
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
  else if (!is_captured(&state->fields[field_callback_date]))
  {
    plan.next_field = field_callback_date;
    plan.response_task = "Ask what date or day works best for a callback.";
    plan.fallback_sentence = "What date or day works best for a callback?";
  }
  else if (!is_captured(&state->fields[field_callback_time]))
  {
    plan.next_field = field_callback_time;
    plan.response_task = "Ask what time between 9 AM and 5 PM works best on the callback date.";
    plan.fallback_sentence = "What time between 9 AM and 5 PM works best that day?";
  }
  else if (!state->fields[field_callback_time].confirmed)
  {
    plan.next_field = field_callback_time;
    plan.response_task = "Read back the interpreted callback date and time and ask if it is correct.";
    plan.fallback_sentence = "Just to confirm, is that callback time correct?";
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
    case field_callback_date: result = "callback_date"; break;
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
    else if (contains_text(text, "callback_date")) { field = field_callback_date; }
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
  found = (json_value(json, "\"turn_type\"", interpretation->turn_type, 64) ||
           json_value(json, "\"tt\"", interpretation->turn_type, 64)) || found;
  found = (json_value(json, "\"answered_field\"", interpretation->answered_field, 64) ||
           json_value(json, "\"af\"", interpretation->answered_field, 64)) || found;
  found = (json_value(json, "\"meaning\"", interpretation->meaning, max_text) ||
           json_value(json, "\"m\"", interpretation->meaning, max_text)) || found;
  has_department = json_value(json, "\"d\"", interpretation->department, 32);
  has_intent = json_value(json, "\"i\"", interpretation->intent, max_text);
  has_request = json_value(json, "\"r\"", interpretation->request, max_text);
  found = has_department || has_intent || has_request;
  found = json_value(json, "\"v\"", interpretation->vehicle, max_text) || found;
  found = json_value(json, "\"cd\"", interpretation->callback_date, max_text) || found;
  found = json_value(json, "\"ct\"", interpretation->callback_time, max_text) || found;
  if (interpretation->callback_time[0] == '\0')
  {
    found = json_value(json, "\"cb\"", interpretation->callback_time, max_text) || found;
  }
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

static void append_json_int(char* output, int capacity, int value)
{
  char number[32];
  std::snprintf(number, sizeof(number), "%d", value);
  append(output, capacity, number);
}

static void append_json_int_array(char* output, int capacity, const int* values, int count)
{
  int index = 0;
  append(output, capacity, "[");
  while (index < count)
  {
    if (index > 0)
    {
      append(output, capacity, ",");
    }
    append_json_int(output, capacity, values[index]);
    index += 1;
  }
  append(output, capacity, "]");
}

static int json_int(const char* json, const char* key, int fallback)
{
  const char* found = 0;
  int value = 0;
  int sign = 1;
  bool any = false;
  if ((json == 0) || (key == 0))
  {
    return fallback;
  }
  found = std::strstr(json, key);
  if (found == 0)
  {
    return fallback;
  }
  found = std::strchr(found, ':');
  if (found == 0)
  {
    return fallback;
  }
  found += 1;
  while ((*found == ' ') || (*found == '\n') || (*found == '\r'))
  {
    found += 1;
  }
  if (*found == '-')
  {
    sign = -1;
    found += 1;
  }
  while ((*found >= '0') && (*found <= '9'))
  {
    value = (value * 10) + (*found - '0');
    any = true;
    found += 1;
  }
  return any ? (value * sign) : fallback;
}

static int json_int_array(const char* json, const char* key, int* values, int capacity)
{
  const char* found = 0;
  int count = 0;
  if ((json == 0) || (key == 0) || (values == 0) || (capacity <= 0))
  {
    return 0;
  }
  found = std::strstr(json, key);
  if (found == 0)
  {
    return 0;
  }
  found = std::strchr(found, '[');
  if (found == 0)
  {
    return 0;
  }
  found += 1;
  while ((*found != '\0') && (*found != ']') && (count < capacity))
  {
    int value = 0;
    int sign = 1;
    bool any = false;
    while ((*found == ' ') || (*found == ',') || (*found == '\n') || (*found == '\r'))
    {
      found += 1;
    }
    if (*found == '-')
    {
      sign = -1;
      found += 1;
    }
    while ((*found >= '0') && (*found <= '9'))
    {
      value = (value * 10) + (*found - '0');
      any = true;
      found += 1;
    }
    if (!any)
    {
      break;
    }
    values[count] = value * sign;
    count += 1;
  }
  return count;
}

static int bounded_int(int value, int minimum, int maximum)
{
  if (value < minimum)
  {
    return minimum;
  }
  if (value > maximum)
  {
    return maximum;
  }
  return value;
}

void state_to_json(const State* state, char* output, int capacity)
{
  clear_text(output);
  append(output, capacity, "{");
  append(output, capacity, "\"call_id\":");
  append_json_string(output, capacity, (state != 0) ? state->call_id : "");
  append(output, capacity, ",\"department\":");
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
  append(output, capacity, ",\"callback_date\":");
  append_json_string(output, capacity, (state != 0) ? state->fields[field_callback_date].value : "");
  append(output, capacity, ",\"callback_time\":");
  append_json_string(output, capacity, (state != 0) ? state->fields[field_callback_time].value : "");
  append(output, capacity, ",\"callback_time_confirmed\":");
  append(output, capacity, ((state != 0) && state->fields[field_callback_time].confirmed) ? "true" : "false");
  append(output, capacity, ",\"phone\":");
  append_json_string(output, capacity, (state != 0) ? state->fields[field_phone].value : "");
  append(output, capacity, ",\"phone_confirmed\":");
  append(output, capacity, ((state != 0) && state->fields[field_phone_confirmed].confirmed) ? "true" : "false");
  append(output, capacity, ",\"final_confirmed\":");
  append(output, capacity, ((state != 0) && state->fields[field_final_confirmed].confirmed) ? "true" : "false");
  append(output, capacity, ",\"last_requested\":");
  append_json_string(output, capacity, (state != 0) ? field_label(state->last_requested) : "none");
  append(output, capacity, ",\"delivery_sent\":");
  append(output, capacity, ((state != 0) && state->delivery_sent) ? "true" : "false");
  if (state != 0)
  {
    append(output, capacity, ",\"h\":{");
    append(output, capacity, "\"tc\":");
    append_json_int(output, capacity, state->history.turn_count);
    append(output, capacity, ",\"rr\":");
    append_json_int_array(output, capacity, state->history.retry_counts, tracked_field_count);
    append(output, capacity, ",\"rs\":");
    append_json_int_array(output, capacity, state->history.recent_structure_ids, state->history.recent_structure_count);
    append(output, capacity, ",\"rp\":");
    append_json_int_array(output, capacity, state->history.recent_phrase_ids, state->history.recent_phrase_count);
    append(output, capacity, ",\"la\":");
    append_json_int(output, capacity, state->history.last_response_act);
    append(output, capacity, ",\"ph\":");
    append_json_int(output, capacity, static_cast<int>(state->history.phase));
    append(output, capacity, ",\"if\":");
    append_json_int(output, capacity, static_cast<int>(state->history.interrupted_field));
    append(output, capacity, ",\"cp\":");
    append_json_int(output, capacity, static_cast<int>(state->history.caller_pace));
    append(output, capacity, ",\"cc\":");
    append(output, capacity, state->history.caller_confused ? "true" : "false");
    append(output, capacity, "}");
  }
  append(output, capacity, "}");
}

void load_state_from_json(State* state, const char* json)
{
  char value[max_text];
  Department department = department_unknown;
  int index = 0;
  if (state == 0)
  {
    return;
  }
  init_state(state);
  if (json == 0)
  {
    return;
  }
  if (json_value(json, "\"call_id\"", value, max_text))
  {
    copy_text(state->call_id, value, 64);
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
  if (json_value(json, "\"callback_date\"", value, max_text)) { capture(&state->fields[field_callback_date], value, 90); }
  if (json_value(json, "\"callback_time\"", value, max_text)) { capture(&state->fields[field_callback_time], value, 90); }
  if (json_bool(json, "\"callback_time_confirmed\""))
  {
    state->fields[field_callback_time].status = status_captured;
    state->fields[field_callback_time].confirmed = true;
  }
  if (json_value(json, "\"phone\"", value, max_text) && valid_callback_phone(value)) { capture(&state->fields[field_phone], value, 90); }
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
  state->delivery_sent = json_bool(json, "\"delivery_sent\"");
  if (json_value(json, "\"last_requested\"", value, max_text))
  {
    state->last_requested = field_from_text(value);
  }
  state->history.turn_count = bounded_int(json_int(json, "\"tc\"", 0), 0, 100000);
  (void)json_int_array(json, "\"rr\"", state->history.retry_counts, tracked_field_count);
  while (index < tracked_field_count)
  {
    state->history.retry_counts[index] = bounded_int(state->history.retry_counts[index], 0, 9);
    index += 1;
  }
  state->history.recent_structure_count = json_int_array(
    json,
    "\"rs\"",
    state->history.recent_structure_ids,
    max_recent_structures);
  state->history.recent_phrase_count = json_int_array(
    json,
    "\"rp\"",
    state->history.recent_phrase_ids,
    max_recent_phrase_ids);
  state->history.last_response_act = bounded_int(json_int(json, "\"la\"", 0), 0, 8);
  state->history.phase = static_cast<Conversation_phase>(bounded_int(json_int(json, "\"ph\"", 0), 0, 4));
  state->history.interrupted_field = static_cast<Field_id>(bounded_int(json_int(json, "\"if\"", static_cast<int>(field_none)), 0, static_cast<int>(field_none)));
  state->history.caller_pace = static_cast<Caller_pace>(bounded_int(json_int(json, "\"cp\"", 0), 0, 2));
  state->history.caller_confused = json_bool(json, "\"cc\"");
}

}
