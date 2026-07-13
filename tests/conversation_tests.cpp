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

static int question_count(const char* text)
{
  int count = 0;
  int index = 0;
  while ((text != 0) && (text[index] != '\0'))
  {
    if (text[index] == '?')
    {
      count += 1;
    }
    index += 1;
  }
  return count;
}

static void capture(
  cerebras_v3::State* state,
  cerebras_v3::Field_id field,
  const char* value)
{
  cerebras_v3::copy_text(state->fields[field].value, value, cerebras_v3::max_text);
  state->fields[field].status = cerebras_v3::status_captured;
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
  capture(state, cerebras_v3::field_department, "service");
  cerebras_v3::copy_text(state->call_id, "conversation_acceptance", 64);
  cerebras_v3::clear_interpretation(interpretation);
  field_plan->next_field = target;
  field_plan->response_task = "acceptance test";
  field_plan->fallback_sentence = "fallback";
  field_plan->complete = false;
  cerebras_v3::init_response_context(context);
  context->state = state;
  context->field_plan = field_plan;
  context->interpretation = interpretation;
}

static bool render(
  cerebras_v3::State* state,
  cerebras_v3::Response_context* context,
  const char* kb_answer,
  cerebras_v3::Response_render_result* result)
{
  cerebras_v3::Response_render_options options;
  cerebras_v3::init_response_render_options(&options);
  options.kb_answer = kb_answer;
  return cerebras_v3::render_structured_response(state, context, &options, result);
}

static void cq02_information_rich_opening(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_result result;
  prepare(&state, &interpretation, &plan, &context, cerebras_v3::field_caller_name);
  capture(&state, cerebras_v3::field_vehicle, "2020 Civic");
  capture(&state, cerebras_v3::field_request, "clicking noise");
  context.has_grounded_acknowledgement = true;
  context.previous_requested = cerebras_v3::field_none;
  expect_true(render(&state, &context, "", &result), "CQ-02 renders");
  expect_true(std::strstr(result.text, "clicking noise") != 0 || std::strstr(result.text, "2020 Civic") != 0, "CQ-02 grounds acknowledgement");
  expect_true(question_count(result.text) == 1, "CQ-02 asks one name question");
}

static void cq03_department_only_answer(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_result result;
  prepare(&state, &interpretation, &plan, &context, cerebras_v3::field_intent);
  cerebras_v3::copy_text(interpretation.turn_type, "field_answer", 64);
  expect_true(render(&state, &context, "", &result), "CQ-03 renders");
  expect_true(result.plan.structure == cerebras_v3::response_structure_ask, "CQ-03 uses direct ask");
  expect_true(std::strstr(result.text, "Great") == 0 && std::strstr(result.text, "Perfect") == 0, "CQ-03 avoids filler");
}

static void cq04_vague_callback_retry(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_result result;
  prepare(&state, &interpretation, &plan, &context, cerebras_v3::field_callback_date);
  state.history.retry_counts[cerebras_v3::field_callback_date] = 1;
  expect_true(render(&state, &context, "", &result), "CQ-04 renders");
  expect_true(result.plan.structure == cerebras_v3::response_structure_clarify_ask, "CQ-04 uses retry structure");
  expect_true(
    (std::strstr(result.text, "date") != 0) || (std::strstr(result.text, "day") != 0),
    "CQ-04 narrows vague callback to date or day");
}

static void cq05_customer_confusion(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_result result;
  prepare(&state, &interpretation, &plan, &context, cerebras_v3::field_caller_name);
  cerebras_v3::copy_text(interpretation.turn_type, "customer_confusion", 64);
  state.history.retry_counts[cerebras_v3::field_caller_name] = 1;
  expect_true(render(&state, &context, "", &result), "CQ-05 renders");
  expect_true(result.plan.structure == cerebras_v3::response_structure_clarify_ask, "CQ-05 explains then reasks");
  expect_true(question_count(result.text) == 1, "CQ-05 retains one question");
}

static void cq06_caller_question_during_collection(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_result result;
  prepare(&state, &interpretation, &plan, &context, cerebras_v3::field_caller_name);
  cerebras_v3::copy_text(interpretation.turn_type, "caller_question", 64);
  context.has_kb_answer = true;
  context.previous_requested = cerebras_v3::field_caller_name;
  expect_true(render(&state, &context, "Service opens at nine tomorrow.", &result), "CQ-06 renders");
  expect_true(std::strstr(result.text, "Service opens at nine tomorrow") != 0, "CQ-06 uses approved KB answer");
  expect_true(question_count(result.text) == 1, "CQ-06 resumes one interrupted question");
}

static void callback_owner_is_explicit_when_caller_asks(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_result result;
  prepare(&state, &interpretation, &plan, &context, cerebras_v3::field_caller_name);
  cerebras_v3::copy_text(interpretation.turn_type, "caller_question", 64);
  context.previous_requested = cerebras_v3::field_caller_name;
  expect_true(render(&state, &context, "", &result), "callback owner answer renders");
  expect_true(
    std::strstr(result.text, "service team") != 0,
    "callback owner names service team");
  expect_true(
    std::strstr(result.text, "call you back") != 0,
    "callback owner explicitly says service team will call back");
  expect_true(question_count(result.text) == 1, "callback owner answer resumes one question");
}

static void cq07_corrected_request(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_result result;
  prepare(&state, &interpretation, &plan, &context, cerebras_v3::field_callback_time);
  cerebras_v3::copy_text(interpretation.turn_type, "correction", 64);
  cerebras_v3::copy_text(interpretation.answered_field, "request", 64);
  capture(&state, cerebras_v3::field_request, "clicking noise");
  expect_true(render(&state, &context, "", &result), "CQ-07 renders");
  expect_true(result.plan.structure == cerebras_v3::response_structure_confirm_correction_ask, "CQ-07 confirms correction then continues");
  expect_true(std::strstr(result.text, "updated") != 0 || std::strstr(result.text, "changed") != 0, "CQ-07 acknowledges changed value");
}

static void cq08_unclear_audio(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_result result;
  prepare(&state, &interpretation, &plan, &context, cerebras_v3::field_phone);
  cerebras_v3::copy_text(interpretation.turn_type, "unclear_audio", 64);
  state.history.retry_counts[cerebras_v3::field_phone] = 1;
  expect_true(render(&state, &context, "", &result), "CQ-08 renders");
  expect_true(result.plan.structure == cerebras_v3::response_structure_clarify_ask, "CQ-08 keeps repair structure");
  expect_true(question_count(result.text) == 1, "CQ-08 reasks only phone");
}

static void cq09_unsupported_price_or_diagnosis(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_result result;
  prepare(&state, &interpretation, &plan, &context, cerebras_v3::field_vehicle);
  cerebras_v3::copy_text(interpretation.turn_type, "caller_question", 64);
  expect_true(render(&state, &context, "", &result), "CQ-09 renders");
  expect_true(
    (std::strstr(result.text, "call you back") != 0) &&
    (std::strstr(result.text, "confirm") != 0),
    "CQ-09 safely defers unsupported answer to callback team");
  expect_true(std::strchr(result.text, '$') == 0, "CQ-09 never invents price");
}

static void cq10_callback_correction(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_result result;
  prepare(&state, &interpretation, &plan, &context, cerebras_v3::field_callback_time);
  cerebras_v3::copy_text(interpretation.turn_type, "correction", 64);
  cerebras_v3::copy_text(interpretation.answered_field, "callback_time", 64);
  capture(&state, cerebras_v3::field_callback_date, "Friday");
  capture(&state, cerebras_v3::field_callback_time, "afternoon");
  expect_true(render(&state, &context, "", &result), "CQ-10 renders");
  expect_true(result.plan.structure == cerebras_v3::response_structure_confirm_correction_readback_ask, "CQ-10 uses correction readback structure");
  expect_true(std::strstr(result.text, "Friday afternoon") != 0, "CQ-10 reads corrected callback exactly");
  expect_true(std::strstr(result.text, "Tuesday morning") == 0, "CQ-10 omits stale callback");
}

static void cq11_phone_correction(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_result result;
  prepare(&state, &interpretation, &plan, &context, cerebras_v3::field_phone_confirmed);
  cerebras_v3::copy_text(interpretation.turn_type, "correction", 64);
  cerebras_v3::copy_text(interpretation.answered_field, "phone", 64);
  capture(&state, cerebras_v3::field_phone, "4165550199");
  expect_true(render(&state, &context, "", &result), "CQ-11 renders");
  expect_true(result.plan.structure == cerebras_v3::response_structure_confirm_correction_readback_ask, "CQ-11 uses correction readback structure");
  expect_true(std::strstr(result.text, "4165550199") != 0, "CQ-11 reads corrected phone exactly");
}

static void cq12_final_close(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_result result;
  prepare(&state, &interpretation, &plan, &context, cerebras_v3::field_none);
  plan.complete = true;
  expect_true(render(&state, &context, "", &result), "CQ-12 renders");
  expect_true(result.plan.structure == cerebras_v3::response_structure_close, "CQ-12 uses close structure");
  expect_true(question_count(result.text) == 0, "CQ-12 asks no question");
  expect_true(std::strstr(result.text, "service team") != 0, "CQ-12 names handoff team");
  expect_true(std::strstr(result.text, "call you back") != 0, "CQ-12 states callback ownership");
}

static void recent_history_changes_repeated_wording(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::Response_context context;
  cerebras_v3::Response_render_result first;
  cerebras_v3::Response_render_result second;
  prepare(&state, &interpretation, &plan, &context, cerebras_v3::field_caller_name);
  expect_true(render(&state, &context, "", &first), "first repeated-field response renders");
  expect_true(render(&state, &context, "", &second), "second repeated-field response renders");
  expect_true(std::strcmp(first.text, second.text) != 0, "recent history changes repeated wording");
}
}

int main(void)
{
  cq02_information_rich_opening();
  cq03_department_only_answer();
  cq04_vague_callback_retry();
  cq05_customer_confusion();
  cq06_caller_question_during_collection();
  callback_owner_is_explicit_when_caller_asks();
  cq07_corrected_request();
  cq08_unclear_audio();
  cq09_unsupported_price_or_diagnosis();
  cq10_callback_correction();
  cq11_phone_correction();
  cq12_final_close();
  recent_history_changes_repeated_wording();
  if (failures == 0)
  {
    (void)write(1, "conversation_tests: PASS\n", 25U);
  }
  return failures == 0 ? 0 : 1;
}
