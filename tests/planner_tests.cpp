#include <planner.h>
#include <generated_kb.h>
#include <prompt_sections.h>
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

static bool callback_phrase_captured(const char* phrase)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  state.department = cerebras_v3::department_service;
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_department].value, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_intent].value, "maintenance", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_caller_name].value, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_last_name_spelling].value, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_vehicle].value, "Tucson", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_request].value, "dash warning", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_department].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_intent].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_caller_name].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_vehicle].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_request].status = cerebras_v3::status_captured;
  state.last_requested = cerebras_v3::field_callback_time;
  cerebras_v3::copy_text(interpretation.callback_time, phrase, cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, phrase);
  return state.fields[cerebras_v3::field_callback_time].status == cerebras_v3::status_captured;
}

static void expect_callback_accepts(const char* phrase, const char* label)
{
  expect_true(callback_phrase_captured(phrase), label);
}

static void expect_callback_rejects(const char* phrase, const char* label)
{
  expect_true(!callback_phrase_captured(phrase), label);
}

static void mark_callback_prerequisites(cerebras_v3::State* state)
{
  if (state == 0)
  {
    return;
  }
  state->fields[cerebras_v3::field_department].status = cerebras_v3::status_captured;
  state->fields[cerebras_v3::field_intent].status = cerebras_v3::status_captured;
  state->fields[cerebras_v3::field_caller_name].status = cerebras_v3::status_captured;
  state->fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state->fields[cerebras_v3::field_vehicle].status = cerebras_v3::status_captured;
  state->fields[cerebras_v3::field_request].status = cerebras_v3::status_captured;
}

static void expect_vehicle_stored(const char* input, const char* expected, const char* label)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "diagnostic", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, input, cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "truck warning light", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, input);
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field != cerebras_v3::field_vehicle, label);
  expect_true(std::strcmp(state.fields[cerebras_v3::field_vehicle].value, expected) == 0, label);
}

static void interpreted_opening_routes_to_name(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "maintenance", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Tucson", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "dash warning", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.callback_time, "next Tuesday morning", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "my dash is yelling maintenance at me");
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field == cerebras_v3::field_caller_name, "opening routes to name");
}

static void asks_only_one_question_at_a_time(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "repair", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "I need service");
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field == cerebras_v3::field_caller_name, "first missing field is name only");
  expect_true(std::strstr(plan.response_task, " and ask ") == 0, "task does not combine two asks");
}

static void parses_compact_json(void)
{
  cerebras_v3::Interpretation interpretation;
  const char* json =
    "{\"d\":\"parts\",\"i\":\"part request\",\"v\":\"Ioniq 5\",\"r\":\"charge port flap\",\"cb\":\"\",\"p\":\"\",\"q\":\"do you have it\",\"f\":\"parts_availability\",\"a\":\"yes\"}";
  const bool ok = cerebras_v3::parse_interpretation_json(json, &interpretation);
  expect_true(ok, "compact json parses");
  expect_true(std::strcmp(interpretation.department, "parts") == 0, "department parsed");
  expect_true(std::strcmp(interpretation.vehicle, "Ioniq 5") == 0, "vehicle parsed");
  expect_true(std::strcmp(interpretation.request, "charge port flap") == 0, "request parsed");
  expect_true(std::strcmp(interpretation.faq_id, "parts_availability") == 0, "faq id parsed");
  expect_true(std::strcmp(interpretation.affirmation, "yes") == 0, "affirmation parsed");
}

static void prompt_sections_include_closed_labels(void)
{
  expect_true(std::strstr(cerebras_v3::prompt_sections::interpreter_role, "live-call interpreter") != 0, "prompt role included");
  expect_true(std::strstr(cerebras_v3::prompt_sections::interpreter_schema, "\"a\":\"none\"") != 0, "prompt schema includes affirmation");
  expect_true(std::strstr(cerebras_v3::prompt_sections::interpreter_field_rules, "last-name spelling") != 0, "prompt includes field rules");
  expect_true(std::strstr(cerebras_v3::generated_kb::interpreter_faq_rules, "recall_service") != 0, "prompt includes recall faq id");
  expect_true(std::strstr(cerebras_v3::generated_kb::interpreter_faq_rules, "trade_in") != 0, "prompt includes trade faq id");
  expect_true(std::strstr(cerebras_v3::generated_kb::interpreter_affirmation_rules, "yes, no, unclear") != 0, "prompt includes affirmation labels");
  expect_true(std::strstr(cerebras_v3::prompt_sections::interpreter_output_rules, "No extra keys") != 0, "prompt includes output rules");
  expect_true(cerebras_v3::generated_kb::faq_entry_count >= 9, "generated faq entries exist");
  expect_true(cerebras_v3::generated_kb::vehicle_model_count >= 1000, "generated vehicle lexicon is full dataset");
  expect_true(cerebras_v3::generated_kb::vehicle_alias_count >= 3, "generated vehicle aliases exist");
  expect_true(cerebras_v3::generated_kb::vehicle_record_count >= 10000, "generated vehicle records exist");
}

static void name_does_not_capture_spelling_too_early(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "maintenance", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "dash warning", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "maintenance light");
  plan = cerebras_v3::plan_next(&state);
  state.last_requested = plan.next_field;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "Smith", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "yeah it is Jordan Smith");
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field == cerebras_v3::field_last_name_spelling, "name does not auto-capture spelling");
}

static void spelling_captures_when_requested(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "maintenance", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "dash warning", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "Jordan Smith");
  state.last_requested = cerebras_v3::field_last_name_spelling;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.spelling, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "Smith, S M I T H");
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field != cerebras_v3::field_last_name_spelling, "requested spelling captures");
}

static void phone_confirmation_advances(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "maintenance", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Tucson", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "dash warning", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.callback_time, "Tuesday morning", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.phone, "416 555 0199", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "full state");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].confirmed = true;
  state.last_requested = cerebras_v3::field_phone_confirmed;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::merge_interpretation(&state, &interpretation, "yes that is correct");
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field == cerebras_v3::field_final_confirmed, "phone confirmation advances to final");
}

static void ai_affirmation_advances_phone_confirmation(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "maintenance", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Tucson", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "dash warning", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.callback_time, "Tuesday morning", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.phone, "416 555 0199", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "full state");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].confirmed = true;
  state.last_requested = cerebras_v3::field_phone_confirmed;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.affirmation, "yes", 32);
  cerebras_v3::merge_interpretation(&state, &interpretation, "that works");
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field == cerebras_v3::field_final_confirmed, "ai yes advances phone confirmation");
}

static void ai_rejection_reasks_phone(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "maintenance", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Tucson", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "dash warning", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.callback_time, "Tuesday morning", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.phone, "416 555 0199", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "full state");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].confirmed = true;
  state.last_requested = cerebras_v3::field_phone_confirmed;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.affirmation, "no", 32);
  cerebras_v3::merge_interpretation(&state, &interpretation, "that is wrong");
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field == cerebras_v3::field_phone, "ai no reasks phone");
}

static void kb_confirmation_phrases_advance(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "parts", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "part request", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Taylor Nguyen", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "N G U Y E N", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Palisade", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "cargo mat", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.callback_time, "Wednesday around 10", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.phone, "519 555 9911", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "full state");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].confirmed = true;
  state.last_requested = cerebras_v3::field_phone_confirmed;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::merge_interpretation(&state, &interpretation, "right");
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field == cerebras_v3::field_final_confirmed, "right advances phone confirmation");

  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "sales", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "lease", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jamie Thompson", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "T H O M P S O N", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Tucson", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "lease numbers", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.callback_time, "Saturday morning", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.phone, "343 555 1010", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "full state");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].confirmed = true;
  state.last_requested = cerebras_v3::field_phone_confirmed;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::merge_interpretation(&state, &interpretation, "yep that is mine");
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field == cerebras_v3::field_final_confirmed, "yep that is mine advances phone confirmation");
}

static void confirmation_state_roundtrips(void)
{
  cerebras_v3::State state;
  cerebras_v3::State loaded;
  char json[2048];
  cerebras_v3::init_state(&state);
  state.fields[cerebras_v3::field_phone_confirmed].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_phone_confirmed].confirmed = true;
  state.fields[cerebras_v3::field_callback_time].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].confirmed = true;
  state.fields[cerebras_v3::field_final_confirmed].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_final_confirmed].confirmed = true;
  state.delivery_sent = true;
  cerebras_v3::copy_text(state.call_id, "call_123", 64);
  cerebras_v3::state_to_json(&state, json, 2048);
  cerebras_v3::load_state_from_json(&loaded, json);
  expect_true(loaded.fields[cerebras_v3::field_callback_time].confirmed, "callback confirmation roundtrips");
  expect_true(loaded.fields[cerebras_v3::field_phone_confirmed].confirmed, "phone confirmation roundtrips");
  expect_true(loaded.fields[cerebras_v3::field_final_confirmed].confirmed, "final confirmation roundtrips");
  expect_true(loaded.delivery_sent, "delivery sent roundtrips");
  expect_true(std::strcmp(loaded.call_id, "call_123") == 0, "call id roundtrips");
}

static void generic_vehicle_is_not_captured(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "diagnostic", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "car", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "hot smell after charging", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "my car smells hot");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field == cerebras_v3::field_vehicle, "generic car does not capture vehicle");

  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "diagnostic", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "2020", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "warning light", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "it is a 2020");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field == cerebras_v3::field_vehicle, "year alone does not capture vehicle");
}

static void whitelisted_vehicle_is_captured(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "diagnostic", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "2022 Hyundai Ioniq 5", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "hot smell after charging", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "2022 Hyundai Ioniq 5");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field != cerebras_v3::field_vehicle, "whitelisted vehicle captures");

  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "diagnostic", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "2021 Hyundai Tucson", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "warning light", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "2021 Hyundai Tucson");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field != cerebras_v3::field_vehicle, "canonical vehicle record captures");
}

static void normalized_vehicle_names_are_captured(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "diagnostic", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Ford F-150", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "truck warning light", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "Ford F-150");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field != cerebras_v3::field_vehicle, "punctuated vehicle model captures");
  expect_true(std::strcmp(state.fields[cerebras_v3::field_vehicle].value, "Ford F-150") == 0, "punctuated vehicle stored canonically");

  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "diagnostic", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "2021 Ford F one fifty", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "truck warning light", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "2021 Ford F one fifty");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field != cerebras_v3::field_vehicle, "spoken F-150 vehicle captures");
  expect_true(std::strcmp(state.fields[cerebras_v3::field_vehicle].value, "2021 Ford F-150") == 0, "spoken F-150 stored canonically");

  expect_vehicle_stored("2021 Ford F-one-fifty", "2021 Ford F-150", "hyphenated spoken F-150 captures");
  expect_vehicle_stored("2021 Ford eff one fifty", "2021 Ford F-150", "eff one fifty captures");
  expect_vehicle_stored("2021 Ford F won fifty", "2021 Ford F-150", "won fifty captures");
  expect_vehicle_stored("2021 Ford F one five zero", "2021 Ford F-150", "one five zero captures");
  expect_vehicle_stored("2021 Ford F one five oh", "2021 Ford F-150", "one five oh captures");
  expect_vehicle_stored("2021 Ford F 1 50", "2021 Ford F-150", "digit spaced F-150 captures");
  expect_vehicle_stored("2021 BMW three series", "2021 BMW 3 Series", "spoken BMW 3 Series captures");
  expect_vehicle_stored("2021 Mazda CX five", "2021 Mazda CX-5", "spoken Mazda CX-5 captures");
  expect_vehicle_stored("2021 Hyundai Ioniq five", "2021 Hyundai IONIQ 5", "spoken IONIQ 5 captures");
  expect_vehicle_stored("2020 Toyota eighty six", "2020 Toyota 86", "spoken Toyota 86 captures");
  expect_vehicle_stored("2020 Mercedes C three hundred", "2020 Mercedes-Benz C-Class", "spoken Mercedes C-Class captures");

  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "diagnostic", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "BMW 3-series", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "sedan warning light", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "BMW 3-series");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field != cerebras_v3::field_vehicle, "hyphenated vehicle model captures");

  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "diagnostic", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Hyundai Ioniq five", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "charging issue", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "Hyundai Ioniq five");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field != cerebras_v3::field_vehicle, "vehicle alias captures");
}

static void sales_opening_does_not_capture_name(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "sales", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "test drive", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Santa Fe hybrid", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "test drive Santa Fe hybrid and trade in old car", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "I want to test drive a Santa Fe hybrid and talk about trading in my old car", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "I want to test drive a Santa Fe hybrid and talk about trading in my old car");
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field == cerebras_v3::field_caller_name, "sales opening does not capture name");
}

static void requested_name_is_captured(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  state.last_requested = cerebras_v3::field_caller_name;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "sales", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Sam Patel", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "Sam Patel");
  plan = cerebras_v3::plan_next(&state);
  expect_true(std::strcmp(state.fields[cerebras_v3::field_caller_name].value, "Sam Patel") == 0, "requested name captures");
  expect_true(plan.next_field != cerebras_v3::field_caller_name, "requested name advances");
}

static void requested_name_requires_first_and_last(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "service request", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "service");
  state.last_requested = cerebras_v3::field_caller_name;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.name, "Sam", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "Sam");
  plan = cerebras_v3::plan_next(&state);
  expect_true(state.fields[cerebras_v3::field_caller_name].status == cerebras_v3::status_missing, "requested name rejects first name only");
  expect_true(plan.next_field == cerebras_v3::field_caller_name, "requested name reasks after first name only");
}

static void interpreter_confusion_captures_nothing(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "service request", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "service");
  state.last_requested = cerebras_v3::field_caller_name;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.turn_type, "customer_confusion", 64);
  cerebras_v3::copy_text(interpretation.answered_field, "none", 64);
  cerebras_v3::copy_text(interpretation.name, "what", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "what");
  plan = cerebras_v3::plan_next(&state);
  expect_true(state.fields[cerebras_v3::field_caller_name].status == cerebras_v3::status_missing, "confusion does not capture name");
  expect_true(plan.next_field == cerebras_v3::field_caller_name, "confusion reasks current field");
}

static void caller_question_mid_form_captures_nothing(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "service request", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "service");
  state.last_requested = cerebras_v3::field_caller_name;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.turn_type, "caller_question", 64);
  cerebras_v3::copy_text(interpretation.answered_field, "none", 64);
  cerebras_v3::copy_text(interpretation.faq_question, "are you open tomorrow", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "open tomorrow", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "are you open tomorrow?");
  plan = cerebras_v3::plan_next(&state);
  expect_true(state.fields[cerebras_v3::field_caller_name].status == cerebras_v3::status_missing, "caller question does not capture name");
  expect_true(plan.next_field == cerebras_v3::field_caller_name, "caller question keeps current field");
}

static void caller_question_during_request_captures_nothing(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "service request", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Sam Patel", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "P A T E L", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Toyota Camry", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "setup");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.last_requested = cerebras_v3::field_request;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.turn_type, "caller_question", 64);
  cerebras_v3::copy_text(interpretation.answered_field, "none", 64);
  cerebras_v3::copy_text(interpretation.faq_question, "how late are you open", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "how late are you open", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "how late are you open?");
  plan = cerebras_v3::plan_next(&state);
  expect_true(state.fields[cerebras_v3::field_request].status == cerebras_v3::status_missing, "caller question does not capture request");
  expect_true(plan.next_field == cerebras_v3::field_request, "caller question keeps request field");
}

static void parses_interpreter_turn_type(void)
{
  cerebras_v3::Interpretation interpretation;
  const char* json =
    "{\"tt\":\"customer_confusion\",\"af\":\"none\",\"m\":\"caller asks what the agent means\",\"d\":\"\",\"i\":\"\",\"v\":\"\",\"r\":\"\",\"cb\":\"\",\"p\":\"\",\"n\":\"\",\"s\":\"\",\"q\":\"\",\"f\":\"none\",\"a\":\"none\"}";
  const bool ok = cerebras_v3::parse_interpretation_json(json, &interpretation);
  expect_true(ok, "turn type json parses");
  expect_true(std::strcmp(interpretation.turn_type, "customer_confusion") == 0, "turn type parsed");
  expect_true(std::strcmp(interpretation.answered_field, "none") == 0, "answered field parsed");
  expect_true(std::strcmp(interpretation.meaning, "caller asks what the agent means") == 0, "meaning parsed");
}

static void correction_overwrites_vehicle(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "service request", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Sam Patel", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "P A T E L", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Hyundai Tucson", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "setup");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.last_requested = cerebras_v3::field_request;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.turn_type, "correction", 64);
  cerebras_v3::copy_text(interpretation.answered_field, "vehicle", 64);
  cerebras_v3::copy_text(interpretation.vehicle, "Honda Civic", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "actually it is a Honda Civic");
  plan = cerebras_v3::plan_next(&state);
  expect_true(std::strcmp(state.fields[cerebras_v3::field_vehicle].value, "Honda Civic") == 0, "correction overwrites vehicle");
  expect_true(plan.next_field == cerebras_v3::field_request, "vehicle correction continues flow");
}

static void correction_overwrites_phone_and_clears_confirmation(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "service request", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Sam Patel", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "P A T E L", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Honda Civic", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "brake noise", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.callback_time, "tomorrow at 10 AM", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.phone, "4165551111", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "setup");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].confirmed = true;
  state.fields[cerebras_v3::field_phone_confirmed].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_phone_confirmed].confirmed = true;
  state.last_requested = cerebras_v3::field_final_confirmed;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.turn_type, "correction", 64);
  cerebras_v3::copy_text(interpretation.answered_field, "phone", 64);
  cerebras_v3::copy_text(interpretation.phone, "4165559999", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "actually my number is 416 555 9999");
  plan = cerebras_v3::plan_next(&state);
  expect_true(std::strcmp(state.fields[cerebras_v3::field_phone].value, "4165559999") == 0, "correction overwrites phone");
  expect_true(!state.fields[cerebras_v3::field_phone_confirmed].confirmed, "phone correction clears confirmation");
  expect_true(plan.next_field == cerebras_v3::field_phone_confirmed, "phone correction re-confirms phone");
}

static void correction_switches_department(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "service request", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "service");
  state.last_requested = cerebras_v3::field_caller_name;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.turn_type, "correction", 64);
  cerebras_v3::copy_text(interpretation.answered_field, "department", 64);
  cerebras_v3::copy_text(interpretation.department, "parts", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "actually this is for parts");
  plan = cerebras_v3::plan_next(&state);
  expect_true(state.department == cerebras_v3::department_parts, "correction switches department");
  expect_true(plan.next_field == cerebras_v3::field_caller_name, "department correction continues flow");
}

static void correction_overwrites_request(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "service request", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Sam Patel", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "P A T E L", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Honda Civic", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "oil change", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "setup");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.last_requested = cerebras_v3::field_callback_time;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.turn_type, "correction", 64);
  cerebras_v3::copy_text(interpretation.answered_field, "request", 64);
  cerebras_v3::copy_text(interpretation.request, "brake noise", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "actually it is a brake noise");
  plan = cerebras_v3::plan_next(&state);
  expect_true(std::strcmp(state.fields[cerebras_v3::field_request].value, "brake noise") == 0, "correction overwrites request");
  expect_true(plan.next_field == cerebras_v3::field_callback_time, "request correction continues flow");
}

static void correction_overwrites_callback_time(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "service request", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Sam Patel", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "P A T E L", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Honda Civic", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "brake noise", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.callback_time, "tomorrow at 10 AM", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "setup");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.last_requested = cerebras_v3::field_phone;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.turn_type, "correction", 64);
  cerebras_v3::copy_text(interpretation.answered_field, "callback_time", 64);
  cerebras_v3::copy_text(interpretation.callback_time, "Friday at noon", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "sorry Friday at noon works better");
  plan = cerebras_v3::plan_next(&state);
  expect_true(std::strcmp(state.fields[cerebras_v3::field_callback_time].value, "Friday at noon") == 0, "correction overwrites callback time");
  expect_true(plan.next_field == cerebras_v3::field_callback_time, "callback correction re-confirms callback time");
}

static void correction_overwrites_spelling(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "service request", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Sam Patel", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "P A T T E L", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "setup");
  state.last_requested = cerebras_v3::field_vehicle;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.turn_type, "correction", 64);
  cerebras_v3::copy_text(interpretation.answered_field, "last_name_spelling", 64);
  cerebras_v3::copy_text(interpretation.spelling, "P A T E L", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "actually one T, P A T E L");
  plan = cerebras_v3::plan_next(&state);
  expect_true(std::strcmp(state.fields[cerebras_v3::field_last_name_spelling].value, "P A T E L") == 0, "correction overwrites spelling");
  expect_true(plan.next_field == cerebras_v3::field_vehicle, "spelling correction continues flow");
}

static void correction_overwrites_name(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "service request", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Sam Patel", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "P A T E L", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "setup");
  state.last_requested = cerebras_v3::field_vehicle;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.turn_type, "correction", 64);
  cerebras_v3::copy_text(interpretation.answered_field, "name", 64);
  cerebras_v3::copy_text(interpretation.name, "Samantha Patel", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "actually it is Samantha Patel");
  plan = cerebras_v3::plan_next(&state);
  expect_true(std::strcmp(state.fields[cerebras_v3::field_caller_name].value, "Samantha Patel") == 0, "correction overwrites name");
  expect_true(plan.next_field == cerebras_v3::field_last_name_spelling, "name correction reasks spelling");
}

static void precursor_name_is_captured(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "my name is Jordan Smith");
  expect_true(std::strcmp(state.fields[cerebras_v3::field_caller_name].value, "Jordan Smith") == 0, "precursor name captures");
}

static void non_name_precursor_does_not_capture_name(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.name, "Service", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "this is for service");
  expect_true(state.fields[cerebras_v3::field_caller_name].status == cerebras_v3::status_missing, "non-name precursor does not capture");
}

static void warranty_sentence_does_not_capture_spelling(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "warranty inquiry", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "2020 Santa Fe", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "noise in 2020 Santa Fe warranty", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "I have a warranty question about a noise in my 2020 Santa Fe", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "I have a warranty question about a noise in my 2020 Santa Fe");
  plan = cerebras_v3::plan_next(&state);
  expect_true(state.fields[cerebras_v3::field_last_name_spelling].status == cerebras_v3::status_missing, "warranty sentence does not capture spelling");
  expect_true(plan.next_field == cerebras_v3::field_caller_name, "warranty sentence still asks name first");
}

static void unsolicited_spelled_letters_capture(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.spelling, "L E E", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "L E E");
  expect_true(std::strcmp(state.fields[cerebras_v3::field_last_name_spelling].value, "L E E") == 0, "unsolicited separated letters capture");
}

static void callback_time_must_be_inside_allowed_window(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "maintenance", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Tucson", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "dash warning", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.callback_time, "later today", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "later today");
  mark_callback_prerequisites(&state);
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field == cerebras_v3::field_callback_time, "later today is rejected after hours");

  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.callback_time, "tomorrow after 2", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "tomorrow after 2");
  mark_callback_prerequisites(&state);
  plan = cerebras_v3::plan_next(&state);
  expect_true(state.fields[cerebras_v3::field_callback_time].status == cerebras_v3::status_captured, "tomorrow after 2 captures");
  expect_true(plan.next_field == cerebras_v3::field_callback_time, "tomorrow after 2 asks confirmation");

  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "maintenance", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "Tucson", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "dash warning", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.callback_time, "tomorrow at 10 AM", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "tomorrow at 10 AM");
  mark_callback_prerequisites(&state);
  plan = cerebras_v3::plan_next(&state);
  expect_true(state.fields[cerebras_v3::field_callback_time].status == cerebras_v3::status_captured, "tomorrow at 10 AM captures");
  expect_true(plan.next_field == cerebras_v3::field_callback_time, "tomorrow at 10 AM asks confirmation");

  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "sales", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "test drive", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Alex Rivera", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "R I V E R A", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "test drive", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.callback_time, "Friday at noon", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "Friday at noon");
  mark_callback_prerequisites(&state);
  plan = cerebras_v3::plan_next(&state);
  expect_true(state.fields[cerebras_v3::field_callback_time].status == cerebras_v3::status_captured, "Friday at noon captures");
  expect_true(plan.next_field == cerebras_v3::field_callback_time, "Friday at noon asks confirmation");

  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "recall", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Morgan Lee", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "L E E", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "2021 Hyundai Tucson", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "recall letter", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "recall setup");
  mark_callback_prerequisites(&state);
  state.last_requested = cerebras_v3::field_callback_time;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::merge_interpretation(&state, &interpretation, "Monday around 10");
  plan = cerebras_v3::plan_next(&state);
  expect_true(state.fields[cerebras_v3::field_callback_time].status == cerebras_v3::status_captured, "raw callback time captures when ai leaves cb empty");
  expect_true(plan.next_field == cerebras_v3::field_callback_time, "raw callback time asks confirmation");

  expect_callback_accepts("tomorrow morning", "tomorrow morning captures");
  expect_callback_accepts("tomorrow afternoon", "tomorrow afternoon captures");
  expect_callback_accepts("tomorrow after lunch", "tomorrow after lunch captures");
  expect_callback_accepts("next Friday first thing", "first thing captures with a day");
  expect_callback_accepts("next business day when you open", "open time captures with business day");
  expect_callback_accepts("Friday between 10 and 2", "between range captures");
  expect_callback_accepts("Monday from 1 to 4", "from range captures");
  expect_callback_accepts("Wednesday 2:30", "colon time captures");
  expect_callback_accepts("Thursday 10 am", "spoken am time captures");
  expect_callback_accepts("anytime tomorrow", "anytime tomorrow captures");
  expect_callback_accepts("June 21, 2026 at 2 PM", "concrete interpreted date captures");

  expect_callback_rejects("anytime", "anytime alone is too vague");
  expect_callback_rejects("next week at 10", "next week without a day is too vague");
  expect_callback_rejects("tomorrow evening", "tomorrow evening is rejected after hours");
  expect_callback_rejects("Friday after work", "after work is rejected after hours");
  expect_callback_rejects("Monday whenever", "whenever is too vague");
  expect_callback_rejects("Tuesday after 6", "after 6 is rejected after hours");
}

static void callback_time_confirmation_advances_or_reasks(void)
{
  cerebras_v3::State state;
  cerebras_v3::Interpretation interpretation;
  cerebras_v3::Plan plan;
  cerebras_v3::init_state(&state);
  state.department = cerebras_v3::department_service;
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_department].value, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_intent].value, "maintenance", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_caller_name].value, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_last_name_spelling].value, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_vehicle].value, "Tucson", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_request].value, "dash warning", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_time].value, "June 21, 2026 at 2 PM", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_department].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_intent].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_caller_name].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_vehicle].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_request].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].status = cerebras_v3::status_captured;
  state.last_requested = cerebras_v3::field_callback_time;
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field == cerebras_v3::field_callback_time, "callback confirmation is requested");

  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.affirmation, "yes", 32);
  cerebras_v3::merge_interpretation(&state, &interpretation, "yes");
  plan = cerebras_v3::plan_next(&state);
  expect_true(state.fields[cerebras_v3::field_callback_time].confirmed, "callback yes confirms time");
  expect_true(plan.next_field == cerebras_v3::field_phone, "callback confirmation advances to phone");

  cerebras_v3::init_state(&state);
  state.department = cerebras_v3::department_service;
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_department].value, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_intent].value, "maintenance", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_caller_name].value, "Jordan Smith", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_last_name_spelling].value, "S M I T H", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_vehicle].value, "Tucson", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_request].value, "dash warning", cerebras_v3::max_text);
  cerebras_v3::copy_text(state.fields[cerebras_v3::field_callback_time].value, "June 21, 2026 at 2 PM", cerebras_v3::max_text);
  state.fields[cerebras_v3::field_department].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_intent].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_caller_name].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_vehicle].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_request].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_callback_time].status = cerebras_v3::status_captured;
  state.last_requested = cerebras_v3::field_callback_time;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.affirmation, "no", 32);
  cerebras_v3::merge_interpretation(&state, &interpretation, "no");
  plan = cerebras_v3::plan_next(&state);
  expect_true(state.fields[cerebras_v3::field_callback_time].status == cerebras_v3::status_missing, "callback no clears time");
  expect_true(plan.next_field == cerebras_v3::field_callback_time, "callback no asks again");
}
}

int main(void)
{
  interpreted_opening_routes_to_name();
  asks_only_one_question_at_a_time();
  parses_compact_json();
  prompt_sections_include_closed_labels();
  name_does_not_capture_spelling_too_early();
  spelling_captures_when_requested();
  phone_confirmation_advances();
  ai_affirmation_advances_phone_confirmation();
  ai_rejection_reasks_phone();
  kb_confirmation_phrases_advance();
  confirmation_state_roundtrips();
  generic_vehicle_is_not_captured();
  whitelisted_vehicle_is_captured();
  normalized_vehicle_names_are_captured();
  sales_opening_does_not_capture_name();
  requested_name_is_captured();
  requested_name_requires_first_and_last();
  interpreter_confusion_captures_nothing();
  caller_question_mid_form_captures_nothing();
  caller_question_during_request_captures_nothing();
  parses_interpreter_turn_type();
  correction_overwrites_vehicle();
  correction_overwrites_phone_and_clears_confirmation();
  correction_switches_department();
  correction_overwrites_request();
  correction_overwrites_callback_time();
  correction_overwrites_spelling();
  correction_overwrites_name();
  precursor_name_is_captured();
  non_name_precursor_does_not_capture_name();
  warranty_sentence_does_not_capture_spelling();
  unsolicited_spelled_letters_capture();
  callback_time_must_be_inside_allowed_window();
  callback_time_confirmation_advances_or_reasks();
  if (failures == 0)
  {
    write_line("planner_tests: PASS");
  }
  return failures;
}
