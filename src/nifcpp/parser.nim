# C++ Parser Main Entry Point for nifcpp
# This file imports the submodules and provides the main parse function.

import std/sequtils
import std/options
import std/strutils # Added import for strutils.contains
import ./lexer
import ./ast
import ./parser_core # Core types, helpers, forward declarations, attribute impls, constructor
import ./parser_expressions # Expression parsing implementations
import ./parser_statements # Statement parsing implementations
import ./parser_types # Type, declarator, parameter parsing implementations

# --- Top Level Parsing ---

proc parseTranslationUnit*(p: var Parser): TranslationUnit =
  result = TranslationUnit(declarations: @[], line: 1, col: 1) # Initialize result

  while p.l.currentToken.kind != tkEndOfFile:
    # Skip leading newlines/whitespace tokens if lexer produces them
    while p.l.currentToken.kind == tkNewline:
      consumeToken(p)
    if p.l.currentToken.kind == tkEndOfFile: break

    # Parse one top-level declaration/statement
    # parseStatement is defined in parser_statements.nim (imported)
    # and forward-declared in parser_core.nim
    let stmt = p.parseStatement()
    if stmt != nil:
      result.declarations.add(stmt)
    else:
      # If parseStatement returned nil, it means parsing failed.
      # We need to consume the problematic token to avoid an infinite loop.
      # An error should have already been added by parseStatement or its sub-procs.
      if p.l.currentToken.kind != tkEndOfFile:
        let errorToken = p.l.currentToken
        # Add a generic error only if no specific error was added for this token
        var foundErrorForToken = false
        if p.errors.len > 0:
          # Check if the *last* error corresponds to the *current* token's position
          # This check might be fragile if errors don't include line/col info consistently
          # Explicitly use strutils.contains for string checking
          if strutils.contains(p.errors[^1], "at " & $errorToken.line & ":" & $errorToken.col):
              foundErrorForToken = true
        if not foundErrorForToken:
            p.errors.add("Skipping unexpected token at top level: " & $errorToken.kind & " at " & $errorToken.line & ":" & $errorToken.col)
        consumeToken(p) # Consume the token to advance
      else:
        # Should not happen if loop condition is correct, but handle defensively
        break

  return result

# --- Main Entry Point ---
proc parse*(l: Lexer): (Option[TranslationUnit], seq[string]) =
  # newParser is defined in parser_core.nim
  var p = newParser(l)
  let tu = p.parseTranslationUnit()

  if p.errors.len > 0:
    return (none(TranslationUnit), p.errors)
  else:
    return (some(tu), p.errors)
