#include <conversation_integrity.h>
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

static void multi_question_turn_is_detected(void)
{
  expect_true(
    cerebras_v3::caller_turn_has_multiple_questions("Do you have loaners and what time do you close?"),
    "multi-question caller turn detected");
}

static void missed_answer_repair_is_detected(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.turn_type, "field_answer", 64);
  expect_true(
    cerebras_v3::should_repair_missed_answer(
      &state,
      cerebras_v3::field_phone,
      &interpretation,
      "six four seven five five five zero one two three"),
    "probable missed phone answer detected");
}

static void response_shape_issues_are_detected(void)
{
  expect_true(
    cerebras_v3::conversation_response_has_multiple_questions("What time? What day?"),
    "multi-question response detected");
  expect_true(
    cerebras_v3::conversation_response_is_incomplete("I will pass this to the team"),
    "incomplete response detected");
}
}

int main(void)
{
  multi_question_turn_is_detected();
  missed_answer_repair_is_detected();
  response_shape_issues_are_detected();
  if (failures == 0)
  {
    (void)write(1, "conversation_integrity_tests: PASS\n", 35U);
  }
  return failures == 0 ? 0 : 1;
}
