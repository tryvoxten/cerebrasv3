# Pilot Test Standard

This is the test standard for the Cerebras v3 dealership receptionist pilot.
Use it before changing prompts, flow logic, or field extraction.

The goal is not to prove the model can handle every possible caller. The goal is
to prove the pilot flow is stable, fast, and polite on the calls the dealership
actually needs handled after hours.

## Architecture Under Test

Each caller turn follows this path:

```text
caller message
-> AI interprets compact JSON
-> code merges fields into call state
-> code chooses exactly one next missing field
-> AI or code writes one receptionist sentence
-> Retell speaks that sentence
```

The AI may interpret and word the sentence. The code owns state, routing, and the
next field.

## Pass Targets

The pilot is ready for live dialing only when a fixed test batch passes these
targets:

| Area | Target |
| --- | --- |
| Completion | 11 of 12 calls reach `next_field=none` |
| Department routing | 12 of 12 correct |
| Required field extraction | 90% of required fields captured correctly |
| One-question rule | 12 of 12 assistant turns ask at most one question |
| Banned wording | 0 banned wording failures |
| Generic vehicle handling | 0 generic `car` / `vehicle` captures accepted as specific vehicles |
| Median latency | under 800 ms from app request to app response |
| P90 latency | under 1200 ms from app request to app response |

One failed call is acceptable only if the reason is written down and not a
repeat of a known issue.

## Hard Fail Wording

Any assistant sentence fails if it contains:

```text
for our records
assist you better
assist you further
service appointment
schedule an appointment
Mr.
Mr 
Ms.
Ms 
Mrs.
Mrs 
```

Any assistant sentence also fails if:

- it asks for two pieces of information at once
- it asks for spelling before the caller has given a name
- it confirms pricing, availability, diagnosis, financing, or exact timelines
- it says the dealership will definitely perform a service, sell a part, or hold inventory

## Required Fields By Department

### Service

Required:

- department
- intent
- caller first and last name
- last name spelling
- vehicle, unless the caller only gives a generic vehicle reference
- request or problem
- preferred callback time
- callback phone
- callback phone confirmation
- final confirmation

Service examples:

- check engine light
- hot smell after charging
- recall letter
- battery warning
- maintenance light
- warranty question

### Parts

Required:

- department
- intent
- caller first and last name
- last name spelling
- vehicle if mentioned or needed for the part
- requested part or accessory
- preferred callback time
- callback phone
- callback phone confirmation
- final confirmation

Parts examples:

- charge port door
- cargo mat
- wiper blades
- key fob battery
- mirror cap

### Sales

Required:

- department
- intent
- caller first and last name
- last name spelling
- vehicle of interest or broad interest
- request
- preferred callback time
- callback phone
- callback phone confirmation
- final confirmation

Sales examples:

- test drive
- lease numbers
- price shopping
- trade-in
- inventory question

## Vehicle Rules

Do not accept generic vehicle values as captured vehicles.

Future version: replace the hardcoded vehicle whitelist with a dealership KB of
known years, makes, models, trims, and aliases. The pilot uses a small hardcoded
whitelist only to keep the first system deterministic.

Reject:

```text
car
my car
the car
vehicle
my vehicle
old car
new car
truck
SUV
EV
Hyundai
```

Accept:

```text
2022 Hyundai Ioniq 5
Ioniq 5
2021 Hyundai Tucson
Tucson
Santa Fe hybrid
Palisade
2020 Elantra
```

If the caller says only `car`, the next field should remain `vehicle` for service
and parts when vehicle is required.

## Fixed 12-Call Batch

Each batch must run the same 12 calls. The test caller should answer exactly the
field requested by `next_field`.

### Service 1: Check Engine And Hot Smell

Opening:

```text
my check engine light is on and the car smells hot after charging
```

Expected:

- department: service
- intent: diagnostic or vehicle problem
- request includes check engine light and hot smell
- vehicle is not captured as `car`
- system asks for vehicle before callback time

Answers:

```text
name: Jordan Smith
last_name_spelling: S M I T H
vehicle: 2022 Hyundai Ioniq 5
callback_time: next Tuesday morning
phone: 416 555 0199
phone_confirmed: yes that is correct
final_confirmed: yes everything is correct
```

### Service 2: Recall Letter

Opening:

```text
I got a recall letter for my 2021 Hyundai Tucson
```

Expected:

- department: service
- intent: recall
- vehicle: 2021 Hyundai Tucson
- request includes recall letter

Answers:

```text
department: service
name: Morgan Lee
last_name_spelling: L E E
callback_time: Monday around 10
phone: 289 555 0144
phone_confirmed: yes
final_confirmed: yes that is right
```

### Service 3: Maintenance Light

Opening:

```text
my maintenance light came on in my Elantra
```

Expected:

- department: service
- intent: maintenance
- vehicle: Elantra
- request includes maintenance light

Answers:

```text
name: Casey Williams
last_name_spelling: W I L L I A M S
callback_time: Wednesday morning
phone: 705 555 3030
phone_confirmed: correct
final_confirmed: yes correct
```

### Service 4: Warranty Question

Opening:

```text
I have a warranty question about a noise in my 2020 Santa Fe
```

Expected:

- department: service
- intent: warranty or diagnostic
- vehicle: 2020 Santa Fe
- request includes warranty question and noise

Answers:

```text
name: Riley Brown
last_name_spelling: B R O W N
callback_time: Thursday afternoon
phone: 613 555 2222
phone_confirmed: yep
final_confirmed: all correct
```

### Parts 1: Charge Port Door

Opening:

```text
I need the plastic charge port door for an Ioniq 5
```

Expected:

- department: parts
- vehicle: Ioniq 5
- request includes charge port door

Answers:

```text
name: Alex Rivera
last_name_spelling: R I V E R A
callback_time: tomorrow afternoon
phone: 647 555 0123
phone_confirmed: correct
final_confirmed: yes that is correct
```

### Parts 2: Cargo Mat

Opening:

```text
do you have a rubber cargo mat for a Palisade
```

Expected:

- department: parts
- vehicle: Palisade
- request includes rubber cargo mat

Answers:

```text
name: Taylor Nguyen
last_name_spelling: N G U Y E N
callback_time: Wednesday around 10
phone: 519 555 9911
phone_confirmed: right
final_confirmed: yup
```

### Parts 3: Wiper Blades

Opening:

```text
need wiper blades for a 2020 Elantra
```

Expected:

- department: parts
- vehicle: 2020 Elantra
- request includes wiper blades

Answers:

```text
name: Chris OBrien
last_name_spelling: O B R I E N
callback_time: today later
phone: 226 555 4567
phone_confirmed: yes
final_confirmed: correct
```

### Parts 4: Key Fob Battery

Opening:

```text
I need a key fob battery for my Tucson
```

Expected:

- department: parts
- vehicle: Tucson
- request includes key fob battery

Answers:

```text
name: Avery Martin
last_name_spelling: M A R T I N
callback_time: Friday evening
phone: 365 555 9876
phone_confirmed: yes
final_confirmed: correct
```

### Sales 1: Test Drive And Trade

Opening:

```text
I want to test drive a Santa Fe hybrid and talk about trading in my old car
```

Expected:

- department: sales
- intent: test drive or trade
- vehicle of interest: Santa Fe hybrid
- caller name is not captured from the opening sentence

Answers:

```text
name: Sam Patel
last_name_spelling: P A T E L
callback_time: Friday after lunch
phone: 905 555 7788
phone_confirmed: yes correct
final_confirmed: yes all correct
```

### Sales 2: Lease Numbers

Opening:

```text
I am looking for lease numbers on a new Tucson
```

Expected:

- department: sales
- intent: lease
- vehicle of interest: Tucson

Answers:

```text
name: Jamie Thompson
last_name_spelling: T H O M P S O N
callback_time: Saturday morning
phone: 343 555 1010
phone_confirmed: yep that is mine
final_confirmed: yes
```

### Sales 3: EV Interest

Opening:

```text
I want to test drive whatever electric Hyundai is not tiny
```

Expected:

- department: sales
- intent: test drive
- vehicle interest can be broad electric Hyundai
- request includes not tiny or larger EV

Answers:

```text
name: Quinn Davis
last_name_spelling: D A V I S
callback_time: tomorrow morning
phone: 437 555 0155
phone_confirmed: correct
final_confirmed: yes correct
```

### Sales 4: Inventory Question

Opening:

```text
do you have any used Palisades available
```

Expected:

- department: sales
- intent: inventory
- vehicle of interest: used Palisade

Answers:

```text
name: Parker Wilson
last_name_spelling: W I L S O N
callback_time: next Monday afternoon
phone: 416 555 0188
phone_confirmed: yes
final_confirmed: yes that is right
```

## Scoring

Each call has 10 points:

| Category | Points |
| --- | --- |
| Correct department | 1 |
| Correct intent/request | 1 |
| Correct name capture | 1 |
| Correct last name spelling capture | 1 |
| Correct vehicle handling | 1 |
| Correct callback time capture | 1 |
| Correct phone capture and confirmation | 1 |
| Final confirmation reaches completion | 1 |
| No banned wording | 1 |
| No multi-question assistant turns | 1 |

Batch score:

```text
pass: 108/120 or higher, with zero banned wording failures
fail: below 108/120, or any banned wording failure
```

## Known Current Failures

These are the first fixes to make before more prompt tuning:

1. Generic `car` is accepted as a specific vehicle.
2. Sales opening can be captured as caller name.
3. Generated response can include `for our records`.
4. Generated response can include titles such as `Mr Smith`.
5. Generated response can say `service appointment`.
6. Final confirmation can be too long or too vague.

## Change Discipline

After every code or prompt change:

1. Run `make check`.
2. Run the fixed 12-call batch.
3. Record score, median latency, P90 latency, and failures.
4. Change only one category at a time:
   - extraction
   - planner flow
   - response cleanup
   - prompt wording
   - deployment/infrastructure

Do not compare random conversations across changes. Random tests are useful only
after the fixed batch passes.
