"""MkDocs build hook: docs/ の外（ソースツリー）を指す相対リンクを GitHub URL へ書き換える。

このリポジトリの一部の Markdown（特に学習ガイド）は、解説対象のソースコードへ
``../../spi-hal/include/ispi_driver.hpp`` のような相対リンクを張っている。
GitHub のファイルビューではこれで解決できるが、MkDocs が生成する静的サイトには
``docs/`` 配下しか含まれないためリンク切れになる。

そこで build 時に「docs/ の外へ出る相対リンク」だけを検出し、GitHub の
blob/tree URL（main ブランチ）へ自動で書き換える。Markdown 本体は手で直さない
（= リンクは GitHub 上では相対のまま、サイト上では絶対 URL に解決される）。

CLAUDE.md の方針（「手動メンテは罠 → 機械で弾く」）に倣い、ソースへの導線は
ファイル追加時に各 Markdown を書き換えずに済むよう、ここで一括変換する。
"""
from __future__ import annotations

import os
import posixpath
import re

# このリポジトリの GitHub URL（公開ブランチは main 固定）
_REPO = "https://github.com/asiball/private-test-cpp-project"
_BLOB = f"{_REPO}/blob/main/"
_TREE = f"{_REPO}/tree/main/"

# インライン Markdown リンク: ](target) / ](target "title") / ](<target>)
_LINK_RE = re.compile(r'\]\(\s*<?([^()<>\s"]+)>?(\s+"[^"]*")?\s*\)')

# フェンス開始/終了（``` または ~~~）。コードブロック内のリンクは書き換えない。
_FENCE_RE = re.compile(r"^\s*(`{3,}|~{3,})")


def _rewrite_target(target: str, page_dir: str) -> str | None:
    """docs/ の外を指す相対リンクなら GitHub URL を返す。対象外なら None。"""
    # 絶対 URL / アンカー / サイトルート絶対パス / mailto などは対象外
    if target.startswith(("#", "/")) or re.match(r"^[A-Za-z][A-Za-z0-9+.\-]*:", target):
        return None
    path_part, sep, anchor = target.partition("#")
    if not path_part:
        return None
    # ページ位置（docs/ 起点）からの相対を正規化
    resolved = posixpath.normpath(posixpath.join("docs", page_dir, path_part))
    # docs/ の内側に収まるならサイト内リンク → 触らない
    if resolved == "docs" or resolved.startswith("docs/"):
        return None
    # リポジトリ root より上に出る異常リンクは触らない（保険）
    if resolved.startswith(".."):
        return None
    base = _TREE if path_part.endswith("/") else _BLOB
    return base + resolved + (("#" + anchor) if sep else "")


def on_page_markdown(markdown: str, page=None, **kwargs) -> str:  # noqa: D401
    page_dir = posixpath.dirname(page.file.src_path.replace(os.sep, "/"))

    out_lines: list[str] = []
    in_fence = False
    for line in markdown.split("\n"):
        if _FENCE_RE.match(line):
            in_fence = not in_fence
            out_lines.append(line)
            continue
        if in_fence:
            out_lines.append(line)
            continue

        def _sub(m: "re.Match[str]") -> str:
            new_target = _rewrite_target(m.group(1), page_dir)
            if new_target is None:
                return m.group(0)
            return f"]({new_target}{m.group(2) or ''})"

        out_lines.append(_LINK_RE.sub(_sub, line))

    return "\n".join(out_lines)
