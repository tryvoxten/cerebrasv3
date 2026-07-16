#ifndef cerebras_v3_relative_callback_time_h
#define cerebras_v3_relative_callback_time_h

namespace cerebras_v3
{

bool resolve_relative_callback_time_from_date(
  const char* input,
  int reference_year,
  int reference_month,
  int reference_day,
  char* output,
  int capacity);

bool resolve_relative_callback_time(
  const char* input,
  char* output,
  int capacity);

}

#endif
