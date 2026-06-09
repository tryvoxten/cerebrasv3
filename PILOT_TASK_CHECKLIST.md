# Pilot Task Checklist

Use this as the side checklist for hardening the pilot one issue at a time.

## Vehicle Capture

- [ ] Trim-to-model-family mapping
  - Examples: BMW M340i -> BMW 3 Series, Mercedes C300 -> Mercedes-Benz C-Class, Honda Civic Si -> Honda Civic.
- [ ] Make nicknames and spoken brand names
  - Examples: Chevy -> Chevrolet, Benz -> Mercedes-Benz, VW -> Volkswagen, Bimmer -> BMW.
- [ ] Spoken alphanumeric model codes
  - Examples: CX five -> CX-5, Q five -> Q5, X five -> X5, four runner -> 4Runner, Silverado fifteen hundred -> Silverado 1500.
- [ ] Wrong-but-close model names
  - Examples: Honda CRV -> Honda CR-V, Hyundai Ionic -> Hyundai IONIQ, Toyota Highlander hybrid -> Toyota Highlander.
- [ ] Vehicle year interpretation
  - Examples: twenty eighteen -> 2018, oh seven -> 2007, ninety nine -> 1999.
- [ ] Vehicle correction handling
  - Example: "It's a 2020 Camry. Actually, sorry, 2021." should store 2021 Toyota Camry.

## Conversation Handling

- [ ] Rejected or unclear input explanations
  - The agent should explain what was wrong and ask for the missing/valid value.
- [ ] Customer questions during capture
  - The agent should answer from the KB when possible, then continue the form flow.
- [ ] Non-answer detection
  - Examples: "Why do you need that?", "I do not know", "same as caller ID", "whenever you open".
- [ ] Friendly confirmation for interpreted values
  - Example: "I heard BMW M340i. I have that under BMW 3 Series in our system. Is that right?"

## Department Routing

- [ ] Natural service detection
  - Examples: funny noise, check engine light, oil change, appointment, repair.
- [ ] Parts-vs-service ambiguity
  - Example: brake pads could mean buying parts or booking service.
- [ ] Sales detection
  - Examples: trade value, test drive, pricing, availability, financing.

## Phone And Callback Capture

- [ ] Phone number correction handling
  - Example: "555-1234, no sorry, 555-1243."
- [ ] Caller ID shortcuts
  - Examples: "the number I am calling from", "same as caller ID", "my cell".
- [ ] Vague callback time interpretation
  - Examples: "two weeks from now at 2", "tomorrow morning", "when you open".

## Knowledge Base Answers

- [ ] Service hours
- [ ] Location and directions
- [ ] Towing policy
- [ ] Loaner vehicle policy
- [ ] Warranty questions
- [ ] Recall questions
- [ ] Appointment policy
- [ ] After-hours process

## Debugging And Reliability

- [ ] Call ID visible in every log path
- [ ] Interpreter request and response logging
- [ ] Validation failure reason logging
- [ ] Final captured summary logging
- [ ] Websocket timeout monitoring
- [ ] Retell transcript comparison against stored fields
