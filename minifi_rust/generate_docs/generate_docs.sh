#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

echo "$PROJECT_ROOT"

DOCKERFILE="generate_docs/generate_docs.dockerfile"
TARGET_DIR="target/docs"

mkdir -p "$TARGET_DIR"

docker buildx build \
  -f "$DOCKERFILE" \
  --target docs-export \
  --output type=local,dest="$TARGET_DIR" \
  .

echo "Build complete. Artifacts updated in $TARGET_DIR"

for filepath in target/docs/modules/*.md; do
    filename=$(basename "$filepath")
    dirname="${filename%.md}"
    if [ -d "extensions/$dirname" ]; then
        echo "Copying $filename to extensions/$dirname/"
        cp "$filepath" "extensions/$dirname/"
    fi
done
