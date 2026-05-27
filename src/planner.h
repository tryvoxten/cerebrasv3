#ifndef cerebras_v3_planner_h
#define cerebras_v3_planner_h

namespace cerebras_v3
{
const int max_text = 256;

enum Department
{
  department_unknown = 0,
  department_service = 1,
  department_parts = 2,
  department_sales = 3
};

enum Field_id
{
  field_department = 0,
  field_intent = 1,
  field_caller_name = 2,
  field_last_name_spelling = 3,
  field_vehicle = 4,
  field_request = 5,
  field_callback_time = 6,
  field_phone = 7,
  field_phone_confirmed = 8,
  field_final_confirmed = 9,
  field_none = 10
};

enum Field_status
{
  status_missing = 0,
  status_captured = 1,
  status_unclear = 2,
  status_refused = 3
};

struct Field
{
  char value[max_text];
  Field_status status;
  int confidence;
  bool confirmed;
};

struct State
{
  Department department;
  Field fields[10];
  Field_id last_requested;
  bool delivery_sent;
};

struct Interpretation
{
  char department[32];
  char intent[max_text];
  char vehicle[max_text];
  char request[max_text];
  char callback_time[max_text];
  char phone[max_text];
  char name[max_text];
  char spelling[max_text];
  char faq_question[max_text];
  char faq_id[64];
  char affirmation[32];
};

struct Plan
{
  Field_id next_field;
  const char* response_task;
  const char* fallback_sentence;
  bool complete;
};

void init_state(State* state);
void clear_interpretation(Interpretation* interpretation);
void merge_interpretation(State* state, const Interpretation* interpretation, const char* caller_text);
Plan plan_next(const State* state);
const char* field_label(Field_id field);
const char* department_name(Department department);
void copy_text(char* destination, const char* source, int capacity);
bool parse_interpretation_json(const char* json, Interpretation* interpretation);
void state_to_json(const State* state, char* output, int capacity);
void load_state_from_json(State* state, const char* json);
void extract_state_json_from_request(const char* request, char* output, int capacity);

}

#endif
