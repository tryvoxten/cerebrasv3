#ifndef cerebras_v3_response_policy_h
#define cerebras_v3_response_policy_h

#include <planner.h>

namespace cerebras_v3
{
const int max_response_acts = 4;
const int ineligible_response_score = -100000;

enum Response_act
{
  response_act_none = 0,
  response_act_acknowledge = 1,
  response_act_answer = 2,
  response_act_clarify = 3,
  response_act_confirm_correction = 4,
  response_act_transition = 5,
  response_act_ask = 6,
  response_act_readback = 7,
  response_act_close = 8
};

enum Response_structure
{
  response_structure_none = 0,
  response_structure_ask = 1,
  response_structure_acknowledge_ask = 2,
  response_structure_acknowledge_transition_ask = 3,
  response_structure_answer_ask = 4,
  response_structure_answer_transition_ask = 5,
  response_structure_clarify_ask = 6,
  response_structure_confirm_correction_ask = 7,
  response_structure_confirm_correction_readback_ask = 8,
  response_structure_readback_ask = 9,
  response_structure_close = 10
};

struct Response_plan
{
  Response_act acts[max_response_acts];
  int act_count;
  Field_id target_field;
  Response_structure structure;
  int retry_count;
  bool requires_question;
  bool complete;
};

struct Response_context
{
  const State* state;
  const Plan* field_plan;
  const Interpretation* interpretation;
  Field_id previous_requested;
  bool has_kb_answer;
  bool has_grounded_acknowledgement;
};

struct Response_structure_definition
{
  Response_structure structure;
  Response_act acts[max_response_acts];
  int act_count;
  bool requires_question;
};

void init_response_plan(Response_plan* plan, Field_id target_field);
void init_response_context(Response_context* context);
bool append_response_act(Response_plan* plan, Response_act act);
const char* response_act_label(Response_act act);
const char* response_structure_label(Response_structure structure);
int response_structure_catalog_size(void);
const Response_structure_definition* response_structure_definition_at(int index);
const Response_structure_definition* find_response_structure_definition(Response_structure structure);
bool response_structure_eligible(Response_structure structure, const Response_context* context);
int collect_eligible_response_structures(
  const Response_context* context,
  Response_structure* output,
  int capacity);
int score_response_structure(Response_structure structure, const Response_context* context);
unsigned int deterministic_response_seed(const Response_context* context);
Response_structure select_response_structure(
  const Response_context* context,
  unsigned int selection_seed);
bool build_response_plan(
  const Response_context* context,
  unsigned int selection_seed,
  Response_plan* output);
void record_response_structure(State* state, Response_structure structure);
void record_response_phrase(State* state, int phrase_id);
bool response_structure_recently_used(const State* state, Response_structure structure);
bool response_phrase_recently_used(const State* state, int phrase_id);

}

#endif
