#!/usr/bin/env bash
# =============================================================================
# build-docs.sh — 納品ドキュメント一式を生成する
#
#   PDF  : docs/deliverables/**/*.md  → output/pdf/<name>.pdf    (pandoc + xelatex)
#   Word : docs/deliverables/**/*.md  → output/docx/<name>.docx  (pandoc + reference.docx)
#   HTML : ソースコード               → docs/doxygen/html/       (Doxygen = APIリファレンス / C層)
#
# 方針:
#   - Markdown を唯一のソースとし、配布物(PDF/Word/HTML)は本スクリプトで機械生成する。
#   - テンプレート(_templates/) と 索引(README.md) は変換対象から除外する。
#   - pandoc / doxygen / xelatex が無い環境では該当ステップを警告してスキップする
#     （CI では Dockerfile.build のイメージ内で全ツールが揃った状態で実行される）。
#   - 前提ツール・Word の体裁(reference.docx)の調整方法は
#     docs/deliverables/README.md の §5 を参照。
#
# 使い方:
#   bash tools/build-docs.sh            # PDF + Word + HTML を生成
#   bash tools/build-docs.sh pdf        # PDF のみ
#   bash tools/build-docs.sh docx html  # Word と HTML のみ
# =============================================================================
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

DELIV_DIR="docs/deliverables"
META_FILE="tools/docs-metadata.yaml"
REF_DOCX="tools/docs-reference.docx"
OUT_PDF="output/pdf"
OUT_DOCX="output/docx"

have() { command -v "$1" >/dev/null 2>&1; }

# 変換対象の Markdown を列挙（テンプレート・各 README は除外）
collect_md() {
    find "$DELIV_DIR" -type f -name '*.md' \
        -not -path '*/_templates/*' \
        -not -name 'README.md' | sort
}

# pandoc 共通オプション。mermaid-filter があれば mermaid 図も画像化する。
pandoc_common_opts() {
    local opts=( --from=gfm --toc --toc-depth=3 --number-sections
                 --metadata-file="$META_FILE" --resource-path="$DELIV_DIR" )
    have mermaid-filter && opts+=( --filter=mermaid-filter )
    printf '%s\n' "${opts[@]}"
}

build_pdf() {
    have pandoc  || { echo "  [スキップ] pandoc が無いため PDF を生成しません";  return; }
    have xelatex || { echo "  [スキップ] xelatex が無いため PDF を生成しません"; return; }
    mkdir -p "$OUT_PDF"
    mapfile -t common < <(pandoc_common_opts)
    local f name
    while IFS= read -r f; do
        name="$(basename "$f" .md)"
        if pandoc "$f" "${common[@]}" --pdf-engine=xelatex -o "$OUT_PDF/$name.pdf"; then
            echo "  PDF : $name.pdf"
        else
            echo "  [警告] $name.pdf 変換失敗（スキップ）"
        fi
    done < <(collect_md)
}

build_docx() {
    have pandoc || { echo "  [スキップ] pandoc が無いため Word を生成しません"; return; }
    mkdir -p "$OUT_DOCX"
    # 体裁テンプレート(reference.docx)が無ければ pandoc 既定から雛形を生成する。
    # 自社体裁に合わせる場合は docs/deliverables/README.md §5 を参照して編集・コミットする。
    if [ ! -s "$REF_DOCX" ]; then
        echo "  [情報] $REF_DOCX が無いため既定テンプレートを生成します"
        pandoc --print-default-data-file reference.docx > "$REF_DOCX" 2>/dev/null || true
    fi
    mapfile -t common < <(pandoc_common_opts)
    local refopt=()
    [ -s "$REF_DOCX" ] && refopt=( --reference-doc="$REF_DOCX" )
    local f name
    while IFS= read -r f; do
        name="$(basename "$f" .md)"
        if pandoc "$f" "${common[@]}" "${refopt[@]}" -o "$OUT_DOCX/$name.docx"; then
            echo "  DOCX: $name.docx"
        else
            echo "  [警告] $name.docx 変換失敗（スキップ）"
        fi
    done < <(collect_md)
}

build_html() {
    have doxygen || { echo "  [スキップ] doxygen が無いため HTML を生成しません"; return; }
    have dot || echo "  [情報] graphviz(dot) が無いため図(UML/呼び出しグラフ)は省略されます"
    mkdir -p docs/doxygen
    if doxygen Doxyfile >/dev/null; then
        echo "  HTML: docs/doxygen/html/index.html"
    else
        echo "  [警告] Doxygen 生成に失敗しました"
    fi
}

targets=( "$@" )
[ ${#targets[@]} -eq 0 ] && targets=( pdf docx html )
for t in "${targets[@]}"; do
    case "$t" in
        pdf)  echo "=== PDF 生成 ===";          build_pdf  ;;
        docx) echo "=== Word(.docx) 生成 ===";   build_docx ;;
        html) echo "=== HTML(Doxygen) 生成 ==="; build_html ;;
        *)    echo "不明なターゲット: $t （pdf|docx|html）"; exit 2 ;;
    esac
done
echo "=== 完了 ==="
