#ifndef cerebras_v3_caller_number_h
#define cerebras_v3_caller_number_h

#include <planner.h>
#include <server_runtime.h>

void caller_number_from_retell_details(
  const char* event,
  char* output,
  int capacity);

bool handle_calling_number_request(
  cerebras_v3::State* state,
  const char* caller_text,
  const char* retell_caller_number,
  Turn_result* result);

#endif
