#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

echo "Project root: $PROJECT_ROOT"

# --- Argument Parsing ---
FLAVOR="debian" # Default

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --debian) FLAVOR="debian" ;;
        --alpine)  FLAVOR="alpine" ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
    shift
done

# Ensure target dir exists
mkdir -p target

# 1. Resolve MINIFI_SDK_PATH (from env, or fallback to cargo config)
RESOLVED_SDK_PATH="${MINIFI_SDK_PATH}"
if [ -z "$RESOLVED_SDK_PATH" ] && [ -f .cargo/config.toml ]; then
    RESOLVED_SDK_PATH=$(grep -m 1 'MINIFI_SDK_PATH' .cargo/config.toml | cut -d '=' -f2 | tr -d ' "')
fi

# 2. Define target locations
DOCKER_SDK_DIR="target/.docker_sdk"
DOCKER_SDK_ZIP="target/.docker_sdk.zip"
DOCKER_SDK_ARG="$RESOLVED_SDK_PATH"

# Clean up any leftover SDK injections
rm -rf "$DOCKER_SDK_DIR" "$DOCKER_SDK_ZIP"

if [[ "$RESOLVED_SDK_PATH" == http* ]]; then
    echo "Detected URL SDK: $RESOLVED_SDK_PATH"
elif [ -f "$RESOLVED_SDK_PATH" ] && [[ "$RESOLVED_SDK_PATH" == *.zip ]]; then
    echo "Detected local ZIP SDK: $RESOLVED_SDK_PATH"
    cp "$RESOLVED_SDK_PATH" "$DOCKER_SDK_ZIP"
    DOCKER_SDK_ARG="/app/$DOCKER_SDK_ZIP"
elif [ -d "$RESOLVED_SDK_PATH" ]; then
    echo "Detected local directory SDK: $RESOLVED_SDK_PATH"
    echo "Extracting only required files to minimize Docker context..."

    mkdir -p "$DOCKER_SDK_DIR"

    # Case A: It's a flat SDK layout
    if [ -f "$RESOLVED_SDK_PATH/minifi-api.h" ]; then
        cp "$RESOLVED_SDK_PATH/minifi-api.h" "$DOCKER_SDK_DIR/"
        cp "$RESOLVED_SDK_PATH/minifi-api.def" "$DOCKER_SDK_DIR/" 2>/dev/null || true
        cp "$RESOLVED_SDK_PATH"/*.whl "$DOCKER_SDK_DIR/" 2>/dev/null || true

    # Case B: It's a raw repository layout
    elif [ -f "$RESOLVED_SDK_PATH/minifi-api/include/minifi-api.h" ]; then
        mkdir -p "$DOCKER_SDK_DIR/minifi-api/include"

        cp "$RESOLVED_SDK_PATH/minifi-api/include/minifi-api.h" "$DOCKER_SDK_DIR/minifi-api/include/"
        cp "$RESOLVED_SDK_PATH/minifi-api/minifi-api.def" "$DOCKER_SDK_DIR/minifi-api/" 2>/dev/null || true

        if [ -d "$RESOLVED_SDK_PATH/behave_framework" ]; then
            cp -R "$RESOLVED_SDK_PATH/behave_framework" "$DOCKER_SDK_DIR/"
        fi
    else
        echo "ERROR: Could not find minifi-api.h in $RESOLVED_SDK_PATH"
        exit 1
    fi

    DOCKER_SDK_ARG="/app/$DOCKER_SDK_DIR"
else
    echo "WARNING: MINIFI_SDK_PATH is neither a valid URL nor a local path: $RESOLVED_SDK_PATH"
fi

# --- Set specific build configurations based on flavor ---
echo "Building for $FLAVOR"
DOCKERFILE="minifi_rs_behave/$FLAVOR.dockerfile"
TARGET_DIR="target/release"

mkdir -p "$TARGET_DIR"

# 3. Build using Docker
docker buildx build \
  -f "$DOCKERFILE" \
  --target bin-export \
  --build-arg MINIFI_SDK_PATH="$DOCKER_SDK_ARG" \
  --output type=local,dest="$TARGET_DIR" \
  .

# 4. Cleanup
rm -rf "$DOCKER_SDK_DIR" "$DOCKER_SDK_ZIP"

echo "Build complete. Artifacts updated in $TARGET_DIR"
