#ifndef cerebras_v3_cerebras_client_h
#define cerebras_v3_cerebras_client_h

#include <server_runtime.h>

bool call_cerebras(
  const Config* config,
  const char* system,
  const char* user,
  int max_tokens,
  bool json_mode,
  char* output,
  int capacity);

#endif
