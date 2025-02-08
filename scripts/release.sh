#!/usr/bin/env bash

# Paths for Linux artifacts
CS2KZ_SO="/home/runner/artifacts/cs2kz-linux/addons/cs2kz/bin/linuxsteamrt64/cs2kz.so"
LINUX_MODES_DIR="/home/runner/artifacts/cs2kz-linux/addons/cs2kz/modes"
LINUX_STYLES_DIR="/home/runner/artifacts/cs2kz-linux/addons/cs2kz/styles"

# Paths for Windows artifacts
CS2KZ_DLL="/home/runner/artifacts/cs2kz-windows/addons/cs2kz/bin/win64/cs2kz.dll"
WINDOWS_MODES_DIR="/home/runner/artifacts/cs2kz-windows/addons/cs2kz/modes"
WINDOWS_STYLES_DIR="/home/runner/artifacts/cs2kz-windows/addons/cs2kz/styles"

# Placeholder values for version, git_revision, and is_cutoff
VERSION="$1"
GIT_REVISION=$(git rev-parse HEAD)
IS_CUTOFF="$2"

# Calculate checksums for cs2kz
cs2kz_sum_lin=$(md5sum "$CS2KZ_SO" | awk '{ print $1 }')
cs2kz_sum_win=$(md5sum "$CS2KZ_DLL" | awk '{ print $1 }')

# Initialize JSON strings for modes and styles
modes_json=$(jq -n \
    --arg linux_checksum "$cs2kz_sum_lin" \
    --arg windows_checksum "$cs2kz_sum_win" \
    '{
      mode: "vnl",
      linux_checksum: $linux_checksum,
      windows_checksum: $windows_checksum
    }')
styles_json=""

# Process modes
for mode_lin in "$LINUX_MODES_DIR"/cs2kz-mode-*.so; do
  mode_name=$(basename "$mode_lin" | sed -E 's/.*-(\w+)\.so/\1/')
  mode_sum_lin=$(md5sum "$mode_lin" | awk '{ print $1 }')

  # Find corresponding Windows mode file
  mode_win="$WINDOWS_MODES_DIR/cs2kz-mode-$mode_name.dll"
  if [[ -f "$mode_win" ]]; then
    mode_sum_win=$(md5sum "$mode_win" | awk '{ print $1 }')
  else
    mode_sum_win=""
  fi

  # Create mode JSON entry
  mode_entry=$(jq -n \
    --arg mode "$mode_name" \
    --arg linux_checksum "$mode_sum_lin" \
    --arg windows_checksum "$mode_sum_win" \
    '{
      mode: $mode,
      linux_checksum: $linux_checksum,
      windows_checksum: $windows_checksum
    }')

    modes_json="$modes_json,$mode_entry"
done

# Process styles
for style_lin in "$LINUX_STYLES_DIR"/cs2kz-style-*.so; do
  style_name=$(basename "$style_lin" | sed -E 's/.*-(\w+)\.so/\1/')
  style_sum_lin=$(md5sum "$style_lin" | awk '{ print $1 }')

  # Find corresponding Windows style file
  style_win="$WINDOWS_STYLES_DIR/cs2kz-style-$style_name.dll"
  if [[ -f "$style_win" ]]; then
    style_sum_win=$(md5sum "$style_win" | awk '{ print $1 }')
  else
    style_sum_win=""
  fi

  # Create style JSON entry
  style_entry=$(jq -n \
    --arg style "$style_name" \
    --arg linux_checksum "$style_sum_lin" \
    --arg windows_checksum "$style_sum_win" \
    '{
      style: $style,
      linux_checksum: $linux_checksum,
      windows_checksum: $windows_checksum
    }')

  # Append to styles JSON
  if [[ -z $styles_json ]]; then
    styles_json="$style_entry"
  else
    styles_json="$styles_json,$style_entry"
  fi
done

# Wrap modes and styles in JSON arrays
modes_json="[$modes_json]"
styles_json="[$styles_json]"

# Construct the final JSON payload
json_payload=$(jq -n \
  --arg version "$VERSION" \
  --arg git_revision "$GIT_REVISION" \
  --arg linux_checksum "$cs2kz_sum_lin" \
  --arg windows_checksum "$cs2kz_sum_win" \
  --argjson is_cutoff "$IS_CUTOFF" \
  --argjson modes "$modes_json" \
  --argjson styles "$styles_json" \
  '{
    version: $version,
    git_revision: $git_revision,
    linux_checksum: $linux_checksum,
    windows_checksum: $windows_checksum,
    is_cutoff: $is_cutoff,
    modes: $modes,
    styles: $styles
  }')

AUTH_TOKEN="$3"

response=$(curl -s -o /dev/null -w "%{http} %{http_code}" \
  -X POST https://api.cs2kz.org/plugin/versions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $AUTH_TOKEN" \
  -d "$json_payload")

# Check the response status code
if [[ $response -eq 201 ]]; then
  echo "POST request successful!"
else
  echo "POST request failed with status code: $response"
  echo "Payload:"
  echo "$json_payload"
  exit 1
fi