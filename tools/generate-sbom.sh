#!/usr/bin/env bash
# Generate SBOM files (sbom.spdx and sbom.cdx.json) from tools/sbom-metadata.json.
#
# Usage:
#   ./tools/generate-sbom.sh           # generate/update SBOM files
#   ./tools/generate-sbom.sh --verify  # verify existing SBOM matches metadata
#
# Requirements: Python 3.6+  (no external packages needed)
# Works locally, in Docker, and in CI without any additional tool installation.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$REPO_ROOT"

if ! command -v python3 &>/dev/null; then
    echo "ERROR: python3 is required but not found in PATH" >&2
    exit 1
fi

exec python3 tools/generate-sbom.py "$@"
