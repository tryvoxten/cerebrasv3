#ifndef voxten_employee_delivery_h
#define voxten_employee_delivery_h

#include <planner.h>
#include <server_runtime.h>

void build_employee_summary_json(
  const cerebras_v3::State* state,
  char* output,
  int capacity);
bool deliver_employee_summary(const Config* config, const char* summary_json);

#endif
