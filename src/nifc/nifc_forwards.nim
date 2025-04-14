# src/nifc/nifc_forwards.nim
# Forward declarations for NIFC code generation modules to break cycles.

import common_defs # For GeneratedCode, Cursor, VarKind, Literals etc.

# Forward declarations for procedures involved in cycles:
# Use .forward. pragma - the body is provided by another module.
proc genType*(c: var GeneratedCode; n: var Cursor; literals: Literals; name = "") {.forward.}
proc genx*(c: var GeneratedCode; n: var Cursor) {.forward.}
proc genLvalue*(c: var GeneratedCode; n: var Cursor) {.forward.}
proc genCall*(c: var GeneratedCode; n: var Cursor) {.forward.}
proc genCallCanRaise*(c: var GeneratedCode; n: var Cursor) {.forward.}
proc genStmt*(c: var GeneratedCode; n: var Cursor) {.forward.}
proc genVarDecl*(c: var GeneratedCode; n: var Cursor; vk: VarKind; toExtern = false): void {.forward.}
proc genOnError*(c: var GeneratedCode; n: var Cursor) {.forward.}
proc genEmitStmt*(c: var GeneratedCode; n: var Cursor) {.forward.} # Needed by genToplevel in codegen
