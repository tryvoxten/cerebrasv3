#include "generated_kb.h"

namespace cerebras_v3
{
namespace generated_kb
{

const Faq_entry faq_entries[] =
{
  { "recall_service", "recall, manufacturer recall, open recall, recall notice", "The service team can help review recall concerns and confirm next steps when they call back." },
  { "warranty_service", "warranty, manufacturer warranty, warranty coverage", "The service team can review warranty questions and confirm what applies when they call back." },
  { "ev_battery_service", "EV battery, high-voltage battery, electric car battery warning", "The service team can review EV battery concerns and confirm next steps when they call back." },
  { "service_hours", "service hours, when service opens or closes, service department schedule", "The team can confirm current service hours when they call back." },
  { "parts_availability", "whether a part can be checked or is available", "The parts team can check availability and confirm options when they call back." },
  { "parts_order_status", "already ordered part status, ordered part arrival, parts order update", "The parts team can review order status and follow up with the latest update." },
  { "sales_inventory", "in stock, on the lot, available vehicles, current inventory", "The sales team can confirm current inventory and follow up with available options." },
  { "financing", "financing, lease rates, monthly payments, credit, approval", "The sales team can review financing questions and follow up with available options." },
  { "trade_in", "trade-in, appraisal, value of current vehicle", "The sales team can review trade-in details and follow up with next steps." },
};
const int faq_entry_count = sizeof(faq_entries) / sizeof(faq_entries[0]);

const Faq_alias faq_aliases[] =
{
  { "recall", "recall_service" },
  { "recall notice", "recall_service" },
  { "open recall", "recall_service" },
  { "manufacturer recall", "recall_service" },
  { "warranty", "warranty_service" },
  { "under warranty", "warranty_service" },
  { "manufacturer warranty", "warranty_service" },
  { "covered under warranty", "warranty_service" },
  { "ev battery", "ev_battery_service" },
  { "electric car battery", "ev_battery_service" },
  { "high-voltage battery", "ev_battery_service" },
  { "high voltage battery", "ev_battery_service" },
  { "ioniq battery", "ev_battery_service" },
  { "electric battery", "ev_battery_service" },
  { "service hours", "service_hours" },
  { "service open", "service_hours" },
  { "service opens", "service_hours" },
  { "service close", "service_hours" },
  { "service closes", "service_hours" },
  { "open on saturday", "service_hours" },
  { "open sundays", "service_hours" },
  { "in stock", "parts_availability" },
  { "available", "parts_availability" },
  { "parts check", "parts_availability" },
  { "do you have", "parts_availability" },
  { "do y'all stock", "parts_availability" },
  { "do you stock", "parts_availability" },
  { "carry", "parts_availability" },
  { "parts order", "parts_order_status" },
  { "ordered part", "parts_order_status" },
  { "part arrived", "parts_order_status" },
  { "special order", "parts_order_status" },
  { "order status", "parts_order_status" },
  { "update on an order", "parts_order_status" },
  { "on the lot", "sales_inventory" },
  { "available vehicles", "sales_inventory" },
  { "current inventory", "sales_inventory" },
  { "in stock", "sales_inventory" },
  { "electric hyundai", "sales_inventory" },
  { "electric hyundais", "sales_inventory" },
  { "ioniq", "sales_inventory" },
  { "tucson hybrid", "sales_inventory" },
  { "lease rate", "financing" },
  { "lease rates", "financing" },
  { "monthly payment", "financing" },
  { "monthly payments", "financing" },
  { "financing", "financing" },
  { "finance", "financing" },
  { "credit", "financing" },
  { "approval", "financing" },
  { "trade-in", "trade_in" },
  { "trade in", "trade_in" },
  { "trade-ins", "trade_in" },
  { "trade ins", "trade_in" },
  { "appraise", "trade_in" },
  { "appraisal", "trade_in" },
  { "current vehicle", "trade_in" },
  { "current car", "trade_in" },
  { "trade is worth", "trade_in" },
};
const int faq_alias_count = sizeof(faq_aliases) / sizeof(faq_aliases[0]);

const char* affirmation_yes_phrases[] =
{
  "yes",
  "correct",
  "yup",
  "yep",
  "yeah",
  "right",
  "that's right",
  "that is right",
  "all correct",
  "that is mine",
  "that's mine",
  "yep that is mine",
  "yup that is mine",
  "looks right",
  "sounds right",
  "correct number",
  "sounds good",
  "that works",
  "all good",
  "perfect",
};
const int affirmation_yes_count = sizeof(affirmation_yes_phrases) / sizeof(affirmation_yes_phrases[0]);

const char* affirmation_no_phrases[] =
{
  "no",
  "nope",
  "not right",
  "not correct",
  "incorrect",
  "wrong",
  "that's wrong",
  "that is wrong",
  "wrong number",
  "not mine",
};
const int affirmation_no_count = sizeof(affirmation_no_phrases) / sizeof(affirmation_no_phrases[0]);

const char* affirmation_unclear_phrases[] =
{
  "maybe",
  "not sure",
  "can you repeat that",
  "say that again",
};
const int affirmation_unclear_count = sizeof(affirmation_unclear_phrases) / sizeof(affirmation_unclear_phrases[0]);

const char* vehicle_models[] =
{
  "Ioniq 5",
  "Tucson",
  "Santa Fe",
  "Elantra",
  "Palisade",
  "Kona",
  "Sonata",
  "Venue",
  "Accent",
  "Veloster",
  "Genesis",
};
const int vehicle_model_count = sizeof(vehicle_models) / sizeof(vehicle_models[0]);

const char* interpreter_faq_rules =
  "f must be exactly one of: none, recall_service, warranty_service, ev_battery_service, service_hours, parts_availability, parts_order_status, sales_inventory, financing, trade_in. FAQ meanings: recall_service=recall, manufacturer recall, open recall, recall notice; warranty_service=warranty, manufacturer warranty, warranty coverage; ev_battery_service=EV battery, high-voltage battery, electric car battery warning; service_hours=service hours, when service opens or closes, service department schedule; parts_availability=whether a part can be checked or is available; parts_order_status=already ordered part status, ordered part arrival, parts order update; sales_inventory=in stock, on the lot, available vehicles, current inventory; financing=financing, lease rates, monthly payments, credit, approval; trade_in=trade-in, appraisal, value of current vehicle. Priority: trade_in beats sales_inventory; financing beats sales_inventory; ev_battery_service beats warranty_service; parts_order_status beats parts_availability.";

const char* interpreter_affirmation_rules =
  "a must be exactly one of: none, yes, no, unclear. Use a=yes only when the caller affirms or confirms the assistant's previous yes/no confirmation, like correct, that's right, yep, sounds good, that is mine. Use a=no for wrong, not correct, nope, wrong number, or not mine.";

}
}
