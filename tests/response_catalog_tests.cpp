#include <response_catalog.h>
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

static void prepare_phrase_context(
  cerebras_v3::State* state,
  cerebras_v3::Response_plan* plan,
  cerebras_v3::Phrase_context* context,
  cerebras_v3::Field_id target,
  cerebras_v3::Response_structure structure)
{
  cerebras_v3::init_state(state);
  state->department = cerebras_v3::department_service;
  cerebras_v3::init_response_plan(plan, target);
  plan->structure = structure;
  cerebras_v3::init_phrase_context(context);
  context->state = state;
  context->response_plan = plan;
}

static void add_act(cerebras_v3::Response_plan* plan, cerebras_v3::Response_act act)
{
  expect_true(cerebras_v3::append_response_act(plan, act), "test response act appends");
}

static void catalog_has_safe_question_coverage(void)
{
  const cerebras_v3::Field_id targets[] =
  {
    cerebras_v3::field_department,
    cerebras_v3::field_intent,
    cerebras_v3::field_caller_name,
    cerebras_v3::field_last_name_spelling,
    cerebras_v3::field_vehicle,
    cerebras_v3::field_request,
    cerebras_v3::field_callback_time,
    cerebras_v3::field_phone
  };
  int target_index = 0;
  expect_true(cerebras_v3::phrase_catalog_size() >= 50, "phrase catalog has broad initial coverage");
  while (target_index < 8)
  {
    cerebras_v3::State state;
    cerebras_v3::Response_plan plan;
    cerebras_v3::Phrase_context context;
    const cerebras_v3::Phrase_definition* phrases[16];
    prepare_phrase_context(&state, &plan, &context, targets[target_index], cerebras_v3::response_structure_ask);
    add_act(&plan, cerebras_v3::response_act_ask);
    expect_true(
      cerebras_v3::collect_eligible_phrases(cerebras_v3::response_act_ask, &context, phrases, 16) >= 1,
      "initial target has a safe question phrase");
    target_index += 1;
  }
}

static int character_count(const char* text, char character)
{
  int count = 0;
  int index = 0;
  if (text == 0)
  {
    return 0;
  }
  while (text[index] != '\0')
  {
    if (text[index] == character)
    {
      count += 1;
    }
    index += 1;
  }
  return count;
}

static void catalog_ids_and_wording_are_safe(void)
{
  const char* banned[] =
  {
    "for our records",
    "assist you better",
    "assist you further",
    "service appointment",
    "schedule an appointment",
    "Mr.",
    "Ms.",
    "Mrs."
  };
  int index = 0;
  while (index < cerebras_v3::phrase_catalog_size())
  {
    const cerebras_v3::Phrase_definition* phrase = cerebras_v3::phrase_definition_at(index);
    int comparison = index + 1;
    int banned_index = 0;
    expect_true(phrase != 0, "catalog phrase exists");
    while ((phrase != 0) && (comparison < cerebras_v3::phrase_catalog_size()))
    {
      const cerebras_v3::Phrase_definition* other = cerebras_v3::phrase_definition_at(comparison);
      expect_true((other != 0) && (phrase->id != other->id), "phrase ids are unique");
      comparison += 1;
    }
    while ((phrase != 0) && (banned_index < 8))
    {
      expect_true(std::strstr(phrase->text, banned[banned_index]) == 0, "preset phrase excludes banned wording");
      banned_index += 1;
    }
    if ((phrase != 0) && (phrase->act == cerebras_v3::response_act_ask))
    {
      expect_true(character_count(phrase->text, '?') == 1, "preset question contains exactly one question mark");
    }
    index += 1;
  }
}

static void question_mode_selects_retry_and_confirmation_collections(void)
{
  cerebras_v3::State state;
  cerebras_v3::Response_plan plan;
  cerebras_v3::Phrase_context context;
  const cerebras_v3::Phrase_definition* phrases[16];
  int count = 0;
  prepare_phrase_context(&state, &plan, &context, cerebras_v3::field_callback_time, cerebras_v3::response_structure_clarify_ask);
  plan.retry_count = 1;
  add_act(&plan, cerebras_v3::response_act_clarify);
  add_act(&plan, cerebras_v3::response_act_ask);
  count = cerebras_v3::collect_eligible_phrases(cerebras_v3::response_act_ask, &context, phrases, 16);
  expect_true(count == 1, "first callback retry has one bounded question");
  expect_true(phrases[0]->id == 261, "first callback retry asks for day");

  cerebras_v3::init_response_plan(&plan, cerebras_v3::field_callback_time);
  plan.structure = cerebras_v3::response_structure_readback_ask;
  add_act(&plan, cerebras_v3::response_act_readback);
  add_act(&plan, cerebras_v3::response_act_ask);
  count = cerebras_v3::collect_eligible_phrases(cerebras_v3::response_act_ask, &context, phrases, 16);
  expect_true(count == 2, "callback readback has confirmation questions");
  expect_true(phrases[0]->variant == cerebras_v3::phrase_variant_confirmation_question, "readback cannot select initial question");
  expect_true(
    std::strstr(cerebras_v3::find_phrase_definition(301)->text, "right") != 0,
    "callback confirmation uses short natural wording");
  expect_true(
    std::strstr(cerebras_v3::find_phrase_definition(302)->text, "right") != 0,
    "callback confirmation checks the assistant's interpretation naturally");
}

static void placeholders_require_grounded_values_and_render_exactly(void)
{
  cerebras_v3::State state;
  cerebras_v3::Response_plan plan;
  cerebras_v3::Phrase_context context;
  const cerebras_v3::Phrase_definition* phrase = 0;
  char output[512];
  prepare_phrase_context(&state, &plan, &context, cerebras_v3::field_callback_time, cerebras_v3::response_structure_readback_ask);
  add_act(&plan, cerebras_v3::response_act_readback);
  add_act(&plan, cerebras_v3::response_act_ask);
  phrase = cerebras_v3::find_phrase_definition(801);
  expect_true(!cerebras_v3::phrase_eligible(phrase, &context), "callback readback requires captured callback value");
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_date].value, "Friday", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_callback_date].status = cerebras_v3::status_captured;
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_time].value, "afternoon", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_callback_time].status = cerebras_v3::status_captured;
  expect_true(cerebras_v3::render_phrase(phrase, &context, output, 512), "grounded callback readback renders");
  expect_true(std::strcmp(output, "Okay, Friday afternoon.") == 0, "callback placeholder frames value naturally");

  phrase = cerebras_v3::find_phrase_definition(901);
  expect_true(!cerebras_v3::phrase_eligible(phrase, &context), "knowledge answer requires approved answer text");
  context.kb_answer = "Service opens at nine tomorrow.";
  expect_true(cerebras_v3::render_phrase(phrase, &context, output, 512), "approved knowledge answer renders");
  expect_true(std::strcmp(output, "Service opens at nine tomorrow.") == 0, "knowledge answer renders without alteration");
}

static void deterministic_selection_avoids_recent_phrases(void)
{
  cerebras_v3::State state;
  cerebras_v3::Response_plan plan;
  cerebras_v3::Phrase_context context;
  const cerebras_v3::Phrase_definition* first = 0;
  const cerebras_v3::Phrase_definition* second = 0;
  unsigned int seed = 0U;
  prepare_phrase_context(&state, &plan, &context, cerebras_v3::field_caller_name, cerebras_v3::response_structure_ask);
  cerebras_v3::copy_text(state.call_id, "phrase_selection_call", 64);
  add_act(&plan, cerebras_v3::response_act_ask);
  seed = cerebras_v3::deterministic_phrase_seed(&context);
  first = cerebras_v3::select_phrase(cerebras_v3::response_act_ask, &context, seed);
  expect_true(first != 0, "phrase selector returns eligible phrase");
  expect_true(first == cerebras_v3::select_phrase(cerebras_v3::response_act_ask, &context, seed), "same seed repeats same phrase selection");
  cerebras_v3::record_response_phrase(&state, first->id);
  second = cerebras_v3::select_phrase(cerebras_v3::response_act_ask, &context, seed);
  expect_true(second != 0, "phrase selector retains fallback after history update");
  expect_true(second->id != first->id, "recent phrase is avoided when another strong phrase exists");
}

static void knowledge_answer_outscores_generic_deferral(void)
{
  cerebras_v3::State state;
  cerebras_v3::Response_plan plan;
  cerebras_v3::Phrase_context context;
  const cerebras_v3::Phrase_definition* selected = 0;
  prepare_phrase_context(&state, &plan, &context, cerebras_v3::field_caller_name, cerebras_v3::response_structure_answer_ask);
  add_act(&plan, cerebras_v3::response_act_answer);
  add_act(&plan, cerebras_v3::response_act_ask);
  context.kb_answer = "Service opens at nine tomorrow.";
  selected = cerebras_v3::select_phrase(cerebras_v3::response_act_answer, &context, 77U);
  expect_true((selected != 0) && (selected->id == 901), "approved knowledge answer always beats generic deferral");
}
}

int main(void)
{
  catalog_has_safe_question_coverage();
  catalog_ids_and_wording_are_safe();
  question_mode_selects_retry_and_confirmation_collections();
  placeholders_require_grounded_values_and_render_exactly();
  deterministic_selection_avoids_recent_phrases();
  knowledge_answer_outscores_generic_deferral();
  if (failures == 0)
  {
    write_line("response_catalog_tests: PASS");
  }
  return failures == 0 ? 0 : 1;
}
