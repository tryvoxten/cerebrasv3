#ifndef cerebras_v3_response_renderer_h
#define cerebras_v3_response_renderer_h

#include <response_catalog.h>
#include <response_validator.h>

namespace cerebras_v3
{
const int rendered_response_capacity = 1024;

typedef bool (*Ai_slot_generator)(
  Response_act act,
  Field_id target_field,
  const State* state,
  char* output,
  int capacity,
  void* user_data);

struct Response_render_options
{
  const char* kb_answer;
  const char* previous_response;
  bool enable_ai_slots;
  Ai_slot_generator ai_generator;
  void* ai_user_data;
};

struct Response_render_result
{
  char text[rendered_response_capacity];
  Response_plan plan;
  int phrase_ids[max_response_acts];
  int phrase_count;
  bool used_ai;
  Response_validation_error validation_error;
};

void init_response_render_options(Response_render_options* options);
void init_response_render_result(Response_render_result* result);
bool render_structured_response(
  State* state,
  const Response_context* context,
  const Response_render_options* options,
  Response_render_result* result);
}

#endif
