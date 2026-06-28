#include <response_catalog.h>
#include <cstring>

namespace cerebras_v3
{
static const Phrase_definition phrase_catalog[] =
{
  {101, response_act_ask, field_department, department_unknown, phrase_variant_initial_question, 0, 0, "Is this for service, parts, or sales?"},
  {102, response_act_ask, field_department, department_unknown, phrase_variant_initial_question, 0, 0, "Which team do you need: service, parts, or sales?"},
  {111, response_act_ask, field_intent, department_unknown, phrase_variant_initial_question, 0, 0, "What can the team help with?"},
  {112, response_act_ask, field_intent, department_service, phrase_variant_initial_question, 0, 0, "What can the service team help with?"},
  {113, response_act_ask, field_intent, department_parts, phrase_variant_initial_question, 0, 0, "What part can the team help you with?"},
  {114, response_act_ask, field_intent, department_sales, phrase_variant_initial_question, 0, 0, "What are you looking for today?"},
  {121, response_act_ask, field_caller_name, department_unknown, phrase_variant_initial_question, 0, 0, "Who should the team ask for?"},
  {122, response_act_ask, field_caller_name, department_unknown, phrase_variant_initial_question, 0, 0, "What name should they use when they call?"},
  {123, response_act_ask, field_caller_name, department_unknown, phrase_variant_initial_question, 0, 0, "May I get your first and last name?"},
  {131, response_act_ask, field_last_name_spelling, department_unknown, phrase_variant_initial_question, 0, 0, "Could you spell your last name?"},
  {132, response_act_ask, field_last_name_spelling, department_unknown, phrase_variant_initial_question, 0, 0, "How do you spell your last name?"},
  {141, response_act_ask, field_vehicle, department_unknown, phrase_variant_initial_question, 0, 0, "What year, make, and model is the vehicle?"},
  {142, response_act_ask, field_vehicle, department_unknown, phrase_variant_initial_question, 0, 0, "Which vehicle is this for?"},
  {151, response_act_ask, field_request, department_unknown, phrase_variant_initial_question, 0, 0, "What should I note for the team?"},
  {152, response_act_ask, field_request, department_unknown, phrase_variant_initial_question, 0, 0, "What would you like the team to know?"},
  {161, response_act_ask, field_callback_time, department_unknown, phrase_variant_initial_question, 0, 0, "What day and time between nine and five works for a callback?"},
  {162, response_act_ask, field_callback_time, department_unknown, phrase_variant_initial_question, 0, 0, "When between nine and five should the team call?"},
  {171, response_act_ask, field_phone, department_unknown, phrase_variant_initial_question, 0, 0, "What is the best callback number?"},
  {172, response_act_ask, field_phone, department_unknown, phrase_variant_initial_question, 0, 0, "Which number should the team call?"},

  {201, response_act_ask, field_department, department_unknown, phrase_variant_retry_question, 1, 9, "Is it service, parts, or sales?"},
  {211, response_act_ask, field_intent, department_unknown, phrase_variant_retry_question, 1, 9, "What do you need help with?"},
  {221, response_act_ask, field_caller_name, department_unknown, phrase_variant_retry_question, 1, 1, "What is your first and last name?"},
  {222, response_act_ask, field_caller_name, department_unknown, phrase_variant_retry_question, 2, 9, "What full name should the team use?"},
  {231, response_act_ask, field_last_name_spelling, department_unknown, phrase_variant_retry_question, 1, 9, "Could you say the letters in your last name?"},
  {241, response_act_ask, field_vehicle, department_unknown, phrase_variant_retry_question, 1, 1, "What make and model is it?"},
  {242, response_act_ask, field_vehicle, department_unknown, phrase_variant_retry_question, 2, 9, "Which vehicle should I write down?"},
  {251, response_act_ask, field_request, department_unknown, phrase_variant_retry_question, 1, 9, "What is the main thing the team should know?"},
  {261, response_act_ask, field_callback_time, department_unknown, phrase_variant_retry_question, 1, 1, "Which day works best?"},
  {262, response_act_ask, field_callback_time, department_unknown, phrase_variant_retry_question, 2, 9, "Would morning or afternoon work better?"},
  {271, response_act_ask, field_phone, department_unknown, phrase_variant_retry_question, 1, 9, "What callback number should I write down?"},

  {301, response_act_ask, field_callback_time, department_unknown, phrase_variant_confirmation_question, 0, 9, "Is that callback time correct?"},
  {302, response_act_ask, field_callback_time, department_unknown, phrase_variant_confirmation_question, 0, 9, "Does that callback time work?"},
  {311, response_act_ask, field_phone_confirmed, department_unknown, phrase_variant_confirmation_question, 0, 9, "Is that the correct callback number?"},
  {312, response_act_ask, field_phone_confirmed, department_unknown, phrase_variant_confirmation_question, 0, 9, "Did I get that number right?"},
  {321, response_act_ask, field_final_confirmed, department_unknown, phrase_variant_confirmation_question, 0, 9, "Do those details sound right?"},
  {322, response_act_ask, field_final_confirmed, department_unknown, phrase_variant_confirmation_question, 0, 9, "Is everything correct?"},

  {401, response_act_acknowledge, field_none, department_unknown, phrase_variant_any, 0, 9, "I have {request} noted."},
  {402, response_act_acknowledge, field_none, department_unknown, phrase_variant_any, 0, 9, "I'll include {request} for the {department} team."},
  {403, response_act_acknowledge, field_none, department_unknown, phrase_variant_any, 0, 9, "I've noted this is for your {vehicle}."},

  {501, response_act_transition, field_caller_name, department_unknown, phrase_variant_any, 0, 9, "For the follow-up,"},
  {502, response_act_transition, field_callback_time, department_unknown, phrase_variant_any, 0, 9, "For the callback,"},
  {503, response_act_transition, field_phone, department_unknown, phrase_variant_any, 0, 9, "For the callback number,"},
  {504, response_act_transition, field_none, department_unknown, phrase_variant_any, 0, 9, "Before I pass this along,"},

  {601, response_act_clarify, field_department, department_unknown, phrase_variant_any, 1, 9, "I just need the team that should follow up."},
  {611, response_act_clarify, field_intent, department_unknown, phrase_variant_any, 1, 9, "I just need the main reason for the call."},
  {621, response_act_clarify, field_caller_name, department_unknown, phrase_variant_any, 1, 9, "I need a first and last name for the follow-up."},
  {631, response_act_clarify, field_last_name_spelling, department_unknown, phrase_variant_any, 1, 9, "I need the letters in your last name."},
  {641, response_act_clarify, field_vehicle, department_unknown, phrase_variant_any, 1, 9, "I need enough detail to identify the vehicle."},
  {651, response_act_clarify, field_request, department_unknown, phrase_variant_any, 1, 9, "I just need a short description for the team."},
  {661, response_act_clarify, field_callback_time, department_unknown, phrase_variant_any, 1, 1, "I need a specific day and a time between nine and five."},
  {662, response_act_clarify, field_callback_time, department_unknown, phrase_variant_any, 2, 9, "A day plus morning or afternoon is enough."},
  {671, response_act_clarify, field_phone, department_unknown, phrase_variant_any, 1, 9, "I need a callback number with at least seven digits."},

  {701, response_act_confirm_correction, field_none, department_unknown, phrase_variant_any, 0, 9, "Thanks, I've updated that."},
  {702, response_act_confirm_correction, field_none, department_unknown, phrase_variant_any, 0, 9, "Got it, I changed that."},

  {801, response_act_readback, field_callback_time, department_unknown, phrase_variant_any, 0, 9, "I have {callback_time}."},
  {811, response_act_readback, field_phone_confirmed, department_unknown, phrase_variant_any, 0, 9, "I have {phone}."},
  {821, response_act_readback, field_final_confirmed, department_unknown, phrase_variant_any, 0, 9, "I have your request and callback details ready for the {department} team."},

  {901, response_act_answer, field_none, department_unknown, phrase_variant_any, 0, 9, "{kb_answer}"},
  {902, response_act_answer, field_none, department_unknown, phrase_variant_any, 0, 9, "The {department} team can confirm that when they follow up."},

  {1001, response_act_close, field_none, department_unknown, phrase_variant_any, 0, 9, "Thanks. I'll pass this to the {department} team for follow-up."},
  {1002, response_act_close, field_none, department_unknown, phrase_variant_any, 0, 9, "I have everything needed to pass this to the {department} team. Thanks for calling."},
  {1003, response_act_close, field_none, department_unknown, phrase_variant_any, 0, 9, "I'll pass these details to the {department} team. Thanks for calling."}
};

void init_phrase_context(Phrase_context* context)
{
  if (context == 0)
  {
    return;
  }
  context->state = 0;
  context->response_plan = 0;
  context->kb_answer = "";
  context->slot_index = 0;
}

int phrase_catalog_size(void)
{
  return static_cast<int>(sizeof(phrase_catalog) / sizeof(phrase_catalog[0]));
}

const Phrase_definition* phrase_definition_at(int index)
{
  if ((index < 0) || (index >= phrase_catalog_size()))
  {
    return 0;
  }
  return &phrase_catalog[index];
}

const Phrase_definition* find_phrase_definition(int phrase_id)
{
  int index = 0;
  while (index < phrase_catalog_size())
  {
    if (phrase_catalog[index].id == phrase_id)
    {
      return &phrase_catalog[index];
    }
    index += 1;
  }
  return 0;
}

static bool text_contains(const char* text, const char* pattern)
{
  return
    (text != 0) &&
    (pattern != 0) &&
    (std::strstr(text, pattern) != 0);
}

static bool plan_contains_act(const Response_plan* plan, Response_act act)
{
  int index = 0;
  if (plan == 0)
  {
    return false;
  }
  while (index < plan->act_count)
  {
    if (plan->acts[index] == act)
    {
      return true;
    }
    index += 1;
  }
  return false;
}

static Phrase_variant required_phrase_variant(
  Response_act act,
  const Response_plan* plan)
{
  if (act != response_act_ask)
  {
    return phrase_variant_any;
  }
  if (plan_contains_act(plan, response_act_readback))
  {
    return phrase_variant_confirmation_question;
  }
  if (plan_contains_act(plan, response_act_clarify))
  {
    return phrase_variant_retry_question;
  }
  return phrase_variant_initial_question;
}

static const char* state_field_value(const State* state, Field_id field)
{
  if ((state == 0) || (field < field_department) || (field >= field_none))
  {
    return "";
  }
  return state->fields[field].value;
}

static bool required_values_available(
  const Phrase_definition* phrase,
  const Phrase_context* context)
{
  const State* state = (context != 0) ? context->state : 0;
  if ((phrase == 0) || (phrase->text == 0))
  {
    return false;
  }
  if (text_contains(phrase->text, "{request}") &&
      (state_field_value(state, field_request)[0] == '\0'))
  {
    return false;
  }
  if (text_contains(phrase->text, "{vehicle}") &&
      (state_field_value(state, field_vehicle)[0] == '\0'))
  {
    return false;
  }
  if (text_contains(phrase->text, "{callback_time}") &&
      (state_field_value(state, field_callback_time)[0] == '\0'))
  {
    return false;
  }
  if (text_contains(phrase->text, "{phone}") &&
      (state_field_value(state, field_phone)[0] == '\0'))
  {
    return false;
  }
  if (text_contains(phrase->text, "{name}") &&
      (state_field_value(state, field_caller_name)[0] == '\0'))
  {
    return false;
  }
  if (text_contains(phrase->text, "{kb_answer}") &&
      ((context == 0) || (context->kb_answer == 0) || (context->kb_answer[0] == '\0')))
  {
    return false;
  }
  return true;
}

bool phrase_eligible(const Phrase_definition* phrase, const Phrase_context* context)
{
  Field_id target = field_none;
  Department department = department_unknown;
  int retry_count = 0;
  if ((phrase == 0) || (context == 0) || (context->response_plan == 0))
  {
    return false;
  }
  target = context->response_plan->target_field;
  if (context->state != 0)
  {
    department = context->state->department;
  }
  retry_count = context->response_plan->retry_count;
  if ((phrase->target_field != field_none) && (phrase->target_field != target))
  {
    return false;
  }
  if ((phrase->department != department_unknown) && (phrase->department != department))
  {
    return false;
  }
  if (phrase->variant != required_phrase_variant(phrase->act, context->response_plan))
  {
    return false;
  }
  if ((retry_count < phrase->minimum_retry_count) ||
      (retry_count > phrase->maximum_retry_count))
  {
    return false;
  }
  return required_values_available(phrase, context);
}

int collect_eligible_phrases(
  Response_act act,
  const Phrase_context* context,
  const Phrase_definition** output,
  int capacity)
{
  int catalog_index = 0;
  int output_count = 0;
  if ((output == 0) || (capacity <= 0))
  {
    return 0;
  }
  while ((catalog_index < phrase_catalog_size()) && (output_count < capacity))
  {
    if ((phrase_catalog[catalog_index].act == act) &&
        phrase_eligible(&phrase_catalog[catalog_index], context))
    {
      output[output_count] = &phrase_catalog[catalog_index];
      output_count += 1;
    }
    catalog_index += 1;
  }
  return output_count;
}

static int phrase_word_count(const char* text)
{
  int count = 0;
  int index = 0;
  bool in_word = false;
  if (text == 0)
  {
    return 0;
  }
  while (text[index] != '\0')
  {
    const bool separator =
      (text[index] == ' ') ||
      (text[index] == '\t') ||
      (text[index] == '\n');
    if (separator)
    {
      if (in_word)
      {
        count += 1;
      }
      in_word = false;
    }
    else
    {
      in_word = true;
    }
    index += 1;
  }
  if (in_word)
  {
    count += 1;
  }
  return count;
}

int score_phrase(const Phrase_definition* phrase, const Phrase_context* context)
{
  int score = 100;
  int word_count = 0;
  if (!phrase_eligible(phrase, context))
  {
    return ineligible_response_score;
  }
  if ((context->response_plan != 0) &&
      (phrase->target_field == context->response_plan->target_field))
  {
    score += 10;
  }
  if ((context->state != 0) &&
      (phrase->department != department_unknown) &&
      (phrase->department == context->state->department))
  {
    score += 6;
  }
  if (phrase->minimum_retry_count == phrase->maximum_retry_count)
  {
    score += 5;
  }
  if (text_contains(phrase->text, "{kb_answer}"))
  {
    score += 40;
  }
  if ((context->state != 0) &&
      response_phrase_recently_used(context->state, phrase->id))
  {
    score -= 50;
  }
  word_count = phrase_word_count(phrase->text);
  if (word_count <= 8)
  {
    score += 5;
  }
  else if (word_count > 14)
  {
    score -= 5;
  }
  return score;
}

unsigned int deterministic_phrase_seed(const Phrase_context* context)
{
  unsigned int hash = 2166136261U;
  int index = 0;
  const char* call_id = "";
  if ((context != 0) && (context->state != 0))
  {
    call_id = context->state->call_id;
  }
  while (call_id[index] != '\0')
  {
    hash ^= static_cast<unsigned int>(static_cast<unsigned char>(call_id[index]));
    hash *= 16777619U;
    index += 1;
  }
  if ((context != 0) && (context->response_plan != 0))
  {
    hash ^= static_cast<unsigned int>(context->response_plan->structure + 1);
    hash *= 16777619U;
  }
  if (context != 0)
  {
    hash ^= static_cast<unsigned int>(context->slot_index + 1);
    hash *= 16777619U;
  }
  if ((context != 0) && (context->state != 0))
  {
    hash ^= static_cast<unsigned int>(context->state->history.turn_count + 1);
    hash *= 16777619U;
  }
  return hash;
}

struct Scored_phrase
{
  const Phrase_definition* phrase;
  int score;
};

static void sort_scored_phrases(Scored_phrase* values, int count)
{
  int outer = 0;
  while (outer < count)
  {
    int inner = outer + 1;
    while (inner < count)
    {
      if ((values[inner].score > values[outer].score) ||
          ((values[inner].score == values[outer].score) &&
           (values[inner].phrase->id < values[outer].phrase->id)))
      {
        const Scored_phrase temporary = values[outer];
        values[outer] = values[inner];
        values[inner] = temporary;
      }
      inner += 1;
    }
    outer += 1;
  }
}

const Phrase_definition* select_phrase(
  Response_act act,
  const Phrase_context* context,
  unsigned int selection_seed)
{
  const Phrase_definition* eligible[16];
  Scored_phrase scored[16];
  int count = collect_eligible_phrases(act, context, eligible, 16);
  int index = 0;
  int shortlist_count = 0;
  unsigned int total_weight = 0U;
  unsigned int choice = 0U;
  const int score_window = 20;
  if (count <= 0)
  {
    return 0;
  }
  while (index < count)
  {
    scored[index].phrase = eligible[index];
    scored[index].score = score_phrase(eligible[index], context);
    index += 1;
  }
  sort_scored_phrases(scored, count);
  while ((shortlist_count < count) &&
         (shortlist_count < 3) &&
         (scored[shortlist_count].score >= (scored[0].score - score_window)))
  {
    total_weight += static_cast<unsigned int>(
      scored[shortlist_count].score - (scored[0].score - score_window) + 1);
    shortlist_count += 1;
  }
  if ((shortlist_count <= 1) || (total_weight == 0U))
  {
    return scored[0].phrase;
  }
  selection_seed = (selection_seed * 1664525U) + 1013904223U;
  choice = selection_seed % total_weight;
  index = 0;
  while (index < shortlist_count)
  {
    const unsigned int weight = static_cast<unsigned int>(
      scored[index].score - (scored[0].score - score_window) + 1);
    if (choice < weight)
    {
      return scored[index].phrase;
    }
    choice -= weight;
    index += 1;
  }
  return scored[0].phrase;
}

static void append_text(char* output, int capacity, const char* text)
{
  int output_length = 0;
  int index = 0;
  if ((output == 0) || (text == 0) || (capacity <= 0))
  {
    return;
  }
  output_length = static_cast<int>(std::strlen(output));
  while ((text[index] != '\0') && (output_length < (capacity - 1)))
  {
    output[output_length] = text[index];
    output_length += 1;
    index += 1;
  }
  output[output_length] = '\0';
}

static const char* placeholder_value(
  const char* placeholder,
  const Phrase_context* context)
{
  const State* state = (context != 0) ? context->state : 0;
  if (std::strcmp(placeholder, "{department}") == 0)
  {
    return department_name((state != 0) ? state->department : department_unknown);
  }
  if (std::strcmp(placeholder, "{request}") == 0)
  {
    return state_field_value(state, field_request);
  }
  if (std::strcmp(placeholder, "{vehicle}") == 0)
  {
    return state_field_value(state, field_vehicle);
  }
  if (std::strcmp(placeholder, "{callback_time}") == 0)
  {
    return state_field_value(state, field_callback_time);
  }
  if (std::strcmp(placeholder, "{phone}") == 0)
  {
    return state_field_value(state, field_phone);
  }
  if (std::strcmp(placeholder, "{name}") == 0)
  {
    return state_field_value(state, field_caller_name);
  }
  if (std::strcmp(placeholder, "{kb_answer}") == 0)
  {
    return ((context != 0) && (context->kb_answer != 0)) ? context->kb_answer : "";
  }
  return "";
}

bool render_phrase(
  const Phrase_definition* phrase,
  const Phrase_context* context,
  char* output,
  int capacity)
{
  int index = 0;
  if ((output == 0) || (capacity <= 0))
  {
    return false;
  }
  output[0] = '\0';
  if (!phrase_eligible(phrase, context))
  {
    return false;
  }
  while (phrase->text[index] != '\0')
  {
    if (phrase->text[index] == '{')
    {
      char placeholder[32];
      int placeholder_length = 0;
      while ((phrase->text[index] != '\0') &&
             (placeholder_length < 31))
      {
        placeholder[placeholder_length] = phrase->text[index];
        placeholder_length += 1;
        if (phrase->text[index] == '}')
        {
          index += 1;
          break;
        }
        index += 1;
      }
      placeholder[placeholder_length] = '\0';
      append_text(output, capacity, placeholder_value(placeholder, context));
    }
    else
    {
      char character[2];
      character[0] = phrase->text[index];
      character[1] = '\0';
      append_text(output, capacity, character);
      index += 1;
    }
  }
  return output[0] != '\0';
}

}
