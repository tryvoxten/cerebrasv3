#include <response_ai.h>
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

static void only_low_risk_acts_are_allowed(void)
{
  expect_true(cerebras_v3::ai_slot_act_allowed(cerebras_v3::response_act_acknowledge), "acknowledgement AI slot allowed");
  expect_true(cerebras_v3::ai_slot_act_allowed(cerebras_v3::response_act_transition), "transition AI slot allowed");
  expect_true(cerebras_v3::ai_slot_act_allowed(cerebras_v3::response_act_clarify), "clarification AI slot allowed");
  expect_true(!cerebras_v3::ai_slot_act_allowed(cerebras_v3::response_act_ask), "question AI slot blocked");
  expect_true(!cerebras_v3::ai_slot_act_allowed(cerebras_v3::response_act_readback), "readback AI slot blocked");
  expect_true(!cerebras_v3::ai_slot_act_allowed(cerebras_v3::response_act_answer), "dealership answer AI slot blocked");
  expect_true(!cerebras_v3::ai_slot_act_allowed(cerebras_v3::response_act_close), "close AI slot blocked");
}

static void prompt_contract_is_narrow_and_grounded(void)
{
  char system[2048];
  char user[2048];
  expect_true(
    cerebras_v3::build_ai_slot_prompts(
      cerebras_v3::response_act_acknowledge,
      cerebras_v3::field_caller_name,
      "{\"request\":\"clicking noise\"}",
      system,
      2048,
      user,
      2048),
    "acknowledgement prompt builds");
  expect_true(std::strstr(system, "Do not ask a question") != 0, "AI contract blocks questions");
  expect_true(std::strstr(system, "Use only facts explicitly present") != 0, "AI contract requires grounding");
  expect_true(std::strstr(system, "Maximum 12 words") != 0, "AI contract sets act word limit");
  expect_true(std::strstr(user, "Next field: name") != 0, "AI prompt includes next field");
  expect_true(std::strstr(user, "clicking noise") != 0, "AI prompt includes known state");
  expect_true(
    !cerebras_v3::build_ai_slot_prompts(
      cerebras_v3::response_act_ask,
      cerebras_v3::field_phone,
      "{}",
      system,
      2048,
      user,
      2048),
    "AI question prompt cannot be built");
}
}

int main(void)
{
  only_low_risk_acts_are_allowed();
  prompt_contract_is_narrow_and_grounded();
  if (failures == 0)
  {
    (void)write(1, "response_ai_tests: PASS\n", 24U);
  }
  return failures == 0 ? 0 : 1;
}
