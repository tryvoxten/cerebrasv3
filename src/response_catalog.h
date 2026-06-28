#ifndef cerebras_v3_response_catalog_h
#define cerebras_v3_response_catalog_h

#include <response_policy.h>

namespace cerebras_v3
{
enum Phrase_variant
{
  phrase_variant_any = 0,
  phrase_variant_initial_question = 1,
  phrase_variant_retry_question = 2,
  phrase_variant_confirmation_question = 3
};

struct Phrase_definition
{
  int id;
  Response_act act;
  Field_id target_field;
  Department department;
  Phrase_variant variant;
  int minimum_retry_count;
  int maximum_retry_count;
  const char* text;
};

struct Phrase_context
{
  const State* state;
  const Response_plan* response_plan;
  const char* kb_answer;
  int slot_index;
};

void init_phrase_context(Phrase_context* context);
int phrase_catalog_size(void);
const Phrase_definition* phrase_definition_at(int index);
const Phrase_definition* find_phrase_definition(int phrase_id);
bool phrase_eligible(const Phrase_definition* phrase, const Phrase_context* context);
int collect_eligible_phrases(
  Response_act act,
  const Phrase_context* context,
  const Phrase_definition** output,
  int capacity);
int score_phrase(const Phrase_definition* phrase, const Phrase_context* context);
unsigned int deterministic_phrase_seed(const Phrase_context* context);
const Phrase_definition* select_phrase(
  Response_act act,
  const Phrase_context* context,
  unsigned int selection_seed);
bool render_phrase(
  const Phrase_definition* phrase,
  const Phrase_context* context,
  char* output,
  int capacity);

}

#endif
