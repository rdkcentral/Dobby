#!/usr/bin/env bash
set -euo pipefail

# Move floating major tag to point to the new release
# Usage: move-major-tag.sh --version <semver>
# Example: move-major-tag.sh --version 1.2.3
#   → creates/updates actions-v1 to point to actions-v1.2.3

VERSION=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --version)
      VERSION="$2"
      shift 2
      ;;
    *)
      shift
      ;;
  esac
done

if [ -z "$VERSION" ]; then
  echo "ERROR: Usage: move-major-tag.sh --version <semver>"
  exit 1
fi

# Skip prerelease versions (e.g. 1.2.3-rc.1) — don't move the stable major tag
if [[ "$VERSION" == *-* ]]; then
  echo "Skipping major tag update for prerelease version: $VERSION"
  exit 0
fi

# Extract major version from semver
MAJOR=$(echo "$VERSION" | cut -d. -f1)

# Configure git if needed
if ! git config user.name 2>/dev/null; then
  git config user.name "github-actions[bot]"
fi
if ! git config user.email 2>/dev/null; then
  git config user.email "github-actions[bot]@users.noreply.github.com"
fi

# Move floating major tag
MAJOR_TAG="actions-v${MAJOR}"
VERSION_TAG="actions-v${VERSION}"
TARGET_COMMIT=$(git rev-parse "${VERSION_TAG}^{commit}" 2>/dev/null || true)

if [ -z "$TARGET_COMMIT" ]; then
  echo "ERROR: Failed to resolve commit for $VERSION_TAG"
  exit 1
fi

git tag -f "$MAJOR_TAG" "$TARGET_COMMIT" || {
  echo "ERROR: Failed to tag $MAJOR_TAG -> $TARGET_COMMIT"
  exit 1
}

git push origin "refs/tags/$MAJOR_TAG" --force || {
  echo "ERROR: Failed to push $MAJOR_TAG"
  exit 1
}

echo "Moved $MAJOR_TAG → $VERSION_TAG ($TARGET_COMMIT)"
