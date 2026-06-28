#ifndef cerebras_v3_planner_h
#define cerebras_v3_planner_h

namespace cerebras_v3
{
const int max_text = 256;
const int tracked_field_count = 10;
const int max_recent_structures = 3;
const int max_recent_phrase_ids = 8;

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

enum Conversation_phase
{
  conversation_phase_opening = 0,
  conversation_phase_discovery = 1,
  conversation_phase_contact = 2,
  conversation_phase_confirmation = 3,
  conversation_phase_complete = 4
};

enum Caller_pace
{
  caller_pace_unknown = 0,
  caller_pace_normal = 1,
  caller_pace_rushed = 2
};

struct Field
{
  char value[max_text];
  Field_status status;
  int confidence;
  bool confirmed;
};

struct Conversation_history
{
  int turn_count;
  int retry_counts[tracked_field_count];
  int recent_structure_ids[max_recent_structures];
  int recent_structure_count;
  int recent_phrase_ids[max_recent_phrase_ids];
  int recent_phrase_count;
  int last_response_act;
  Conversation_phase phase;
  Field_id interrupted_field;
  Caller_pace caller_pace;
  bool caller_confused;
};

struct State
{
  char call_id[64];
  Department department;
  Field fields[tracked_field_count];
  Field_id last_requested;
  bool delivery_sent;
  Conversation_history history;
};

struct Interpretation
{
  char turn_type[64];
  char answered_field[64];
  char meaning[max_text];
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
