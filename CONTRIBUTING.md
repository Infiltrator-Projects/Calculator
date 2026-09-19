# Contributing

## Engineering standard

Start by identifying which layer owns the behaviour and how the change will be validated.

## Required practice

1. Read README.md plus docs/ARCHITECTURE.md and docs/DESIGN.md.
2. Search for existing project/Common capability before adding a parallel implementation.
3. Keep domain logic separate from platform/rendering/hosting details where practical.
4. Add regression coverage for changed rules and failure cases.
5. Update roadmap/validation/specialist documentation when support boundaries change.

## Verification

Run the normal build/test path and ensure relevant CI remains green. Visual, playability, browser or physical-device claims require corresponding manual evidence.

## Repository policy

main is the working branch. Published tags/releases are immutable source identities.
