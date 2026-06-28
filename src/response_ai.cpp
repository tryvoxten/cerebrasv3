#include <response_ai.h>
#include <cstring>
#include <cstdio>

namespace cerebras_v3
{
static void clear_text(char* text, int capacity)
{
  if ((text != 0) && (capacity > 0))
  {
    text[0] = '\0';
  }
}

static void append_text(char* output, int capacity, const char* text)
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

bool ai_slot_act_allowed(Response_act act)
{
  return
    (act == response_act_acknowledge) ||
    (act == response_act_transition) ||
    (act == response_act_clarify);
}

int ai_slot_word_limit(Response_act act)
{
  switch (act)
  {
    case response_act_acknowledge: return 12;
    case response_act_transition: return 6;
    case response_act_clarify: return 14;
    default: return 0;
  }
}

bool build_ai_slot_prompts(
  Response_act act,
  Field_id target_field,
  const char* state_json,
  char* system_prompt,
  int system_capacity,
  char* user_prompt,
  int user_capacity)
{
  char word_limit[16];
  if (!ai_slot_act_allowed(act) ||
      (system_prompt == 0) ||
      (user_prompt == 0) ||
      (system_capacity <= 0) ||
      (user_capacity <= 0))
  {
    return false;
  }
  clear_text(system_prompt, system_capacity);
  clear_text(user_prompt, user_capacity);
  std::snprintf(word_limit, sizeof(word_limit), "%d", ai_slot_word_limit(act));
  append_text(
    system_prompt,
    system_capacity,
    "You write one low-risk spoken phrase for a dealership after-hours assistant. "
    "Use only facts explicitly present in the supplied state. Do not ask a question. "
    "Do not mention or invent prices, availability, diagnosis, appointments, financing, "
    "timelines, names, phone numbers, dates, or dealership policy. No greeting, markdown, "
    "quotation marks, titles, or internal-process language. Output only the phrase. Maximum ");
  append_text(system_prompt, system_capacity, word_limit);
  append_text(system_prompt, system_capacity, " words.");

  append_text(user_prompt, user_capacity, "Act: ");
  append_text(user_prompt, user_capacity, response_act_label(act));
  append_text(user_prompt, user_capacity, "\nNext field: ");
  append_text(user_prompt, user_capacity, field_label(target_field));
  append_text(user_prompt, user_capacity, "\nKnown state: ");
  append_text(user_prompt, user_capacity, (state_json != 0) ? state_json : "{}");
  if (act == response_act_acknowledge)
  {
    append_text(user_prompt, user_capacity, "\nAcknowledge one captured request or vehicle fact without adding interpretation.");
  }
  else if (act == response_act_transition)
  {
    append_text(user_prompt, user_capacity, "\nWrite a short bridge into the next field. Do not request the field yourself.");
  }
  else
  {
    append_text(user_prompt, user_capacity, "\nBriefly explain what kind of answer is needed. Do not ask the follow-up question.");
  }
  return true;
}
}
