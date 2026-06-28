#include <response_renderer.h>
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

static void prepare(
  cerebras_v3::State* state,
  cerebras_v3::Interpretation* interpretation,
  cerebras_v3::Plan* field_plan,
  cerebras_v3::Response_context* context,
  cerebras_v3::Field_id target)
{
  cerebras_v3::init_state(state);
  state->department = cerebras_v3::department_service;
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

static void normal_response_composes_and_records_history(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan field_plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_options options;
  cerebras_v3::Response_render_result result;
  prepare(&state, &interpretation, &field_plan, &context, cerebras_v3::field_caller_name);
  cerebras_v3::copy_text(state.call_id, "render_normal", 64);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_request].value, "clicking noise", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_request].status = cerebras_v3::status_captured;
  context.has_grounded_acknowledgement = true;
  cerebras_v3::init_response_render_options(&options);
  expect_true(cerebras_v3::render_structured_response(&state, &context, &options, &result), "normal structured response renders");
  expect_true(std::strchr(result.text, '?') != 0, "normal structured response asks one question");
  expect_true(state.history.recent_structure_count == 1, "successful render records structure");
  expect_true(state.history.recent_phrase_count >= 1, "successful render records preset phrases");
}

static void readback_response_contains_exact_value(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan field_plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_options options;
  cerebras_v3::Response_render_result result;
  prepare(&state, &interpretation, &field_plan, &context, cerebras_v3::field_callback_time);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_time].value, "Friday afternoon", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_callback_time].status = cerebras_v3::status_captured;
  cerebras_v3::init_response_render_options(&options);
  expect_true(cerebras_v3::render_structured_response(&state, &context, &options, &result), "callback readback response renders");
  expect_true(std::strstr(result.text, "Friday afternoon") != 0, "callback readback uses exact state value");
  expect_true(result.plan.structure == cerebras_v3::response_structure_readback_ask, "callback uses readback structure");
}

static bool unsafe_ai_slot(
  cerebras_v3::Response_act,
  cerebras_v3::Field_id,
  const cerebras_v3::State*,
  char* output,
  int capacity,
  void*)
{
  cerebras_v3::copy_text(output, "It will cost $500.", capacity);
  return true;
}

static void unsafe_ai_slot_falls_back_to_preset(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan field_plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_options options;
  cerebras_v3::Response_render_result result;
  prepare(&state, &interpretation, &field_plan, &context, cerebras_v3::field_caller_name);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_request].value, "brake noise", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_request].status = cerebras_v3::status_captured;
  context.has_grounded_acknowledgement = true;
  cerebras_v3::init_response_render_options(&options);
  options.enable_ai_slots = true;
  options.ai_generator = unsafe_ai_slot;
  expect_true(cerebras_v3::render_structured_response(&state, &context, &options, &result), "unsafe AI slot falls back and response renders");
  expect_true(!result.used_ai, "rejected AI slot is not marked used");
  expect_true(std::strstr(result.text, "$500") == 0, "unsafe AI output never reaches response");
}
}

int main(void)
{
  normal_response_composes_and_records_history();
  readback_response_contains_exact_value();
  unsafe_ai_slot_falls_back_to_preset();
  if (failures == 0)
  {
    (void)write(1, "response_renderer_tests: PASS\n", 30U);
  }
  return failures == 0 ? 0 : 1;
}
