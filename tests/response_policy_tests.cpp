#include <response_policy.h>
#include <unistd.h>
#include <cstring>

namespace
{
static int failures = 0;

static void write_line(const char* text)
{
  if (text != 0)
  {
    (void)write(1, text, std::strlen(text));
    (void)write(1, "\n", 1U);
  }
}

static void expect_true(bool value, const char* label)
{
  if (!value)
  {
    failures += 1;
    write_line(label);
  }
}

static void initializes_empty_plan(void)
{
  cerebras_v3::Response_plan plan;
  cerebras_v3::init_response_plan(&plan, cerebras_v3::field_caller_name);
  expect_true(plan.act_count == 0, "response plan starts without acts");
  expect_true(plan.target_field == cerebras_v3::field_caller_name, "response plan stores target field");
  expect_true(plan.structure == cerebras_v3::response_structure_none, "response plan starts without structure");
  expect_true(plan.retry_count == 0, "response plan starts without retries");
  expect_true(!plan.requires_question, "response plan starts without question requirement");
  expect_true(!plan.complete, "response plan starts incomplete");
}

static void appends_acts_in_order(void)
{
  cerebras_v3::Response_plan plan;
  cerebras_v3::init_response_plan(&plan, cerebras_v3::field_request);
  expect_true(cerebras_v3::append_response_act(&plan, cerebras_v3::response_act_acknowledge), "acknowledge act appends");
  expect_true(cerebras_v3::append_response_act(&plan, cerebras_v3::response_act_transition), "transition act appends");
  expect_true(cerebras_v3::append_response_act(&plan, cerebras_v3::response_act_ask), "ask act appends");
  expect_true(plan.act_count == 3, "three acts stored");
  expect_true(plan.acts[0] == cerebras_v3::response_act_acknowledge, "first act remains acknowledge");
  expect_true(plan.acts[1] == cerebras_v3::response_act_transition, "second act remains transition");
  expect_true(plan.acts[2] == cerebras_v3::response_act_ask, "third act remains ask");
  expect_true(plan.requires_question, "ask act requires question");
}

static void rejects_invalid_or_excess_acts(void)
{
  cerebras_v3::Response_plan plan;
  cerebras_v3::init_response_plan(&plan, cerebras_v3::field_phone);
  expect_true(!cerebras_v3::append_response_act(&plan, cerebras_v3::response_act_none), "none act is rejected");
  expect_true(cerebras_v3::append_response_act(&plan, cerebras_v3::response_act_confirm_correction), "first valid act appends");
  expect_true(cerebras_v3::append_response_act(&plan, cerebras_v3::response_act_readback), "second valid act appends");
  expect_true(cerebras_v3::append_response_act(&plan, cerebras_v3::response_act_transition), "third valid act appends");
  expect_true(cerebras_v3::append_response_act(&plan, cerebras_v3::response_act_ask), "fourth valid act appends");
  expect_true(!cerebras_v3::append_response_act(&plan, cerebras_v3::response_act_close), "act beyond capacity is rejected");
}

static void labels_are_stable(void)
{
  expect_true(std::strcmp(cerebras_v3::response_act_label(cerebras_v3::response_act_confirm_correction), "confirm_correction") == 0, "correction act label is stable");
  expect_true(std::strcmp(cerebras_v3::response_structure_label(cerebras_v3::response_structure_confirm_correction_readback_ask), "confirm_correction_readback_ask") == 0, "combined correction structure label is stable");
  expect_true(std::strcmp(cerebras_v3::response_structure_label(cerebras_v3::response_structure_close), "close") == 0, "close structure label is stable");
}

static void response_history_tracks_recent_choices(void)
{
  cerebras_v3::State state;
  cerebras_v3::init_state(&state);
  cerebras_v3::record_response_structure(&state, cerebras_v3::response_structure_ask);
  cerebras_v3::record_response_structure(&state, cerebras_v3::response_structure_acknowledge_ask);
  cerebras_v3::record_response_structure(&state, cerebras_v3::response_structure_clarify_ask);
  cerebras_v3::record_response_structure(&state, cerebras_v3::response_structure_close);
  expect_true(!cerebras_v3::response_structure_recently_used(&state, cerebras_v3::response_structure_ask), "oldest structure leaves recent window");
  expect_true(cerebras_v3::response_structure_recently_used(&state, cerebras_v3::response_structure_close), "latest structure stays in recent window");
  cerebras_v3::record_response_phrase(&state, 101);
  cerebras_v3::record_response_phrase(&state, 202);
  expect_true(cerebras_v3::response_phrase_recently_used(&state, 101), "recorded phrase is found");
  expect_true(!cerebras_v3::response_phrase_recently_used(&state, 303), "unrecorded phrase is absent");
}

static void prepare_context(
  cerebras_v3::State* state,
  cerebras_v3::Interpretation* interpretation,
  cerebras_v3::Plan* field_plan,
  cerebras_v3::Response_context* context,
  cerebras_v3::Field_id target)
{
  cerebras_v3::init_state(state);
  cerebras_v3::clear_interpretation(interpretation);
  field_plan->next_field = target;
  field_plan->response_task = "test";
  field_plan->fallback_sentence = "test";
  field_plan->complete = false;
  cerebras_v3::init_response_context(context);
  context->state = state;
  context->field_plan = field_plan;
  context->interpretation = interpretation;
}

static void catalog_defines_all_structures(void)
{
  int index = 0;
  expect_true(cerebras_v3::response_structure_catalog_size() == 10, "catalog contains ten structures");
  while (index < cerebras_v3::response_structure_catalog_size())
  {
    const cerebras_v3::Response_structure_definition* definition =
      cerebras_v3::response_structure_definition_at(index);
    expect_true(definition != 0, "catalog entry exists");
    expect_true((definition != 0) && (definition->act_count > 0), "catalog entry contains acts");
    expect_true((definition != 0) && (cerebras_v3::find_response_structure_definition(definition->structure) == definition), "catalog entry is findable");
    index += 1;
  }
  expect_true(cerebras_v3::response_structure_definition_at(-1) == 0, "negative catalog index is rejected");
  expect_true(cerebras_v3::response_structure_definition_at(10) == 0, "large catalog index is rejected");
}

static void normal_turn_allows_only_grounded_normal_structures(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan field_plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_structure eligible[10];
  int count = 0;
  prepare_context(&state, &interpretation, &field_plan, &context, cerebras_v3::field_caller_name);
  cerebras_v3::copy_text(interpretation.turn_type, "field_answer", 64);
  count = cerebras_v3::collect_eligible_response_structures(&context, eligible, 10);
  expect_true(count == 1, "normal turn without acknowledgement has one structure");
  expect_true(eligible[0] == cerebras_v3::response_structure_ask, "normal turn always allows ask");
  context.has_grounded_acknowledgement = true;
  count = cerebras_v3::collect_eligible_response_structures(&context, eligible, 10);
  expect_true(count == 3, "grounded normal turn has three structures");
  expect_true(cerebras_v3::response_structure_eligible(cerebras_v3::response_structure_acknowledge_ask, &context), "grounded acknowledgement structure is eligible");
  expect_true(cerebras_v3::response_structure_eligible(cerebras_v3::response_structure_acknowledge_transition_ask, &context), "grounded transition structure is eligible");
}

static void special_turns_enforce_special_structures(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan field_plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_structure eligible[10];
  int count = 0;
  prepare_context(&state, &interpretation, &field_plan, &context, cerebras_v3::field_caller_name);
  cerebras_v3::copy_text(interpretation.turn_type, "caller_question", 64);
  count = cerebras_v3::collect_eligible_response_structures(&context, eligible, 10);
  expect_true(count == 2, "caller question has two answer structures");
  expect_true(eligible[0] == cerebras_v3::response_structure_answer_ask, "caller question starts with answer ask");
  expect_true(eligible[1] == cerebras_v3::response_structure_answer_transition_ask, "caller question allows answer transition ask");

  cerebras_v3::copy_text(interpretation.turn_type, "customer_confusion", 64);
  count = cerebras_v3::collect_eligible_response_structures(&context, eligible, 10);
  expect_true(count == 1, "confusion has one repair structure");
  expect_true(eligible[0] == cerebras_v3::response_structure_clarify_ask, "confusion requires clarification");

  cerebras_v3::copy_text(interpretation.turn_type, "correction", 64);
  cerebras_v3::copy_text(interpretation.answered_field, "request", 64);
  count = cerebras_v3::collect_eligible_response_structures(&context, eligible, 10);
  expect_true(count == 1, "ordinary correction has one correction structure");
  expect_true(eligible[0] == cerebras_v3::response_structure_confirm_correction_ask, "ordinary correction confirms then asks");

  field_plan.next_field = cerebras_v3::field_phone_confirmed;
  cerebras_v3::copy_text(interpretation.answered_field, "phone", 64);
  count = cerebras_v3::collect_eligible_response_structures(&context, eligible, 10);
  expect_true(count == 1, "phone correction has one readback correction structure");
  expect_true(eligible[0] == cerebras_v3::response_structure_confirm_correction_readback_ask, "phone correction requires readback");
}

static void retries_readbacks_and_close_have_strict_eligibility(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan field_plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_structure eligible[10];
  int count = 0;
  prepare_context(&state, &interpretation, &field_plan, &context, cerebras_v3::field_callback_time);
  state.history.retry_counts[cerebras_v3::field_callback_time] = 1;
  count = cerebras_v3::collect_eligible_response_structures(&context, eligible, 10);
  expect_true((count == 1) && (eligible[0] == cerebras_v3::response_structure_clarify_ask), "retry requires clarification structure");

  state.history.retry_counts[cerebras_v3::field_callback_time] = 0;
  state.fields[cerebras_v3::field_callback_time].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].confirmed = false;
  count = cerebras_v3::collect_eligible_response_structures(&context, eligible, 10);
  expect_true((count == 1) && (eligible[0] == cerebras_v3::response_structure_readback_ask), "unconfirmed callback requires readback");

  field_plan.next_field = cerebras_v3::field_none;
  field_plan.complete = true;
  count = cerebras_v3::collect_eligible_response_structures(&context, eligible, 10);
  expect_true((count == 1) && (eligible[0] == cerebras_v3::response_structure_close), "complete plan requires close");
}

static void confirmation_readback_takes_priority_over_repair(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan field_plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_structure eligible[10];
  int count = 0;
  prepare_context(&state, &interpretation, &field_plan, &context, cerebras_v3::field_callback_time);
  state.fields[cerebras_v3::field_callback_time].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].confirmed = false;
  state.history.retry_counts[cerebras_v3::field_callback_time] = 1;
  cerebras_v3::copy_text(interpretation.turn_type, "customer_confusion", 64);
  count = cerebras_v3::collect_eligible_response_structures(&context, eligible, 10);
  expect_true(count == 1, "confused confirmation has one structure");
  expect_true(eligible[0] == cerebras_v3::response_structure_readback_ask, "confused confirmation repeats exact readback");
}

static void scoring_rewards_context_and_penalizes_repetition(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan field_plan;
  cerebras_v3::Response_context context;
  int acknowledgement_score = 0;
  int repeated_score = 0;
  prepare_context(&state, &interpretation, &field_plan, &context, cerebras_v3::field_caller_name);
  context.has_grounded_acknowledgement = true;
  acknowledgement_score = cerebras_v3::score_response_structure(
    cerebras_v3::response_structure_acknowledge_ask,
    &context);
  expect_true(
    acknowledgement_score > cerebras_v3::score_response_structure(cerebras_v3::response_structure_ask, &context),
    "grounded acknowledgement outranks plain ask");
  cerebras_v3::record_response_structure(&state, cerebras_v3::response_structure_acknowledge_ask);
  repeated_score = cerebras_v3::score_response_structure(
    cerebras_v3::response_structure_acknowledge_ask,
    &context);
  expect_true(repeated_score == (acknowledgement_score - 40), "recent structure receives repetition penalty");
  expect_true(
    cerebras_v3::score_response_structure(cerebras_v3::response_structure_answer_ask, &context) == cerebras_v3::ineligible_response_score,
    "ineligible structure cannot receive usable score");
}

static void deterministic_selection_is_reproducible_and_contextual(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan field_plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_structure first = cerebras_v3::response_structure_none;
  cerebras_v3::Response_structure second = cerebras_v3::response_structure_none;
  unsigned int seed = 0U;
  prepare_context(&state, &interpretation, &field_plan, &context, cerebras_v3::field_caller_name);
  cerebras_v3::copy_text(state.call_id, "call_selection_test", 64);
  state.history.turn_count = 4;
  context.has_grounded_acknowledgement = true;
  context.previous_requested = cerebras_v3::field_request;
  seed = cerebras_v3::deterministic_response_seed(&context);
  first = cerebras_v3::select_response_structure(&context, seed);
  second = cerebras_v3::select_response_structure(&context, seed);
  expect_true(first == second, "same context and seed select same structure");
  expect_true(cerebras_v3::response_structure_eligible(first, &context), "selector returns eligible structure");
  cerebras_v3::record_response_structure(&state, first);
  expect_true(
    cerebras_v3::select_response_structure(&context, seed) != first,
    "recent selection is avoided when another strong structure exists");

  cerebras_v3::copy_text(interpretation.turn_type, "customer_confusion", 64);
  expect_true(
    cerebras_v3::select_response_structure(&context, seed) == cerebras_v3::response_structure_clarify_ask,
    "special turn selection cannot be randomized away");
}

static void selected_structure_builds_typed_response_plan(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan field_plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_plan response_plan;
  prepare_context(&state, &interpretation, &field_plan, &context, cerebras_v3::field_phone_confirmed);
  cerebras_v3::copy_text(interpretation.turn_type, "correction", 64);
  cerebras_v3::copy_text(interpretation.answered_field, "phone", 64);
  state.history.retry_counts[cerebras_v3::field_phone_confirmed] = 1;
  expect_true(cerebras_v3::build_response_plan(&context, 123U, &response_plan), "typed response plan builds");
  expect_true(
    response_plan.structure == cerebras_v3::response_structure_confirm_correction_readback_ask,
    "typed response plan stores selected structure");
  expect_true(response_plan.target_field == cerebras_v3::field_phone_confirmed, "typed response plan stores target");
  expect_true(response_plan.retry_count == 1, "typed response plan stores retry count");
  expect_true(response_plan.act_count == 3, "typed response plan stores all structure acts");
  expect_true(response_plan.acts[0] == cerebras_v3::response_act_confirm_correction, "typed plan begins with correction confirmation");
  expect_true(response_plan.acts[1] == cerebras_v3::response_act_readback, "typed plan includes readback");
  expect_true(response_plan.acts[2] == cerebras_v3::response_act_ask, "typed plan ends with ask");
}
}

int main(void)
{
  initializes_empty_plan();
  appends_acts_in_order();
  rejects_invalid_or_excess_acts();
  labels_are_stable();
  response_history_tracks_recent_choices();
  catalog_defines_all_structures();
  normal_turn_allows_only_grounded_normal_structures();
  special_turns_enforce_special_structures();
  retries_readbacks_and_close_have_strict_eligibility();
  confirmation_readback_takes_priority_over_repair();
  scoring_rewards_context_and_penalizes_repetition();
  deterministic_selection_is_reproducible_and_contextual();
  selected_structure_builds_typed_response_plan();
  if (failures == 0)
  {
    write_line("response_policy_tests: PASS");
  }
  return failures == 0 ? 0 : 1;
}
