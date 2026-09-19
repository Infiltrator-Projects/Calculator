# Validation

## Evidence model

Compilation, deterministic regression tests, platform integration and human visual/interaction testing prove different things.

## Automated gates

- .github/workflows/release.yml

tests/ covers the calculator engine, session model, programmer behaviour, desktop layout, UI contract and shared controller. Release automation builds the supported platform outputs from the same source.

## Manual/environment evidence

Signed iPhone installation requires external Apple signing credentials. Visual/platform integration on physical devices and native Windows/Linux desktops remains a real-environment check beyond core unit tests.

Manual evidence supplements automation and should record the platform/environment actually observed.

## Release criterion

The exact source intended for release/publication must pass required automated checks, and generated/package artifacts must correspond to that source identity.

## Regression rule

Reproducible defects should gain permanent automated coverage at the narrowest layer that captures the original failure.
