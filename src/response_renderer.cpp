#include <response_renderer.h>
#include <cctype>
#include <cstring>

namespace cerebras_v3
{
static void clear_text(char* text, int capacity)
{
  if ((text != 0) && (capacity > 0))
  {
    text[0] = '\0';
  }
}

static bool append_text(char* output, int capacity, const char* text)
{
  int length = 0;
  int index = 0;
  if ((output == 0) || (text == 0) || (capacity <= 0))
  {
    return false;
  }
  length = static_cast<int>(std::strlen(output));
  while ((text[index] != '\0') && (length < (capacity - 1)))
  {
    output[length] = text[index];
    length += 1;
    index += 1;
  }
  output[length] = '\0';
  return text[index] == '\0';
}

static bool append_slot(char* output, int capacity, const char* slot)
{
  char adjusted[rendered_response_capacity];
  int length = 0;
  if ((slot == 0) || (slot[0] == '\0'))
  {
    return false;
  }
  copy_text(adjusted, slot, rendered_response_capacity);
  length = static_cast<int>(std::strlen(output));
  if ((length > 0) && !append_text(output, capacity, " "))
  {
    return false;
  }
  if ((length > 0) &&
      (output[length - 1] == ',') &&
      (adjusted[0] != 'I'))
  {
    adjusted[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(adjusted[0])));
  }
  return append_text(output, capacity, adjusted);
}

void init_response_render_options(Response_render_options* options)
{
  if (options == 0)
  {
    return;
  }
  options->kb_answer = "";
  options->previous_response = "";
  options->enable_ai_slots = false;
  options->ai_generator = 0;
  options->ai_user_data = 0;
}

void init_response_render_result(Response_render_result* result)
{
  int index = 0;
  if (result == 0)
  {
    return;
  }
  clear_text(result->text, rendered_response_capacity);
  init_response_plan(&result->plan, field_none);
  while (index < max_response_acts)
  {
    result->phrase_ids[index] = 0;
    index += 1;
  }
  result->phrase_count = 0;
  result->used_ai = false;
  result->validation_error = response_validation_ok;
}

bool render_structured_response(
  State* state,
  const Response_context* context,
  const Response_render_options* options,
  Response_render_result* result)
{
  Response_render_options defaults;
  Phrase_context phrase_context;
  Response_validation_result validation;
  unsigned int plan_seed = 0U;
  int slot_index = 0;
  if ((state == 0) || (context == 0) || (result == 0))
  {
    return false;
  }
  init_response_render_result(result);
  init_response_render_options(&defaults);
  if (options == 0)
  {
    options = &defaults;
  }
  plan_seed = deterministic_response_seed(context);
  if (!build_response_plan(context, plan_seed, &result->plan))
  {
    return false;
  }
  init_phrase_context(&phrase_context);
  phrase_context.state = state;
  phrase_context.response_plan = &result->plan;
  phrase_context.kb_answer = options->kb_answer;

  while (slot_index < result->plan.act_count)
  {
    const Response_act act = result->plan.acts[slot_index];
    char slot[rendered_response_capacity];
    bool slot_ready = false;
    clear_text(slot, rendered_response_capacity);
    phrase_context.slot_index = slot_index;
    if (options->enable_ai_slots &&
        ai_slot_act_allowed(act) &&
        (options->ai_generator != 0))
    {
      slot_ready = options->ai_generator(
        act,
        result->plan.target_field,
        state,
        slot,
        rendered_response_capacity,
        options->ai_user_data);
      if (slot_ready)
      {
        validation = validate_ai_slot(slot, act, state);
        slot_ready = validation.valid;
      }
      if (slot_ready)
      {
        result->used_ai = true;
      }
    }
    if (!slot_ready)
    {
      const Phrase_definition* phrase = select_phrase(
        act,
        &phrase_context,
        deterministic_phrase_seed(&phrase_context));
      if ((phrase == 0) ||
          !render_phrase(phrase, &phrase_context, slot, rendered_response_capacity))
      {
        return false;
      }
      result->phrase_ids[result->phrase_count] = phrase->id;
      result->phrase_count += 1;
    }
    if (!append_slot(result->text, rendered_response_capacity, slot))
    {
      return false;
    }
    slot_index += 1;
  }

  validation = validate_composed_response(
    result->text,
    &result->plan,
    state,
    options->previous_response);
  result->validation_error = validation.error;
  if (!validation.valid)
  {
    return false;
  }
  record_response_structure(state, result->plan.structure);
  slot_index = 0;
  while (slot_index < result->phrase_count)
  {
    record_response_phrase(state, result->phrase_ids[slot_index]);
    slot_index += 1;
  }
  if (result->plan.act_count > 0)
  {
    state->history.last_response_act = static_cast<int>(result->plan.acts[0]);
  }
  return true;
}
}
