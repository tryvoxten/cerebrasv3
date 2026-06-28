#include <response_policy.h>
#include <cstring>

namespace cerebras_v3
{
void init_response_plan(Response_plan* plan, Field_id target_field)
{
  int index = 0;
  if (plan == 0)
  {
    return;
  }
  while (index < max_response_acts)
  {
    plan->acts[index] = response_act_none;
    index += 1;
  }
  plan->act_count = 0;
  plan->target_field = target_field;
  plan->structure = response_structure_none;
  plan->retry_count = 0;
  plan->requires_question = false;
  plan->complete = false;
}

void init_response_context(Response_context* context)
{
  if (context == 0)
  {
    return;
  }
  context->state = 0;
  context->field_plan = 0;
  context->interpretation = 0;
  context->previous_requested = field_none;
  context->has_kb_answer = false;
  context->has_grounded_acknowledgement = false;
}

bool append_response_act(Response_plan* plan, Response_act act)
{
  if ((plan == 0) ||
      (act == response_act_none) ||
      (plan->act_count < 0) ||
      (plan->act_count >= max_response_acts))
  {
    return false;
  }
  plan->acts[plan->act_count] = act;
  plan->act_count += 1;
  if (act == response_act_ask)
  {
    plan->requires_question = true;
  }
  return true;
}

const char* response_act_label(Response_act act)
{
  switch (act)
  {
    case response_act_acknowledge: return "acknowledge";
    case response_act_answer: return "answer";
    case response_act_clarify: return "clarify";
    case response_act_confirm_correction: return "confirm_correction";
    case response_act_transition: return "transition";
    case response_act_ask: return "ask";
    case response_act_readback: return "readback";
    case response_act_close: return "close";
    default: return "none";
  }
}

const char* response_structure_label(Response_structure structure)
{
  switch (structure)
  {
    case response_structure_ask: return "ask";
    case response_structure_acknowledge_ask: return "acknowledge_ask";
    case response_structure_acknowledge_transition_ask: return "acknowledge_transition_ask";
    case response_structure_answer_ask: return "answer_ask";
    case response_structure_answer_transition_ask: return "answer_transition_ask";
    case response_structure_clarify_ask: return "clarify_ask";
    case response_structure_confirm_correction_ask: return "confirm_correction_ask";
    case response_structure_confirm_correction_readback_ask: return "confirm_correction_readback_ask";
    case response_structure_readback_ask: return "readback_ask";
    case response_structure_close: return "close";
    default: return "none";
  }
}

static const Response_structure_definition response_catalog[] =
{
  {
    response_structure_ask,
    {response_act_ask, response_act_none, response_act_none, response_act_none},
    1,
    true
  },
  {
    response_structure_acknowledge_ask,
    {response_act_acknowledge, response_act_ask, response_act_none, response_act_none},
    2,
    true
  },
  {
    response_structure_acknowledge_transition_ask,
    {response_act_acknowledge, response_act_transition, response_act_ask, response_act_none},
    3,
    true
  },
  {
    response_structure_answer_ask,
    {response_act_answer, response_act_ask, response_act_none, response_act_none},
    2,
    true
  },
  {
    response_structure_answer_transition_ask,
    {response_act_answer, response_act_transition, response_act_ask, response_act_none},
    3,
    true
  },
  {
    response_structure_clarify_ask,
    {response_act_clarify, response_act_ask, response_act_none, response_act_none},
    2,
    true
  },
  {
    response_structure_confirm_correction_ask,
    {response_act_confirm_correction, response_act_ask, response_act_none, response_act_none},
    2,
    true
  },
  {
    response_structure_confirm_correction_readback_ask,
    {response_act_confirm_correction, response_act_readback, response_act_ask, response_act_none},
    3,
    true
  },
  {
    response_structure_readback_ask,
    {response_act_readback, response_act_ask, response_act_none, response_act_none},
    2,
    true
  },
  {
    response_structure_close,
    {response_act_close, response_act_none, response_act_none, response_act_none},
    1,
    false
  }
};

int response_structure_catalog_size(void)
{
  return static_cast<int>(sizeof(response_catalog) / sizeof(response_catalog[0]));
}

const Response_structure_definition* response_structure_definition_at(int index)
{
  if ((index < 0) || (index >= response_structure_catalog_size()))
  {
    return 0;
  }
  return &response_catalog[index];
}

const Response_structure_definition* find_response_structure_definition(Response_structure structure)
{
  int index = 0;
  while (index < response_structure_catalog_size())
  {
    if (response_catalog[index].structure == structure)
    {
      return &response_catalog[index];
    }
    index += 1;
  }
  return 0;
}

static bool context_turn_is(const Response_context* context, const char* turn_type)
{
  return
    (context != 0) &&
    (context->interpretation != 0) &&
    (turn_type != 0) &&
    (std::strcmp(context->interpretation->turn_type, turn_type) == 0);
}

static bool context_has_question_target(const Response_context* context)
{
  return
    (context != 0) &&
    (context->field_plan != 0) &&
    !context->field_plan->complete &&
    (context->field_plan->next_field != field_none);
}

static int context_retry_count(const Response_context* context)
{
  Field_id field = field_none;
  if (!context_has_question_target(context) || (context->state == 0))
  {
    return 0;
  }
  field = context->field_plan->next_field;
  if ((field < field_department) || (field >= field_none))
  {
    return 0;
  }
  return context->state->history.retry_counts[field];
}

static bool context_requires_readback(const Response_context* context)
{
  Field_id target = field_none;
  if (!context_has_question_target(context) || (context->state == 0))
  {
    return false;
  }
  target = context->field_plan->next_field;
  return
    ((target == field_callback_time) &&
     (context->state->fields[field_callback_time].status == status_captured) &&
     !context->state->fields[field_callback_time].confirmed) ||
    (target == field_phone_confirmed) ||
    (target == field_final_confirmed);
}

static bool correction_requires_readback(const Response_context* context)
{
  if (!context_turn_is(context, "correction"))
  {
    return false;
  }
  return
    (std::strcmp(context->interpretation->answered_field, "callback_time") == 0) ||
    (std::strcmp(context->interpretation->answered_field, "phone") == 0);
}

static bool context_is_repair(const Response_context* context)
{
  return
    context_turn_is(context, "customer_confusion") ||
    context_turn_is(context, "unclear_audio") ||
    context_turn_is(context, "off_topic") ||
    (context_retry_count(context) > 0);
}

bool response_structure_eligible(Response_structure structure, const Response_context* context)
{
  const bool complete =
    (context != 0) &&
    (context->field_plan != 0) &&
    context->field_plan->complete;
  const bool correction = context_turn_is(context, "correction");
  const bool caller_question = context_turn_is(context, "caller_question");
  const bool repair = context_is_repair(context);
  const bool readback = context_requires_readback(context);
  if (structure == response_structure_close)
  {
    return complete;
  }
  if (!context_has_question_target(context) || complete)
  {
    return false;
  }
  if (correction)
  {
    if (correction_requires_readback(context))
    {
      return structure == response_structure_confirm_correction_readback_ask;
    }
    return structure == response_structure_confirm_correction_ask;
  }
  if (caller_question)
  {
    return
      (structure == response_structure_answer_ask) ||
      (structure == response_structure_answer_transition_ask);
  }
  if (readback)
  {
    return structure == response_structure_readback_ask;
  }
  if (repair)
  {
    return structure == response_structure_clarify_ask;
  }
  if ((structure == response_structure_acknowledge_ask) ||
      (structure == response_structure_acknowledge_transition_ask))
  {
    return context->has_grounded_acknowledgement;
  }
  return structure == response_structure_ask;
}

int collect_eligible_response_structures(
  const Response_context* context,
  Response_structure* output,
  int capacity)
{
  int catalog_index = 0;
  int output_count = 0;
  if ((output == 0) || (capacity <= 0))
  {
    return 0;
  }
  while ((catalog_index < response_structure_catalog_size()) &&
         (output_count < capacity))
  {
    if (response_structure_eligible(response_catalog[catalog_index].structure, context))
    {
      output[output_count] = response_catalog[catalog_index].structure;
      output_count += 1;
    }
    catalog_index += 1;
  }
  return output_count;
}

static bool definition_contains_act(
  const Response_structure_definition* definition,
  Response_act act)
{
  int index = 0;
  if (definition == 0)
  {
    return false;
  }
  while (index < definition->act_count)
  {
    if (definition->acts[index] == act)
    {
      return true;
    }
    index += 1;
  }
  return false;
}

int score_response_structure(Response_structure structure, const Response_context* context)
{
  const Response_structure_definition* definition =
    find_response_structure_definition(structure);
  int score = 100;
  if ((definition == 0) || !response_structure_eligible(structure, context))
  {
    return ineligible_response_score;
  }

  score += (max_response_acts - definition->act_count) * 3;

  if (definition_contains_act(definition, response_act_acknowledge))
  {
    score += context->has_grounded_acknowledgement ? 12 : -30;
  }
  if (definition_contains_act(definition, response_act_answer))
  {
    score += context->has_kb_answer ? 10 : 4;
  }
  if (definition_contains_act(definition, response_act_transition))
  {
    if (context_turn_is(context, "caller_question"))
    {
      score += 4;
    }
    else if ((context->previous_requested != field_none) &&
             (context->field_plan != 0) &&
             (context->previous_requested != context->field_plan->next_field))
    {
      score += 6;
    }
    else
    {
      score -= 2;
    }
  }
  if ((context->state != 0) &&
      response_structure_recently_used(context->state, structure))
  {
    score -= 40;
  }
  if ((context->state != 0) &&
      (definition->act_count > 0) &&
      (context->state->history.last_response_act == static_cast<int>(definition->acts[0])))
  {
    score -= 8;
  }
  return score;
}

unsigned int deterministic_response_seed(const Response_context* context)
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
  if ((context != 0) && (context->state != 0))
  {
    hash ^= static_cast<unsigned int>(context->state->history.turn_count + 1);
    hash *= 16777619U;
  }
  if ((context != 0) && (context->field_plan != 0))
  {
    hash ^= static_cast<unsigned int>(context->field_plan->next_field + 1);
    hash *= 16777619U;
  }
  return hash;
}

struct Scored_structure
{
  Response_structure structure;
  int score;
};

static void sort_scored_structures(Scored_structure* values, int count)
{
  int outer = 0;
  if (values == 0)
  {
    return;
  }
  while (outer < count)
  {
    int inner = outer + 1;
    while (inner < count)
    {
      if ((values[inner].score > values[outer].score) ||
          ((values[inner].score == values[outer].score) &&
           (values[inner].structure < values[outer].structure)))
      {
        const Scored_structure temporary = values[outer];
        values[outer] = values[inner];
        values[inner] = temporary;
      }
      inner += 1;
    }
    outer += 1;
  }
}

Response_structure select_response_structure(
  const Response_context* context,
  unsigned int selection_seed)
{
  Response_structure eligible[10];
  Scored_structure scored[10];
  int eligible_count = 0;
  int shortlist_count = 0;
  int index = 0;
  unsigned int total_weight = 0U;
  unsigned int choice = 0U;
  const int shortlist_score_window = 24;
  const int shortlist_limit = 3;

  eligible_count = collect_eligible_response_structures(context, eligible, 10);
  if (eligible_count <= 0)
  {
    return response_structure_none;
  }
  while (index < eligible_count)
  {
    scored[index].structure = eligible[index];
    scored[index].score = score_response_structure(eligible[index], context);
    index += 1;
  }
  sort_scored_structures(scored, eligible_count);
  while ((shortlist_count < eligible_count) &&
         (shortlist_count < shortlist_limit) &&
         (scored[shortlist_count].score >= (scored[0].score - shortlist_score_window)))
  {
    total_weight += static_cast<unsigned int>(
      scored[shortlist_count].score - (scored[0].score - shortlist_score_window) + 1);
    shortlist_count += 1;
  }
  if ((shortlist_count <= 1) || (total_weight == 0U))
  {
    return scored[0].structure;
  }
  selection_seed = (selection_seed * 1664525U) + 1013904223U;
  choice = selection_seed % total_weight;
  index = 0;
  while (index < shortlist_count)
  {
    const unsigned int weight = static_cast<unsigned int>(
      scored[index].score - (scored[0].score - shortlist_score_window) + 1);
    if (choice < weight)
    {
      return scored[index].structure;
    }
    choice -= weight;
    index += 1;
  }
  return scored[0].structure;
}

bool build_response_plan(
  const Response_context* context,
  unsigned int selection_seed,
  Response_plan* output)
{
  const Response_structure_definition* definition = 0;
  Response_structure selected = response_structure_none;
  int index = 0;
  Field_id target = field_none;
  if ((context == 0) || (context->field_plan == 0) || (output == 0))
  {
    return false;
  }
  target = context->field_plan->next_field;
  init_response_plan(output, target);
  selected = select_response_structure(context, selection_seed);
  definition = find_response_structure_definition(selected);
  if (definition == 0)
  {
    return false;
  }
  output->structure = selected;
  output->retry_count = context_retry_count(context);
  output->requires_question = definition->requires_question;
  output->complete = context->field_plan->complete;
  while (index < definition->act_count)
  {
    if (!append_response_act(output, definition->acts[index]))
    {
      return false;
    }
    index += 1;
  }
  return true;
}

static void append_recent_id(int* values, int capacity, int* count, int value)
{
  int index = 0;
  if ((values == 0) || (count == 0) || (capacity <= 0) || (value <= 0))
  {
    return;
  }
  if (*count < capacity)
  {
    values[*count] = value;
    *count += 1;
    return;
  }
  while (index < (capacity - 1))
  {
    values[index] = values[index + 1];
    index += 1;
  }
  values[capacity - 1] = value;
}

static bool recent_id_contains(const int* values, int count, int value)
{
  int index = 0;
  if ((values == 0) || (value <= 0))
  {
    return false;
  }
  while (index < count)
  {
    if (values[index] == value)
    {
      return true;
    }
    index += 1;
  }
  return false;
}

void record_response_structure(State* state, Response_structure structure)
{
  if ((state == 0) || (structure == response_structure_none))
  {
    return;
  }
  append_recent_id(
    state->history.recent_structure_ids,
    max_recent_structures,
    &state->history.recent_structure_count,
    static_cast<int>(structure));
}

void record_response_phrase(State* state, int phrase_id)
{
  if (state == 0)
  {
    return;
  }
  append_recent_id(
    state->history.recent_phrase_ids,
    max_recent_phrase_ids,
    &state->history.recent_phrase_count,
    phrase_id);
}

bool response_structure_recently_used(const State* state, Response_structure structure)
{
  return
    (state != 0) &&
    recent_id_contains(
      state->history.recent_structure_ids,
      state->history.recent_structure_count,
      static_cast<int>(structure));
}

bool response_phrase_recently_used(const State* state, int phrase_id)
{
  return
    (state != 0) &&
    recent_id_contains(
      state->history.recent_phrase_ids,
      state->history.recent_phrase_count,
      phrase_id);
}

}
