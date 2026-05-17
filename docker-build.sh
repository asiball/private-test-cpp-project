#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMAGE="embedded-device-suite-builder"

echo "=== イメージをビルド ==="
docker build -f Dockerfile.build -t "$IMAGE" "$PROJECT_DIR"

echo "=== コンテナ内でビルド & Doxygen ==="
docker run --rm -v "$PROJECT_DIR":/workspace "$IMAGE"

echo ""
echo "=== 完了 ==="
echo "  バイナリ : build/"
echo "  ドキュメント : docs/doxygen/html/index.html"
