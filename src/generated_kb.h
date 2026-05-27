#ifndef cerebras_v3_generated_kb_h
#define cerebras_v3_generated_kb_h

namespace cerebras_v3
{
namespace generated_kb
{

struct Faq_entry
{
  const char* id;
  const char* description;
  const char* answer;
};

struct Faq_alias
{
  const char* phrase;
  const char* faq_id;
};

extern const Faq_entry faq_entries[];
extern const int faq_entry_count;
extern const Faq_alias faq_aliases[];
extern const int faq_alias_count;
extern const char* affirmation_yes_phrases[];
extern const int affirmation_yes_count;
extern const char* affirmation_no_phrases[];
extern const int affirmation_no_count;
extern const char* affirmation_unclear_phrases[];
extern const int affirmation_unclear_count;
extern const char* vehicle_models[];
extern const int vehicle_model_count;
extern const char* interpreter_faq_rules;
extern const char* interpreter_affirmation_rules;

}
}

#endif
