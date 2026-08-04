#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT_DIR/scripts/lib/native_host_guard.sh"
ssa_native_guard_repo "$ROOT_DIR" || exit 1
ssa_native_guard_tools rm mkdir python3 cmake || exit 1
ROWS="${SSA_IMPORT_SMOKE_ROWS:-80000}"
BUILD_DIR="${SSA_BUILD_DIR:-"$ROOT_DIR/build/dev"}"
RUNTIME_DIR="${SSA_IMPORT_SMOKE_RUNTIME:-"$ROOT_DIR/build/runtime/import-smoke"}"
ssa_native_guard_path "$RUNTIME_DIR" "$ROOT_DIR" || exit 1
INPUT_DIR="$RUNTIME_DIR/docs_entrada"
DATA_DIR="$RUNTIME_DIR/data"
DB_PATH="$DATA_DIR/ssas.db"
XLSX_PATH="$INPUT_DIR/large.xlsx"
METRICS_PATH="$RUNTIME_DIR/import-metrics.txt"

rm -rf "$INPUT_DIR"
mkdir -p "$INPUT_DIR" "$DATA_DIR"
rm -f "$DB_PATH" "$METRICS_PATH"

python3 - "$DB_PATH" <<'PY'
import sqlite3
import sys

sqlite3.connect(sys.argv[1]).close()
PY

python3 - "$XLSX_PATH" "$ROWS" <<'PY'
import html
import sys
import zipfile

path = sys.argv[1]
rows = int(sys.argv[2])

def cell(ref, value):
    return f'<c r="{ref}" t="inlineStr"><is><t>{html.escape(str(value))}</t></is></c>'

with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    zf.writestr("[Content_Types].xml", """<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>
<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>
</Types>""")
    zf.writestr("_rels/.rels", """<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rIdWorkbook" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>
</Relationships>""")
    zf.writestr("xl/workbook.xml", """<?xml version="1.0" encoding="UTF-8"?>
<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
<sheets><sheet name="Consulta SSA" sheetId="1" r:id="rId1"/></sheets>
</workbook>""")
    zf.writestr("xl/_rels/workbook.xml.rels", """<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>
</Relationships>""")
    with zf.open("xl/worksheets/sheet1.xml", "w") as sheet:
        sheet.write(b'<?xml version="1.0" encoding="UTF-8"?>')
        sheet.write(f'<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><dimension ref="A1:D{rows + 1}"/><sheetData>'.encode())
        sheet.write(('<row r="1">' + cell("A1", "Numero SSA") + cell("B1", "Situacao") + cell("C1", "Setor Executor") + cell("D1", "Descricao da SSA") + '</row>').encode())
        for index in range(rows):
            row_number = index + 2
            ssa_number = 202600000 + index
            sector = "MEL1" if index % 2 == 0 else "SMIN"
            text = f"Smoke import row {index}"
            sheet.write((f'<row r="{row_number}">' + cell(f"A{row_number}", ssa_number) + cell(f"B{row_number}", "ASE") + cell(f"C{row_number}", sector) + cell(f"D{row_number}", text) + '</row>').encode())
        sheet.write(b"</sheetData></worksheet>")
PY

cmake --build --preset dev --target ssa_consulta_rapida_cli

TIME_BIN="/usr/bin/time"
if "$TIME_BIN" -l true >/dev/null 2>&1; then
    "$TIME_BIN" -l "$BUILD_DIR/ssa_consulta_rapida_cli" \
        --db "$DB_PATH" --docs-dir "$INPUT_DIR" --force-rescan 2>"$METRICS_PATH"
else
    "$TIME_BIN" -v "$BUILD_DIR/ssa_consulta_rapida_cli" \
        --db "$DB_PATH" --docs-dir "$INPUT_DIR" --force-rescan 2>"$METRICS_PATH"
fi

python3 - "$DB_PATH" "$ROWS" <<'PY'
import sqlite3
import sys

db_path = sys.argv[1]
expected = int(sys.argv[2])
with sqlite3.connect(db_path) as db:
    actual = db.execute("SELECT COUNT(*) FROM ssa_table").fetchone()[0]
if actual != expected:
    raise SystemExit(f"expected {expected} rows, got {actual}")
print(f"rows={actual}")
PY

cat "$METRICS_PATH"
