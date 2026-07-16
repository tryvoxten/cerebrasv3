# Live Call Failure Tracker

This file maps real call failures to fixes, tests, and deploy evidence. Keep it
updated when a transcript exposes a new failure class or a regression.

## How to use this tracker

For every issue, capture:

- call evidence: Retell call ID or transcript URL
- failure class: reusable category, not just one exact phrase
- symptom: what the caller experienced
- expected behavior: what Voxten should have done
- code/test owner: file or test that should prevent recurrence
- fix status: `open`, `fixed-local`, `deployed`, or `watching`
- deployment evidence: commit, Fly version, image, and health result when known

## Failure classes

| ID | Status | Evidence | Symptom | Expected behavior | Guardrail |
| --- | --- | --- | --- | --- | --- |
| LCF-001 | deployed | `call_3cb795e524c6d94540d7660fa84` | Relative date phrases were not consistently converted into concrete callback slots. | Phrases like `tomorrow at 3`, `after tomorrow at 3`, `Monday at 3`, and `two weeks from now at 4` resolve to a concrete day, date, year, and time before readback. | `relative_callback_time_resolves_to_concrete_date` in `tests/main_tests.cpp`; parser isolated in `src/relative_callback_time.cpp`. |
| LCF-002 | deployed | `call_d14b27acbd755a7ef3edcff30f0` | Callback readback could confirm the date twice after a relative-date capture. | Final confirmation should paste the captured callback slot once and stay under the spoken-word cap. | `relative_callback_readback_does_not_duplicate_dates` in `tests/main_tests.cpp`. |
| LCF-003 | deployed | `call_7011a7766b54e1522dac1fd615e` | Knowledge-base answers could sound like copied sections or graph/list output. | KB answers should be summarized as spoken language, answer the caller first, and continue intake with one question if needed. | Conversation quality scenarios in `CONVERSATION_QUALITY_STANDARD.md`; FAQ priority checks in `tests/main_tests.cpp`. |
| LCF-004 | deployed | `call_22ee89f4f6d258b103a3df580aa` | Caller asked whether the agent could check the number they were calling from. | Use Retell phone metadata when available; otherwise say the number is not visible and ask the caller to dictate the full ten-digit number. | `calling_number_question_uses_metadata_or_requests_dictation` in `tests/main_tests.cpp`. |
| LCF-005 | deployed | `call_22ee89f4f6d258b103a3df580aa` | Vague sales or parts requests could move on without enough detail. | Keep the first question broad, then ask one specific follow-up for model/type of car or specific part/make/model. | `vague_sales_request_keeps_model_followup` and `vague_parts_request_keeps_part_followup` in `tests/main_tests.cpp`. |
| LCF-006 | deployed | `call_22ee89f4f6d258b103a3df580aa` | Completed intake did not reliably end the Retell call. | Once required details are confirmed, send `end_call: true` and avoid restarting questioning. | `completed_intake_ends_retell_call` and `final_close_check_yes_ends_call` in `tests/main_tests.cpp`. |
| LCF-007 | ready for deploy | Conversation realism review | Phone confirmation could close too abruptly, missed answers were not explicitly repaired, multi-question caller turns had weak recovery, and weird flow had no structured inspection event. | Phone confirmation now moves to a final `Will that be all?` close check; probable missed answers get `Sorry, I did not get X`; multi-question FAQ turns ask for the other question; integrity issues are emitted as `conversation_integrity` log events. | `phone_confirmation_moves_to_final_close_check`, `final_close_check_yes_ends_call`, `final_confirmation_asks_will_that_be_all`, and `conversation_integrity_tests`. |

## Open watchlist

| ID | Status | Evidence needed | Risk | Next guardrail |
| --- | --- | --- | --- | --- |
| LCF-W001 | watching | More real phone calls, not web calls | ASR can still distort names, phone digits, relative dates, and multi-question turns. | Add replayable transcript fixtures for realistic multi-turn calls before larger behavior refactors. |
| LCF-W002 | watching | Calls with corrections at different points | Corrections may update one field while leaving stale confirmation language elsewhere. | Add transcript replay cases for corrected date, phone, vehicle, and request. |
| LCF-W003 | watching | Calls with multiple KB questions in one turn | FAQ routing may answer the lower-priority question or sound too enumerated. | Add transcript replay cases for multi-question KB turns. |
