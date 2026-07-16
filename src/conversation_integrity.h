#ifndef cerebras_v3_conversation_integrity_h
#define cerebras_v3_conversation_integrity_h

#include <planner.h>

namespace cerebras_v3
{

int conversation_question_count(const char* text);
bool caller_turn_has_multiple_questions(const char* text);
bool caller_probably_answered_field(Field_id field, const char* caller_text);
bool conversation_response_is_incomplete(const char* response_text);
bool conversation_response_has_multiple_questions(const char* response_text);
bool should_repair_missed_answer(
  const State* state,
  Field_id previous_requested,
  const Interpretation* interpretation,
  const char* caller_text);
void conversation_integrity_issues(
  const State* state,
  Field_id previous_requested,
  const Interpretation* interpretation,
  const Plan* plan,
  const char* caller_text,
  const char* response_text,
  bool end_call,
  char* output,
  int capacity);

}

#endif
