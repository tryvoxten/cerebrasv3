#ifndef cerebras_v3_server_runtime_h
#define cerebras_v3_server_runtime_h

#include <planner.h>

const int request_capacity = 8192;
const int response_capacity = 8192;
const int text_capacity = 1024;
const int cerebras_capacity = 8192;
const int cerebras_payload_capacity = 16384;
const int cerebras_system_capacity = 8192;
const int context_capacity = 768;
const int summary_capacity = 4096;
const int websocket_capacity = 262144;
const int default_port = 8080;

struct Config
{
  int port;
  char shared_secret[128];
  char cerebras_key[256];
  char cerebras_model[128];
  char cerebras_url[256];
  char delivery_webhook_url[256];
  char delivery_webhook_secret[128];
  bool cerebras_debug;
  bool structured_responses;
  bool ai_response_slots;
};

struct Turn_result
{
  char response_text[text_capacity];
  char state_json[2048];
  char employee_summary[summary_capacity];
  char next_field[64];
  char turn_type[64];
  char answered_field[64];
  char faq_id[64];
  char affirmation[32];
  bool used_interpreter;
  bool used_generator;
  bool used_kb_answer;
  bool delivery_attempted;
  bool delivery_sent;
  bool end_call;
};

void clear_buffer(char* buffer, int capacity);
void clear_turn_result(Turn_result* result);
void load_config(char** envp, Config* config);
void set_call_id_from_websocket_path(cerebras_v3::State* state, const char* request, const Config* config);
bool websocket_read_text(int fd, char* output, int capacity, int* opcode);
void process_chat_turn(
  cerebras_v3::State* state,
  const Config* config,
  const char* message,
  const char* last_assistant,
  const char* recent_context,
  Turn_result* result);
void build_retell_response_json(
  char* response,
  int capacity,
  int response_id,
  const char* call_id,
  const char* content,
  bool end_call);
bool try_structured_response(
  cerebras_v3::State* state,
  const Config* config,
  const cerebras_v3::Plan* plan,
  const cerebras_v3::Interpretation* interpretation,
  cerebras_v3::Field_id answered_field,
  const char* latest_message,
  Turn_result* result);
void correct_faq_id_from_message(const char* message, cerebras_v3::Interpretation* interpretation);

#endif
