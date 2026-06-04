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
  expect_true(std::strstr(cerebras_v3::prompt_sections::interpreter_role, "Classify dealership") != 0, "prompt role included");
  expect_true(std::strstr(cerebras_v3::prompt_sections::interpreter_schema, "\"a\":\"none\"") != 0, "prompt schema includes affirmation");
  expect_true(std::strstr(cerebras_v3::prompt_sections::interpreter_field_rules, "last-name spelling") != 0, "prompt includes field rules");
  expect_true(std::strstr(cerebras_v3::generated_kb::interpreter_faq_rules, "recall_service") != 0, "prompt includes recall faq id");
  expect_true(std::strstr(cerebras_v3::generated_kb::interpreter_faq_rules, "trade_in") != 0, "prompt includes trade faq id");
  expect_true(std::strstr(cerebras_v3::generated_kb::interpreter_affirmation_rules, "yes, no, unclear") != 0, "prompt includes affirmation labels");
  expect_true(std::strstr(cerebras_v3::prompt_sections::interpreter_output_rules, "No extra keys") != 0, "prompt includes output rules");
  expect_true(cerebras_v3::generated_kb::faq_entry_count >= 9, "generated faq entries exist");
  expect_true(cerebras_v3::generated_kb::vehicle_model_count >= 1000, "generated vehicle lexicon is full dataset");
  expect_true(cerebras_v3::generated_kb::vehicle_alias_count >= 3, "generated vehicle aliases exist");
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
  state.fields[cerebras_v3::field_final_confirmed].status = cerebras_v3::status_captured;
  state.fields[cerebras_v3::field_final_confirmed].confirmed = true;
  state.delivery_sent = true;
  cerebras_v3::copy_text(state.call_id, "call_123", 64);
  cerebras_v3::state_to_json(&state, json, 2048);
  cerebras_v3::load_state_from_json(&loaded, json);
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
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field == cerebras_v3::field_callback_time, "later today is rejected after hours");

  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.callback_time, "tomorrow after 2", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "tomorrow after 2");
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field != cerebras_v3::field_callback_time, "tomorrow after 2 captures");

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
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field != cerebras_v3::field_callback_time, "tomorrow at 10 AM captures");

  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "sales", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "test drive", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Alex Rivera", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "R I V E R A", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "test drive", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.callback_time, "Friday at noon", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "Friday at noon");
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field != cerebras_v3::field_callback_time, "Friday at noon captures");

  cerebras_v3::init_state(&state);
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::copy_text(interpretation.department, "service", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.intent, "recall", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.name, "Morgan Lee", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.spelling, "L E E", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.vehicle, "2021 Hyundai Tucson", cerebras_v3::max_text);
  cerebras_v3::copy_text(interpretation.request, "recall letter", cerebras_v3::max_text);
  cerebras_v3::merge_interpretation(&state, &interpretation, "recall setup");
  state.fields[cerebras_v3::field_last_name_spelling].status = cerebras_v3::status_captured;
  state.last_requested = cerebras_v3::field_callback_time;
  cerebras_v3::clear_interpretation(&interpretation);
  cerebras_v3::merge_interpretation(&state, &interpretation, "Monday around 10");
  plan = cerebras_v3::plan_next(&state);
  expect_true(plan.next_field == cerebras_v3::field_phone, "raw callback time captures when ai leaves cb empty");
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
  precursor_name_is_captured();
  non_name_precursor_does_not_capture_name();
  warranty_sentence_does_not_capture_spelling();
  unsolicited_spelled_letters_capture();
  callback_time_must_be_inside_allowed_window();
  if (failures == 0)
  {
    write_line("planner_tests: PASS");
  }
  return failures;
}
