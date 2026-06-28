#ifndef cerebras_v3_response_ai_h
#define cerebras_v3_response_ai_h

#include <response_policy.h>

namespace cerebras_v3
{
bool ai_slot_act_allowed(Response_act act);
int ai_slot_word_limit(Response_act act);
bool build_ai_slot_prompts(
  Response_act act,
  Field_id target_field,
  const char* state_json,
  char* system_prompt,
  int system_capacity,
  char* user_prompt,
  int user_capacity);
}

#endif
