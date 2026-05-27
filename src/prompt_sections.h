#ifndef cerebras_v3_prompt_sections_h
#define cerebras_v3_prompt_sections_h

namespace cerebras_v3
{
namespace prompt_sections
{

static const char* interpreter_role =
  "Classify dealership caller text.";

static const char* interpreter_schema =
  "Return only minified JSON: {\"d\":\"\",\"i\":\"\",\"v\":\"\",\"r\":\"\",\"cb\":\"\",\"p\":\"\",\"n\":\"\",\"s\":\"\",\"q\":\"\",\"f\":\"none\",\"a\":\"none\"}.";

static const char* interpreter_field_rules =
  "d values: service=repair/maintenance/diagnostic/recall/warranty/vehicle problem; parts=part/accessory/order/availability; sales=buy/lease/test drive/inventory/price/trade; unknown=unclear. i=short intent. v=vehicle if mentioned. r=specific request/problem/part/vehicle interest in 3-10 words. cb=callback time. p=phone. n=caller full name. s=last-name spelling. q=caller FAQ question if they ask one.";

static const char* interpreter_output_rules =
  "Never invent f or a. If unsure use none. Use empty strings if missing. No extra keys. No explanation.";

}
}

#endif
