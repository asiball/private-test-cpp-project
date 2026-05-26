#!/usr/bin/env python3
"""Generate SPDX 2.3 tag-value and CycloneDX 1.6 JSON SBOMs from tools/sbom-metadata.json.

Usage:
  python3 tools/generate-sbom.py           # generate sbom.spdx and sbom.cdx.json
  python3 tools/generate-sbom.py --verify  # check existing files match metadata
"""

from __future__ import annotations

import json
import subprocess
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
METADATA = SCRIPT_DIR / "sbom-metadata.json"
SPDX_OUT = REPO_ROOT / "sbom.spdx"
CDX_OUT = REPO_ROOT / "sbom.cdx.json"

_SIMPLE_SPDX_IDS = {
    "MIT", "BSD-3-Clause", "Apache-2.0",
    "LGPL-2.1-or-later", "LGPL-2.0-or-later",
    "GPL-2.0-only", "GPL-3.0-only",
}


# ── helpers ───────────────────────────────────────────────────────────────────

def load_metadata() -> dict:
    with METADATA.open(encoding="utf-8") as f:
        return json.load(f)


def make_doc_uuid(project: dict) -> str:
    seed = f"{project['namespace_base']}/{project['version']}"
    return str(uuid.uuid5(uuid.NAMESPACE_URL, seed))


def get_metadata_timestamp() -> str:
    """Return the git commit timestamp of sbom-metadata.json, or now if unavailable."""
    try:
        result = subprocess.run(
            ["git", "log", "-1", "--format=%cI", "--", str(METADATA)],
            capture_output=True, text=True, cwd=REPO_ROOT, timeout=5,
        )
        raw = result.stdout.strip()
        if result.returncode == 0 and raw:
            dt = datetime.fromisoformat(raw.replace("Z", "+00:00"))
            return dt.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    except Exception:
        pass
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def cdx_license(expr: str) -> dict:
    if expr in _SIMPLE_SPDX_IDS:
        return {"license": {"id": expr}}
    return {"expression": expr}


def spdx_supplier(pkg: dict) -> str:
    sup = pkg.get("supplier", "")
    if not sup:
        return "NOASSERTION"
    if sup.startswith(("Organization:", "Person:")):
        return sup
    return f"Organization: {sup} ()"


# ── SPDX 2.3 tag-value generator ─────────────────────────────────────────────

def generate_spdx(meta: dict, timestamp: str, doc_uuid: str) -> str:
    project = meta["project"]
    lines: list[str] = [
        "SPDXVersion: SPDX-2.3",
        "DataLicense: CC0-1.0",
        "SPDXID: SPDXRef-DOCUMENT",
        f"DocumentName: {project['name']}-sbom",
        f"DocumentNamespace: {project['namespace_base']}/{project['version']}-{doc_uuid}",
        f"Creator: Tool: tools/generate-sbom.py",
        f"Creator: Organization: {project['organization']} ()",
        f"Created: {timestamp}",
    ]

    for pkg in meta["packages"]:
        lines += [
            "",
            f"PackageName: {pkg['name']}",
            f"SPDXID: {pkg['spdx_id']}",
            f"PackageVersion: {pkg['version']}",
            f"PackageSupplier: {spdx_supplier(pkg)}",
            f"PackageDownloadLocation: {pkg['download_location']}",
            "FilesAnalyzed: false",
            f"PackageLicenseConcluded: {pkg['license']}",
            f"PackageLicenseDeclared: {pkg['license']}",
            f"PackageCopyrightText: {pkg['copyright']}",
        ]
        if desc := pkg.get("description"):
            lines.append(f"PackageComment: {desc}")

    lines.append("")
    for rel in meta["relationships"]:
        lines.append(f"Relationship: {rel['element']} {rel['type']} {rel['related']}")

    lines.append("")
    return "\n".join(lines)


# ── CycloneDX 1.6 JSON generator ─────────────────────────────────────────────

def _cdx_component(pkg: dict) -> dict:
    comp: dict = {
        "type": pkg.get("cdx_type", "library"),
        "bom-ref": pkg["bom_ref"],
        "supplier": {"name": pkg.get("supplier", "")},
        "name": pkg["name"],
        "purl": pkg["bom_ref"],
        "licenses": [cdx_license(pkg["license"])],
    }
    # Version: omit for NOASSERTION packages
    ver = pkg["version"]
    if ver != "NOASSERTION":
        comp["version"] = ver
    # Copyright: omit for NOASSERTION
    copyright_text = pkg.get("copyright", "")
    if copyright_text and copyright_text != "NOASSERTION":
        comp["copyright"] = copyright_text
    if desc := pkg.get("description"):
        comp["description"] = desc
    if scope := pkg.get("cdx_scope"):
        comp["scope"] = scope
    return comp


def _pkg_by_spdx_id(packages: list[dict], spdx_id: str) -> dict | None:
    return next((p for p in packages if p["spdx_id"] == spdx_id), None)


def generate_cdx(meta: dict, timestamp: str, doc_uuid: str) -> dict:
    packages = meta["packages"]
    root_pkg = next(p for p in packages if p.get("is_root"))
    other_pkgs = [p for p in packages if not p.get("is_root")]

    id_to_ref = {p["spdx_id"]: p["bom_ref"] for p in packages}
    dep_map: dict[str, set[str]] = {p["bom_ref"]: set() for p in packages}

    for rel in meta["relationships"]:
        rel_type = rel["type"]
        if rel_type in ("DESCRIBES", "TEST_DEPENDENCY_OF"):
            continue
        if rel_type == "BUILD_DEPENDENCY_OF":
            # "A BUILD_DEPENDENCY_OF B" → B depends on A
            src_ref = id_to_ref.get(rel["related"])
            dst_ref = id_to_ref.get(rel["element"])
        else:
            src_ref = id_to_ref.get(rel["element"])
            dst_ref = id_to_ref.get(rel["related"])
        if src_ref and dst_ref:
            dep_map[src_ref].add(dst_ref)

    dependencies = [
        {"ref": ref, "dependsOn": sorted(deps)}
        for ref, deps in dep_map.items()
    ]

    return {
        "bomFormat": "CycloneDX",
        "specVersion": "1.6",
        "serialNumber": f"urn:uuid:{doc_uuid}",
        "version": 1,
        "metadata": {
            "timestamp": timestamp,
            "tools": {
                "components": [
                    {"type": "application", "name": "generate-sbom.py", "version": "1.0.0"}
                ]
            },
            "component": _cdx_component(root_pkg),
            "licenses": [cdx_license(root_pkg["license"])],
        },
        "components": [_cdx_component(p) for p in other_pkgs],
        "dependencies": dependencies,
    }


# ── verification ──────────────────────────────────────────────────────────────

def verify(meta: dict) -> bool:
    expected = {p["name"] for p in meta["packages"]}
    ok = True

    if not SPDX_OUT.exists():
        print(f"MISSING: {SPDX_OUT}", file=sys.stderr)
        ok = False
    else:
        found = {
            line.split(": ", 1)[1].strip()
            for line in SPDX_OUT.read_text().splitlines()
            if line.startswith("PackageName: ")
        }
        missing = expected - found
        extra = found - expected
        if missing or extra:
            if missing:
                print(f"sbom.spdx: missing packages: {missing}", file=sys.stderr)
            if extra:
                print(f"sbom.spdx: unexpected packages: {extra}", file=sys.stderr)
            ok = False
        else:
            print(f"OK sbom.spdx  ({len(found)} packages)")

    if not CDX_OUT.exists():
        print(f"MISSING: {CDX_OUT}", file=sys.stderr)
        ok = False
    else:
        cdx = json.loads(CDX_OUT.read_text())
        found = {cdx["metadata"]["component"]["name"]}
        found |= {c["name"] for c in cdx.get("components", [])}
        missing = expected - found
        extra = found - expected
        if missing or extra:
            if missing:
                print(f"sbom.cdx.json: missing packages: {missing}", file=sys.stderr)
            if extra:
                print(f"sbom.cdx.json: unexpected packages: {extra}", file=sys.stderr)
            ok = False
        else:
            print(f"OK sbom.cdx.json  ({len(found)} packages)")

    return ok


# ── main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    meta = load_metadata()
    doc_uuid = make_doc_uuid(meta["project"])
    timestamp = get_metadata_timestamp()

    if "--verify" in sys.argv:
        if not verify(meta):
            print(
                "\nSBOM is stale. Run: ./tools/generate-sbom.sh",
                file=sys.stderr,
            )
            sys.exit(1)
        return

    spdx_text = generate_spdx(meta, timestamp, doc_uuid)
    cdx_obj = generate_cdx(meta, timestamp, doc_uuid)

    SPDX_OUT.write_text(spdx_text, encoding="utf-8")
    CDX_OUT.write_text(
        json.dumps(cdx_obj, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    print(f"Generated: {SPDX_OUT.relative_to(REPO_ROOT)}")
    print(f"Generated: {CDX_OUT.relative_to(REPO_ROOT)}")
    print(f"Packages:  {len(meta['packages'])}")
    print(f"Relations: {len(meta['relationships'])}")
    print(f"Timestamp: {timestamp}")


if __name__ == "__main__":
    main()
