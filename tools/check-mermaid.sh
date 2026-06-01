#!/usr/bin/env bash
#
# check-mermaid.sh — Markdown 内の ```mermaid ブロックを構文検証する。
#
# GitHub の mermaid はクライアント側描画のため、構文エラーが push/PR 時に
# 検知されない。本スクリプトを CI とローカル pre-push フックの双方から呼び、
# 図のレンダリング失敗を機械的に弾く（仕組みの単一情報源）。
#
# 使い方:
#   tools/check-mermaid.sh [file.md ...]
#     引数なし: 追跡対象の全 *.md を対象
#     引数あり: 指定ファイルのうち *.md だけを対象（pre-push の差分検証用）
#
# 検証は Docker イメージ minlag/mermaid-cli（mmdc）で行う。
# Docker が無い/動いていない場合は、導入方法を表示して警告スキップする
# （ローカル作業を止めない。強制は CI 側で担保）。
#
set -euo pipefail

readonly IMAGE="minlag/mermaid-cli:11.4.2"
readonly REPO="$(git rev-parse --show-toplevel)"
readonly PUPPETEER_CFG="tools/mermaid-puppeteer.json"

# --- 対象ファイルの決定 --------------------------------------------------
candidates=()
if [[ "$#" -gt 0 ]]; then
  for f in "$@"; do
    [[ "$f" == *.md ]] || continue
    [[ -f "$REPO/$f" ]] || continue
    candidates+=("$f")
  done
else
  while IFS= read -r f; do
    candidates+=("$f")
  done < <(cd "$REPO" && git ls-files '*.md')
fi

# mermaid ブロックを含むものだけに絞る
targets=()
for f in "${candidates[@]:-}"; do
  [[ -n "$f" ]] || continue
  if grep -q '```mermaid' "$REPO/$f"; then
    targets+=("$f")
  fi
done

if [[ "${#targets[@]}" -eq 0 ]]; then
  echo "check-mermaid: mermaid を含む対象ファイルはありません。スキップします。"
  exit 0
fi

# --- Docker の存在確認（無ければ警告スキップ） ---------------------------
if ! command -v docker >/dev/null 2>&1 || ! docker info >/dev/null 2>&1; then
  cat >&2 <<'EOF'
check-mermaid: Docker が見つからない/動作していないため検証をスキップしました（exit 0）。
  mermaid 図の検証には Docker が必要です。導入後に再実行してください:
    docker pull minlag/mermaid-cli:11.4.2
  ※ この検証は CI（GitHub Actions）でも実行され、そこで強制されます。
EOF
  exit 0
fi

# --- 検証 ----------------------------------------------------------------
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

failed=()
for f in "${targets[@]}"; do
  echo "check-mermaid: 検証中 -> $f"
  # markdown モード: ファイル内の全 mermaid ブロックを解析。1つでも構文NGなら mmdc が非0で終了。
  # 出力(-o)はコンテナ内 /tmp に書いて捨てる（--rm で消える）。リポは ro マウントのみ。
  # ※ -u でユーザーを差し替えるとイメージ内 Chromium のキャッシュを見失うため指定しない。
  if ! docker run --rm \
        -v "$REPO:/data:ro" \
        "$IMAGE" \
        -p "/data/$PUPPETEER_CFG" \
        -i "/data/$f" \
        -o "/tmp/out.md" >/dev/null 2>"$tmp/err.log"; then
    echo "  NG: $f" >&2
    sed 's/^/    | /' "$tmp/err.log" >&2 || true
    failed+=("$f")
  fi
done

if [[ "${#failed[@]}" -gt 0 ]]; then
  echo "" >&2
  echo "check-mermaid: ${#failed[@]} 件の Markdown で mermaid 構文エラーがあります:" >&2
  for f in "${failed[@]}"; do echo "  - $f" >&2; done
  exit 1
fi

echo "check-mermaid: OK（${#targets[@]} ファイル）"
exit 0
