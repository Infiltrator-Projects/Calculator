#!/usr/bin/env bash
set -euo pipefail

: "${APP_ID:?APP_ID is required}"
: "${VERSION:?VERSION is required}"
: "${RELEASE_SHA:?RELEASE_SHA is required}"
: "${GITHUB_REPOSITORY:?GITHUB_REPOSITORY is required}"

catalogue='https://infiltrator-projects.github.io/Infiltrator-Repository/catalogue/apps.json'
dispatched=false

if [[ -n "${DISPATCH_TOKEN:-}" ]]; then
  payload="$(jq -nc \
    --arg repository "$GITHUB_REPOSITORY" \
    --arg sha "$RELEASE_SHA" \
    '{event_type:"application-release",client_payload:{repository:$repository,sha:$sha}}')"
  if curl --fail-with-body --silent --show-error \
      --request POST \
      --header "Authorization: Bearer $DISPATCH_TOKEN" \
      --header 'Accept: application/vnd.github+json' \
      --header 'X-GitHub-Api-Version: 2022-11-28' \
      --data "$payload" \
      https://api.github.com/repos/Infiltrator-Projects/Infiltrator-Repository/dispatches; then
    dispatched=true
    echo 'Requested immediate central APT refresh.'
  else
    echo '::warning::Immediate central APT dispatch failed; falling back to tokenless discovery.'
  fi
else
  echo 'No cross-repository token configured. Package Repository owns tokenless scheduled discovery.'
fi

# The central repository is an independent publication authority.  Poll for
# the fast path without allowing its scheduled fallback latency to invalidate
# an otherwise complete Calculator release.
for attempt in $(seq 1 60); do
  if curl -fsS --retry 2 --retry-delay 1 \
      -H 'Cache-Control: no-cache' \
      "$catalogue?release=$RELEASE_SHA&attempt=$attempt" \
      | jq -e --arg id "$APP_ID" --arg version "$VERSION" \
          '.[] | select(.id == $id and .version == $version)' >/dev/null; then
    echo "APT repository advertises $APP_ID $VERSION."
    exit 0
  fi
  echo "Waiting for central APT discovery of $APP_ID $VERSION ($attempt/60)..."
  sleep 10
done

if [[ "$dispatched" == true ]]; then
  echo "::error::An immediate APT refresh was accepted, but $APP_ID $VERSION was not published within 10 minutes."
  exit 1
fi

echo "::warning::Calculator release is valid and published, but tokenless central APT discovery has not propagated yet."
