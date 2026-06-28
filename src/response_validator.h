#ifndef cerebras_v3_response_validator_h
#define cerebras_v3_response_validator_h

#include <response_ai.h>

namespace cerebras_v3
{
enum Response_validation_error
{
  response_validation_ok = 0,
  response_validation_empty = 1,
  response_validation_question_count = 2,
  response_validation_banned_wording = 3,
  response_validation_unsafe_claim = 4,
  response_validation_too_long = 5,
  response_validation_missing_readback = 6,
  response_validation_repeated = 7,
  response_validation_ungrounded = 8,
  response_validation_disallowed_act = 9
};

struct Response_validation_result
{
  bool valid;
  Response_validation_error error;
};

Response_validation_result validate_ai_slot(
  const char* text,
  Response_act act,
  const State* state);
Response_validation_result validate_composed_response(
  const char* text,
  const Response_plan* plan,
  const State* state,
  const char* previous_response);
const char* response_validation_error_label(Response_validation_error error);
}

#endif
