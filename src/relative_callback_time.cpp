#include <relative_callback_time.h>
#include <planner.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace
{

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

static int spoken_number_value(const char* token)
{
  if (token == 0) { return 0; }
  if (std::strcmp(token, "one") == 0) { return 1; }
  if (std::strcmp(token, "two") == 0) { return 2; }
  if (std::strcmp(token, "three") == 0) { return 3; }
  if (std::strcmp(token, "four") == 0) { return 4; }
  if (std::strcmp(token, "five") == 0) { return 5; }
  if (std::strcmp(token, "six") == 0) { return 6; }
  if (std::strcmp(token, "seven") == 0) { return 7; }
  if (std::strcmp(token, "eight") == 0) { return 8; }
  if (std::strcmp(token, "nine") == 0) { return 9; }
  if (std::strcmp(token, "ten") == 0) { return 10; }
  if (std::strcmp(token, "eleven") == 0) { return 11; }
  if (std::strcmp(token, "twelve") == 0) { return 12; }
  if (std::strcmp(token, "thirteen") == 0) { return 13; }
  if (std::strcmp(token, "fourteen") == 0) { return 14; }
  if (std::strcmp(token, "fifteen") == 0) { return 15; }
  if (std::strcmp(token, "sixteen") == 0) { return 16; }
  if (std::strcmp(token, "seventeen") == 0) { return 17; }
  if (std::strcmp(token, "eighteen") == 0) { return 18; }
  if (std::strcmp(token, "nineteen") == 0) { return 19; }
  if (std::strcmp(token, "twenty") == 0) { return 20; }
  if (std::strcmp(token, "thirty") == 0) { return 30; }
  if (std::strcmp(token, "forty") == 0) { return 40; }
  if (std::strcmp(token, "fifty") == 0) { return 50; }
  return 0;
}

static int parse_numeric_token(const char* token)
{
  int value = 0;
  int index = 0;
  if ((token == 0) || (token[0] == '\0'))
  {
    return 0;
  }
  if ((token[0] < '0') || (token[0] > '9'))
  {
    return 0;
  }
  while ((token[index] >= '0') && (token[index] <= '9'))
  {
    value = (value * 10) + (token[index] - '0');
    index += 1;
  }
  return token[index] == '\0' ? value : 0;
}

static void token_before_position(
  const char* lowered,
  int before,
  char* output,
  int capacity,
  int* token_start)
{
  int end = before;
  int start = 0;
  int index = 0;
  if ((lowered == 0) || (output == 0) || (capacity <= 0) || (token_start == 0))
  {
    return;
  }
  clear_buffer(output, capacity);
  *token_start = before;
  while ((end > 0) && (lowered[end - 1] == ' '))
  {
    end -= 1;
  }
  start = end;
  while ((start > 0) &&
         (std::isalnum(static_cast<unsigned char>(lowered[start - 1])) != 0))
  {
    start -= 1;
  }
  while ((start + index < end) && (index < (capacity - 1)))
  {
    output[index] = lowered[start + index];
    index += 1;
  }
  output[index] = '\0';
  *token_start = start;
}

static int relative_quantity_before(const char* lowered, const char* unit)
{
  const char* found = 0;
  int end = 0;
  int last_start = 0;
  int previous_start = 0;
  int last_value = 0;
  int previous_value = 0;
  char last_token[16];
  char previous_token[16];
  clear_buffer(last_token, 16);
  clear_buffer(previous_token, 16);
  if ((lowered == 0) || (unit == 0))
  {
    return 0;
  }
  found = std::strstr(lowered, unit);
  if (found == 0)
  {
    return 0;
  }
  end = static_cast<int>(found - lowered);
  while ((end > 0) && (lowered[end - 1] == ' '))
  {
    end -= 1;
  }
  token_before_position(lowered, end, last_token, 16, &last_start);
  last_value = parse_numeric_token(last_token);
  if (last_value == 0)
  {
    last_value = spoken_number_value(last_token);
  }
  if ((last_value >= 1) && (last_value <= 52))
  {
    token_before_position(lowered, last_start, previous_token, 16, &previous_start);
    previous_value = spoken_number_value(previous_token);
    if (((previous_value == 20) || (previous_value == 30) ||
         (previous_value == 40) || (previous_value == 50)) &&
        (last_value >= 1) && (last_value <= 9))
    {
      return ((previous_value + last_value) <= 52) ? (previous_value + last_value) : 0;
    }
    return last_value;
  }
  return 0;
}

static int weekday_index_from_name(const char* lowered)
{
  if (lowered == 0)
  {
    return -1;
  }
  if (std::strstr(lowered, "sunday") != 0) { return 0; }
  if (std::strstr(lowered, "monday") != 0) { return 1; }
  if (std::strstr(lowered, "tuesday") != 0) { return 2; }
  if (std::strstr(lowered, "wednesday") != 0) { return 3; }
  if (std::strstr(lowered, "thursday") != 0) { return 4; }
  if (std::strstr(lowered, "friday") != 0) { return 5; }
  if (std::strstr(lowered, "saturday") != 0) { return 6; }
  return -1;
}

static int current_weekday_index(
  int reference_year,
  int reference_month,
  int reference_day)
{
  std::tm reference;
  std::memset(&reference, 0, sizeof(reference));
  reference.tm_year = reference_year - 1900;
  reference.tm_mon = reference_month - 1;
  reference.tm_mday = reference_day;
  reference.tm_hour = 12;
  reference.tm_isdst = -1;
  if (std::mktime(&reference) == static_cast<std::time_t>(-1))
  {
    return -1;
  }
  return reference.tm_wday;
}

static int weekday_callback_day_offset(
  const char* lowered,
  int reference_year,
  int reference_month,
  int reference_day)
{
  const int target = weekday_index_from_name(lowered);
  const int current = current_weekday_index(
    reference_year,
    reference_month,
    reference_day);
  int offset = 0;
  if ((target < 0) || (current < 0))
  {
    return 0;
  }
  offset = (target - current + 7) % 7;
  if (offset == 0)
  {
    offset = 7;
  }
  return offset;
}

static int relative_callback_day_offset(
  const char* lowered,
  int reference_year,
  int reference_month,
  int reference_day)
{
  const bool relative_marker =
    (lowered != 0) &&
    ((std::strstr(lowered, "from now") != 0) ||
     (std::strstr(lowered, " week today") != 0) ||
     (std::strstr(lowered, " weeks today") != 0) ||
     (std::strncmp(lowered, "in ", 3) == 0) ||
     (std::strstr(lowered, " in ") != 0));
  int quantity = 0;
  if (lowered == 0)
  {
    return 0;
  }
  if ((std::strstr(lowered, "day after tomorrow") != 0) ||
      (std::strstr(lowered, "after tomorrow") != 0))
  {
    return 2;
  }
  if (std::strstr(lowered, "tomorrow") != 0)
  {
    return 1;
  }
  quantity = weekday_callback_day_offset(
    lowered,
    reference_year,
    reference_month,
    reference_day);
  if (quantity > 0)
  {
    return quantity;
  }
  if (!relative_marker)
  {
    return 0;
  }
  quantity = relative_quantity_before(lowered, " week");
  if (quantity > 0)
  {
    return quantity * 7;
  }
  quantity = relative_quantity_before(lowered, " day");
  return quantity;
}

static void normalize_spoken_callback_hour(
  const char* input,
  char* output,
  int capacity)
{
  char lowered[64];
  char token[16];
  char hour_text[4];
  int index = 0;
  int hour = 0;
  clear_buffer(output, capacity);
  clear_buffer(lowered, 64);
  clear_buffer(token, 16);
  clear_buffer(hour_text, 4);
  if ((input == 0) || (output == 0) || (capacity <= 0))
  {
    return;
  }
  lowercase_callback_text(lowered, input, 64);
  while ((lowered[index] != '\0') &&
         (lowered[index] != ' ') &&
         (index < 15))
  {
    token[index] = lowered[index];
    index += 1;
  }
  token[index] = '\0';
  hour = spoken_number_value(token);
  if (hour == 0)
  {
    cerebras_v3::copy_text(output, input, capacity);
    return;
  }
  std::snprintf(hour_text, sizeof(hour_text), "%d", hour);
  append_text(output, capacity, hour_text);
  append_text(output, capacity, &input[index]);
}

static int bare_business_hour_period(int hour)
{
  if ((hour >= 9) && (hour <= 11))
  {
    return 1;
  }
  if ((hour == 12) || ((hour >= 1) && (hour <= 5)))
  {
    return 2;
  }
  return 0;
}

static bool parse_bare_hour_after_marker(
  const char* lowered,
  const char* marker,
  int* hour)
{
  const char* found = 0;
  int index = 0;
  char token[16];
  int value = 0;
  if ((lowered == 0) || (marker == 0) || (hour == 0))
  {
    return false;
  }
  found = std::strstr(lowered, marker);
  if (found == 0)
  {
    return false;
  }
  found += std::strlen(marker);
  while ((*found == ' ') || (*found == '\t'))
  {
    found += 1;
  }
  clear_buffer(token, 16);
  while ((found[index] != '\0') &&
         (std::isalnum(static_cast<unsigned char>(found[index])) != 0) &&
         (index < 15))
  {
    token[index] = found[index];
    index += 1;
  }
  token[index] = '\0';
  if (token[0] == '\0')
  {
    return false;
  }
  if ((token[0] >= '0') && (token[0] <= '9'))
  {
    index = 0;
    while ((token[index] >= '0') && (token[index] <= '9'))
    {
      value = (value * 10) + (token[index] - '0');
      index += 1;
    }
  }
  else
  {
    value = spoken_number_value(token);
  }
  if (bare_business_hour_period(value) == 0)
  {
    return false;
  }
  *hour = value;
  return true;
}

static bool copy_bare_business_hour_window(
  const char* lowered,
  char* output,
  int capacity,
  bool* has_preposition)
{
  int hour = 0;
  int period = 0;
  char hour_text[8];
  const char* prefix = "";
  if ((lowered == 0) || (output == 0) || (capacity <= 0) || (has_preposition == 0))
  {
    return false;
  }
  clear_buffer(hour_text, 8);
  if (parse_bare_hour_after_marker(lowered, "at ", &hour))
  {
    prefix = "";
    *has_preposition = false;
  }
  else if (parse_bare_hour_after_marker(lowered, "around ", &hour))
  {
    prefix = "around ";
    *has_preposition = true;
  }
  else if (parse_bare_hour_after_marker(lowered, "after ", &hour))
  {
    prefix = "after ";
    *has_preposition = true;
  }
  else if (parse_bare_hour_after_marker(lowered, "before ", &hour))
  {
    prefix = "before ";
    *has_preposition = true;
  }
  else
  {
    return false;
  }
  period = bare_business_hour_period(hour);
  if (period == 0)
  {
    return false;
  }
  std::snprintf(hour_text, sizeof(hour_text), "%d", hour);
  append_text(output, capacity, prefix);
  append_text(output, capacity, hour_text);
  append_text(output, capacity, (period == 1) ? " AM" : " PM");
  return true;
}

static bool extract_relative_callback_window(
  const char* input,
  const char* lowered,
  char* output,
  int capacity,
  bool* has_preposition)
{
  const char* marker = 0;
  char extracted[64];
  int end = 0;
  int start = 0;
  int length = 0;
  if ((input == 0) || (lowered == 0) || (output == 0) ||
      (capacity <= 0) || (has_preposition == 0))
  {
    return false;
  }
  clear_buffer(output, capacity);
  clear_buffer(extracted, 64);
  *has_preposition = false;
  marker = std::strstr(lowered, " am");
  if (marker == 0)
  {
    marker = std::strstr(lowered, " pm");
  }
  if (marker != 0)
  {
    end = static_cast<int>((marker - lowered) + 3);
    start = static_cast<int>(marker - lowered);
    while ((start > 0) && (lowered[start - 1] != ' '))
    {
      start -= 1;
    }
    length = end - start;
    if ((length >= capacity) || (length >= 64))
    {
      return false;
    }
    std::memcpy(extracted, &input[start], static_cast<unsigned long>(length));
    extracted[length] = '\0';
    normalize_spoken_callback_hour(extracted, output, capacity);
    return true;
  }
  if (copy_bare_business_hour_window(lowered, output, capacity, has_preposition))
  {
    return true;
  }
  if (std::strstr(lowered, "morning") != 0)
  {
    cerebras_v3::copy_text(output, "in the morning", capacity);
    *has_preposition = true;
    return true;
  }
  if (std::strstr(lowered, "afternoon") != 0)
  {
    cerebras_v3::copy_text(output, "in the afternoon", capacity);
    *has_preposition = true;
    return true;
  }
  if ((std::strstr(lowered, "after lunch") != 0) ||
      (std::strstr(lowered, "after lunchtime") != 0))
  {
    cerebras_v3::copy_text(output, "after lunch", capacity);
    *has_preposition = true;
    return true;
  }
  if ((std::strstr(lowered, "noon") != 0) ||
      (std::strstr(lowered, "lunchtime") != 0))
  {
    cerebras_v3::copy_text(output, "at noon", capacity);
    *has_preposition = true;
    return true;
  }
  return false;
}

}

namespace cerebras_v3
{

bool resolve_relative_callback_time_from_date(
  const char* input,
  int reference_year,
  int reference_month,
  int reference_day,
  char* output,
  int capacity)
{
  char lowered[cerebras_v3::max_text];
  char date_text[96];
  char window[64];
  std::tm target;
  int day_offset = 0;
  bool window_has_preposition = false;
  if ((input == 0) || (output == 0) || (capacity <= 0))
  {
    return false;
  }
  clear_buffer(output, capacity);
  clear_buffer(lowered, cerebras_v3::max_text);
  clear_buffer(date_text, 96);
  clear_buffer(window, 64);
  lowercase_callback_text(lowered, input, cerebras_v3::max_text);
  day_offset = relative_callback_day_offset(
    lowered,
    reference_year,
    reference_month,
    reference_day);
  if ((day_offset <= 0) ||
      !extract_relative_callback_window(
        input,
        lowered,
        window,
        64,
        &window_has_preposition))
  {
    return false;
  }
  std::memset(&target, 0, sizeof(target));
  target.tm_year = reference_year - 1900;
  target.tm_mon = reference_month - 1;
  target.tm_mday = reference_day + day_offset;
  target.tm_hour = 12;
  target.tm_isdst = -1;
  if (std::mktime(&target) == static_cast<std::time_t>(-1))
  {
    return false;
  }
  if (std::strftime(
        date_text,
        static_cast<unsigned long>(sizeof(date_text)),
        "%A, %B %d, %Y",
        &target) == 0U)
  {
    return false;
  }
  append_text(output, capacity, date_text);
  append_text(output, capacity, window_has_preposition ? " " : " at ");
  append_text(output, capacity, window);
  return output[0] != '\0';
}

bool resolve_relative_callback_time(
  const char* input,
  char* output,
  int capacity)
{
  const std::time_t now = std::time(0);
  const std::tm* local_time = std::localtime(&now);
  if (local_time == 0)
  {
    return false;
  }
  return resolve_relative_callback_time_from_date(
    input,
    local_time->tm_year + 1900,
    local_time->tm_mon + 1,
    local_time->tm_mday,
    output,
    capacity);
}

}
