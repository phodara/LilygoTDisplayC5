#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <version> <commit message>"
  echo "Example: $0 v2.2.0 \"Release v2.2.0 display and BLE data refinements\""
  exit 1
fi

version="$1"
shift
message="$*"

if [[ ! "$version" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Version must look like v2.0.1"
  exit 1
fi

if git rev-parse "$version" >/dev/null 2>&1; then
  echo "Tag $version already exists."
  exit 1
fi

git add \
  src/main.cpp \
  .vscode/extensions.json \
  .vscode/settings.json \
  README.md \
  docs/battery-recommendations.md \
  docs/bluetooth-scanner.md \
  docs/button-commands.md \
  docs/release-assets \
  docs/wifi-security-labels.md \
  docs/wifi-vendor-lookup.md \
  CHANGELOG.md

if git diff --cached --quiet; then
  echo "No staged changes to commit."
  exit 1
fi

git commit -m "$message"
git tag -a "$version" -m "Version ${version#v}: $message"
git push origin main
git push origin "$version"
