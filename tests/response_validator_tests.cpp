#include <response_validator.h>
#include <unistd.h>
#include <cstring>

namespace
{
static int failures = 0;

static void expect_true(bool value, const char* label)
{
  if (!value)
  {
    failures += 1;
    (void)write(1, label, std::strlen(label));
    (void)write(1, "\n", 1U);
  }
}

static void ai_slots_require_safety_and_grounding(void)
{
  cerebras_v3::State state;
  cerebras_v3::Response_validation_result result;
  cerebras_v3::init_state(&state);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_request].value, "clicking noise", cerebras_v3::max_text);
  result = cerebras_v3::validate_ai_slot("I have the clicking noise noted.", cerebras_v3::response_act_acknowledge, &state);
  expect_true(result.valid, "grounded acknowledgement passes");
  result = cerebras_v3::validate_ai_slot("I have the urgent repair noted.", cerebras_v3::response_act_acknowledge, &state);
  expect_true(result.error == cerebras_v3::response_validation_ungrounded, "invented acknowledgement fails grounding");
  result = cerebras_v3::validate_ai_slot("Will Tuesday work?", cerebras_v3::response_act_transition, &state);
  expect_true(result.error == cerebras_v3::response_validation_question_count, "AI slot question is rejected");
  result = cerebras_v3::validate_ai_slot("It will cost $500.", cerebras_v3::response_act_clarify, &state);
  expect_true(result.error == cerebras_v3::response_validation_unsafe_claim, "AI slot price claim is rejected");
  result = cerebras_v3::validate_ai_slot("Who should the team ask for?", cerebras_v3::response_act_ask, &state);
  expect_true(result.error == cerebras_v3::response_validation_disallowed_act, "AI question act is rejected");
}

static void composed_response_enforces_structure_and_readback(void)
{
  cerebras_v3::State state;
  cerebras_v3::Response_plan plan;
  cerebras_v3::Response_validation_result result;
  cerebras_v3::init_state(&state);
  cerebras_v3::init_response_plan(&plan, cerebras_v3::field_callback_time);
  plan.structure = cerebras_v3::response_structure_readback_ask;
  (void)cerebras_v3::append_response_act(&plan, cerebras_v3::response_act_readback);
  (void)cerebras_v3::append_response_act(&plan, cerebras_v3::response_act_ask);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_time].value, "Friday afternoon", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_callback_time].status = cerebras_v3::status_captured;
  result = cerebras_v3::validate_composed_response("I have Friday afternoon. Is that correct?", &plan, &state, "");
  expect_true(result.valid, "exact callback readback passes");
  result = cerebras_v3::validate_composed_response("I have Tuesday morning. Is that correct?", &plan, &state, "");
  expect_true(result.error == cerebras_v3::response_validation_missing_readback, "stale callback readback fails");
  result = cerebras_v3::validate_composed_response("I have Friday afternoon. Is that correct? What number should we call?", &plan, &state, "");
  expect_true(result.error == cerebras_v3::response_validation_question_count, "multiple questions fail");
}

static void composed_response_blocks_repetition_and_claims(void)
{
  cerebras_v3::State state;
  cerebras_v3::Response_plan plan;
  cerebras_v3::Response_validation_result result;
  cerebras_v3::init_state(&state);
  cerebras_v3::init_response_plan(&plan, cerebras_v3::field_caller_name);
  (void)cerebras_v3::append_response_act(&plan, cerebras_v3::response_act_ask);
  result = cerebras_v3::validate_composed_response(
    "Who should the team ask for?",
    &plan,
    &state,
    "Who should the team ask for!");
  expect_true(result.error == cerebras_v3::response_validation_repeated, "normalized repeated response fails");
  result = cerebras_v3::validate_composed_response(
    "The part is available. Who should the team ask for?",
    &plan,
    &state,
    "");
  expect_true(result.error == cerebras_v3::response_validation_unsafe_claim, "availability claim fails");
  result = cerebras_v3::validate_composed_response(
    "For our records, who should the team ask for?",
    &plan,
    &state,
    "");
  expect_true(result.error == cerebras_v3::response_validation_banned_wording, "banned wording fails");
}
}

int main(void)
{
  ai_slots_require_safety_and_grounding();
  composed_response_enforces_structure_and_readback();
  composed_response_blocks_repetition_and_claims();
  if (failures == 0)
  {
    (void)write(1, "response_validator_tests: PASS\n", 31U);
  }
  return failures == 0 ? 0 : 1;
}
