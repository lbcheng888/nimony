# C++ Lexer (Tokenizer) for nifcpp

import std/strutils
import std/streams except readChar # Exclude streams.readChar to avoid name collision
import std/tables # Import the tables module

type
  TokenType* = enum
    # Keywords
    tkClass, tkStruct, tkEnum, tkUnion,
    tkVoid, tkBool, tkChar, tkInt, tkFloat, tkDouble, tkLong, tkShort, tkSigned, tkUnsigned, tkAuto,
    tkConst, tkStatic, tkExtern, tkInline, tkVolatile, tkTypedef, tkUsing, tkNamespace, tkDecltype, # Added tkDecltype
    tkPublic, tkPrivate, tkProtected,
    tkIf, tkElse, tkSwitch, tkCase, tkDefault, tkFor, tkWhile, tkDo, tkBreak, tkContinue, tkReturn, tkGoto,
    tkTry, tkCatch, tkThrow, tkNoexcept,
    tkNew, tkDelete, tkThis, tkOperator, tkTemplate, tkTypename, tkFinal, # Added tkFinal

    # Identifiers and Literals
    tkIdentifier, tkIntegerLit, tkFloatLit, tkStringLit, tkCharLit,

    # Operators and Punctuation
    tkPlus, tkMinus, tkStar, tkSlash, tkPercent,         # +, -, *, /, %
    tkIncrement, tkDecrement,                             # ++, --
    tkEqual, tkNotEqual, tkLess, tkLessEqual, tkGreater, tkGreaterEqual, # ==, !=, <, <=, >, >=
    tkLogicalAnd, tkLogicalOr, tkLogicalNot,             # &&, ||, !
    tkBitwiseAnd, tkBitwiseOr, tkBitwiseXor, tkBitwiseNot, # &, |, ^, ~
    tkLeftShift, tkRightShift,                           # <<, >>
    tkAssign, tkPlusAssign, tkMinusAssign, tkMulAssign, tkDivAssign, tkModAssign, # =, +=, -=, *=, /=, %=
    tkAndAssign, tkOrAssign, tkXorAssign, tkLShiftAssign, tkRShiftAssign, # &=, |=, ^=, <<=, >>=
    tkLParen, tkRParen, tkLBrace, tkRBrace, tkLBracket, tkRBracket, # (, ), {, }, [, ]
    tkLBracket2, tkRBracket2,                             # [[, ]] (Attributes) - NEW
    tkComma, tkSemicolon, tkColon, tkDot, tkArrow, tkQuestion, # ,, ;, :, ., ->, ?
    tkScope,                                              # ::
    tkEllipsis,                                           # ...
    tkHash, tkHashHash,                                   # #, ## (Preprocessor)

    # Other
    tkWhitespace, tkNewline, tkComment, tkEndOfFile, tkError

  Token* = object
    kind*: TokenType
    literal*: string
    line*, col*: int

proc newToken*(kind: TokenType, literal: string, line, col: int): Token =
  result = Token(kind: kind, literal: literal, line: line, col: col)

# String representation for Token (for echo)
proc `$`*(t: Token): string =
  result = "Token(kind: " & $t.kind & ", literal: \"" & t.literal & "\", line: " & $t.line & ", col: " & $t.col & ")"

# Keyword map
const keywords*: Table[string, TokenType] = {
  "class": tkClass, "struct": tkStruct, "enum": tkEnum, "union": tkUnion,
  "void": tkVoid, "bool": tkBool, "char": tkChar, "int": tkInt, "float": tkFloat, "double": tkDouble, "long": tkLong, "short": tkShort, "signed": tkSigned, "unsigned": tkUnsigned, "auto": tkAuto,
  "const": tkConst, "static": tkStatic, "extern": tkExtern, "inline": tkInline, "volatile": tkVolatile, "typedef": tkTypedef, "using": tkUsing, "namespace": tkNamespace, "decltype": tkDecltype, # Added decltype
  "public": tkPublic, "private": tkPrivate, "protected": tkProtected,
  "if": tkIf, "else": tkElse, "switch": tkSwitch, "case": tkCase, "default": tkDefault, "for": tkFor, "while": tkWhile, "do": tkDo, "break": tkBreak, "continue": tkContinue, "return": tkReturn, "goto": tkGoto,
  "try": tkTry, "catch": tkCatch, "throw": tkThrow, "noexcept": tkNoexcept,
  "new": tkNew, "delete": tkDelete, "this": tkThis, "operator": tkOperator, "template": tkTemplate, "typename": tkTypename, "final": tkFinal # Added final
}.toTable()

type
  Lexer* = object
    input: string
    position: int # current position in input (points to current char)
    readPosition: int # current reading position in input (after current char)
    ch: char # current char under examination
    line, col: int # current line and column number
    # Token buffer for lookahead
    currentToken*: Token
    peekToken*: Token

proc readChar(l: var Lexer) =
  if l.readPosition >= l.input.len:
    l.ch = '\0' # Use NUL character for EOF
  else:
    l.ch = l.input[l.readPosition]
  l.position = l.readPosition
  l.readPosition += 1
  if l.ch == '\n':
    l.line += 1
    l.col = 1
  else:
    l.col += 1

proc peekChar(l: Lexer): char =
  if l.readPosition >= l.input.len:
    return '\0'
  else:
    return l.input[l.readPosition]

# --- Helper Functions --- (Declare before use in readNextTokenInternal)

proc skipWhitespace(l: var Lexer) =
  while l.ch == ' ' or l.ch == '\t' or l.ch == '\r': # Don't skip newline here, handle separately if needed
    readChar(l)

proc skipComment(l: var Lexer): bool =
  if l.ch == '/' and peekChar(l) == '/':
    # Single-line comment
    while l.ch != '\n' and l.ch != '\0':
      readChar(l)
    # Don't consume the newline, let the main loop handle it if needed
    return true
  elif l.ch == '/' and peekChar(l) == '*':
    # Multi-line comment
    let startLine = l.line
    let startCol = l.col
    readChar(l) # Consume '/'
    readChar(l) # Consume '*'
    while not (l.ch == '*' and peekChar(l) == '/') and l.ch != '\0':
      readChar(l)
    if l.ch == '\0':
      # Unterminated comment - report error? For now, just stop.
      # Consider adding an error token or mechanism later.
      echo "Warning: Unterminated multi-line comment starting at ", startLine, ":", startCol
    else:
      readChar(l) # Consume '*'
      readChar(l) # Consume '/'
    return true
  return false

proc isLetter(ch: char): bool =
  result = (ch >= 'a' and ch <= 'z') or (ch >= 'A' and ch <= 'Z') or ch == '_'

proc isDigit(ch: char): bool =
  result = ch >= '0' and ch <= '9'

proc readIdentifier(l: var Lexer): string =
  let startPosition = l.position
  while isLetter(l.ch) or isDigit(l.ch):
    readChar(l)
  result = l.input.substr(startPosition, l.position - 1)

proc readNumber(l: var Lexer): (string, TokenType) =
  let startPosition = l.position
  var tokenType = tkIntegerLit # Assume integer initially
  while isDigit(l.ch):
    readChar(l)

  # Check for floating point
  if l.ch == '.' and isDigit(peekChar(l)):
    tokenType = tkFloatLit
    readChar(l) # Consume '.'
    while isDigit(l.ch):
      readChar(l)

  # TODO: Handle exponents (e/E)
  # TODO: Handle suffixes (f, L, U, UL, ULL, etc.)

  let literal = l.input.substr(startPosition, l.position - 1)
  return (literal, tokenType)

proc readString(l: var Lexer): string =
  # Assumes l.ch is currently '"'
  let startPosition = l.position + 1 # Start after the opening quote
  var literal = ""
  while true:
    readChar(l)
    if l.ch == '\\': # Handle escape sequences
      readChar(l) # Consume the character after backslash
      case l.ch:
      of 'n': literal.add('\n')
      of 't': literal.add('\t')
      of 'r': literal.add('\r')
      of '\\': literal.add('\\')
      of '"': literal.add('"')
      of '\'': literal.add('\'')
      # TODO: Add more escapes (\0, \xHH, \uHHHH, \UHHHHHHHH)
      else: literal.add(l.ch) # Add the escaped char itself if not special
    elif l.ch == '"' or l.ch == '\0':
      break # End of string or EOF
    else:
      literal.add(l.ch)

  if l.ch == '"':
    readChar(l) # Consume the closing quote
  else:
    echo "Warning: Unterminated string literal" # Or return error token
  return literal

proc readCharLiteral(l: var Lexer): string =
  # Assumes l.ch is currently '\''
  let startLine = l.line
  let startCol = l.col
  readChar(l) # Consume opening quote
  var literal = ""
  if l.ch == '\\': # Handle escape sequence
    readChar(l) # Consume backslash
    case l.ch:
      of 'n': literal = "\n" # Store actual char value? Or literal representation? Let's use literal for now.
      of 't': literal = "\t"
      of 'r': literal = "\r"
      of '\\': literal = "\\"
      of '"': literal = "\""
      of '\'': literal = "\'"
      # TODO: More escapes
      else: literal = $l.ch
    readChar(l) # Consume escaped char
  elif l.ch != '\'' and l.ch != '\0':
    literal = $l.ch
    readChar(l) # Consume the character
  # Else: Empty char literal ''? Invalid.

  if l.ch == '\'':
    readChar(l) # Consume closing quote
  else:
    echo "Warning: Unterminated or invalid character literal starting at ", startLine, ":", startCol
    literal = "" # Return empty on error?

  return literal # Return the literal string representation for now


# --- Internal Token Reading Logic ---
proc readNextTokenInternal(l: var Lexer): Token =
  var tok: Token

  while true: # Loop to skip whitespace/comments
    skipWhitespace(l) # Use function call syntax
    if not skipComment(l): # Use function call syntax
      break # Exit loop if not a comment

  let line = l.line
  let col = l.col

  # Logic from the previous nextToken implementation
  case l.ch:
  of '\0': tok = newToken(tkEndOfFile, "", line, col)
  of '\n':
    tok = newToken(tkNewline, "\n", line, col)
    readChar(l) # Consume newline
    return tok # Return directly as we consumed the char
  # Operators and Punctuation
  of '=':
    if peekChar(l) == '=':
      readChar(l); tok = newToken(tkEqual, "==", line, col)
    else: tok = newToken(tkAssign, "=", line, col)
  of '+':
    if peekChar(l) == '+':
      readChar(l); tok = newToken(tkIncrement, "++", line, col)
    elif peekChar(l) == '=':
      readChar(l); tok = newToken(tkPlusAssign, "+=", line, col)
    else: tok = newToken(tkPlus, "+", line, col)
  of '-':
    if peekChar(l) == '-':
      readChar(l); tok = newToken(tkDecrement, "--", line, col)
    elif peekChar(l) == '=':
      readChar(l); tok = newToken(tkMinusAssign, "-=", line, col)
    elif peekChar(l) == '>':
      readChar(l); tok = newToken(tkArrow, "->", line, col)
    else: tok = newToken(tkMinus, "-", line, col)
  of '*':
    if peekChar(l) == '=':
      readChar(l); tok = newToken(tkMulAssign, "*=", line, col)
    else: tok = newToken(tkStar, "*", line, col)
  of '/':
    # Comment handling already done
    if peekChar(l) == '=':
      readChar(l); tok = newToken(tkDivAssign, "/=", line, col)
    else: tok = newToken(tkSlash, "/", line, col)
  of '%':
    if peekChar(l) == '=':
      readChar(l); tok = newToken(tkModAssign, "%=", line, col)
    else: tok = newToken(tkPercent, "%", line, col)
  of '<':
    if peekChar(l) == '<':
      readChar(l)
      if peekChar(l) == '=':
        readChar(l); tok = newToken(tkLShiftAssign, "<<=", line, col)
      else: tok = newToken(tkLeftShift, "<<", line, col)
    elif peekChar(l) == '=':
      readChar(l); tok = newToken(tkLessEqual, "<=", line, col)
    else: tok = newToken(tkLess, "<", line, col)
  of '>':
    if peekChar(l) == '>':
      readChar(l)
      if peekChar(l) == '=':
        readChar(l); tok = newToken(tkRShiftAssign, ">>=", line, col)
      else: tok = newToken(tkRightShift, ">>", line, col)
    elif peekChar(l) == '=':
      readChar(l); tok = newToken(tkGreaterEqual, ">=", line, col)
    else: tok = newToken(tkGreater, ">", line, col)
  of '&':
    if peekChar(l) == '&':
      readChar(l); tok = newToken(tkLogicalAnd, "&&", line, col)
    elif peekChar(l) == '=':
      readChar(l); tok = newToken(tkAndAssign, "&=", line, col)
    else: tok = newToken(tkBitwiseAnd, "&", line, col)
  of '|':
    if peekChar(l) == '|':
      readChar(l); tok = newToken(tkLogicalOr, "||", line, col)
    elif peekChar(l) == '=':
      readChar(l); tok = newToken(tkOrAssign, "|=", line, col)
    else: tok = newToken(tkBitwiseOr, "|", line, col)
  of '^':
    if peekChar(l) == '=':
      readChar(l); tok = newToken(tkXorAssign, "^=", line, col)
    else: tok = newToken(tkBitwiseXor, "^", line, col)
  of '!':
    if peekChar(l) == '=':
      readChar(l); tok = newToken(tkNotEqual, "!=", line, col)
    else: tok = newToken(tkLogicalNot, "!", line, col)
  of '~': tok = newToken(tkBitwiseNot, "~", line, col)
  of '(': tok = newToken(tkLParen, "(", line, col)
  of ')': tok = newToken(tkRParen, ")", line, col)
  of '{': tok = newToken(tkLBrace, "{", line, col)
  of '}': tok = newToken(tkRBrace, "}", line, col)
  of '[':
    if peekChar(l) == '[':
      readChar(l); tok = newToken(tkLBracket2, "[[", line, col)
    else: tok = newToken(tkLBracket, "[", line, col)
  of ']':
    if peekChar(l) == ']':
      readChar(l); tok = newToken(tkRBracket2, "]]", line, col)
    else: tok = newToken(tkRBracket, "]", line, col)
  of ',': tok = newToken(tkComma, ",", line, col)
  of ';': tok = newToken(tkSemicolon, ";", line, col)
  of ':':
    if peekChar(l) == ':':
      readChar(l); tok = newToken(tkScope, "::", line, col)
    else: tok = newToken(tkColon, ":", line, col)
  of '.':
    if peekChar(l) == '.':
       readChar(l)
       if peekChar(l) == '.':
         readChar(l); tok = newToken(tkEllipsis, "...", line, col)
       else:
         tok = newToken(tkError, "..", line, col) # Error for '..'
    else:
      tok = newToken(tkDot, ".", line, col)
  of '?': tok = newToken(tkQuestion, "?", line, col)
  of '#':
     if peekChar(l) == '#':
       readChar(l); tok = newToken(tkHashHash, "##", line, col)
     else: tok = newToken(tkHash, "#", line, col)
  # Literals
  of '"':
    let literal = readString(l) # Use function call syntax
    tok = newToken(tkStringLit, literal, line, col)
    return tok # Return directly as readString handles advancement
  of '\'':
    let literal = readCharLiteral(l) # Use function call syntax
    tok = newToken(tkCharLit, literal, line, col)
    return tok # Return directly as readCharLiteral handles advancement
  else:
    # Check for identifiers or keywords
    if isLetter(l.ch): # Use function call syntax
      let literal = readIdentifier(l) # Use function call syntax
      let kind = keywords.getOrDefault(literal, tkIdentifier)
      tok = newToken(kind, literal, line, col)
      return tok # Return directly as readIdentifier handles advancement
    # Check for numbers
    elif isDigit(l.ch): # Use function call syntax
      let (literal, kind) = readNumber(l) # Use function call syntax
      tok = newToken(kind, literal, line, col)
      return tok # Return directly as readNumber handles advancement
    else:
      # Unrecognized character
      tok = newToken(tkError, $l.ch, line, col)

  # Advance lexer for single/double character tokens processed above
  # (This is reached only for operators/punctuation not handled by literal readers)
  readChar(l)
  return tok

# --- Public Interface ---

proc newLexer*(input: string): Lexer =
  # Initialize basic fields
  result = Lexer(input: input, position: 0, readPosition: 0, ch: '\0', line: 1, col: 1)
  # Read first character needed by readNextTokenInternal
  readChar(result)
  # Prime the token buffer by reading the first two tokens
  result.currentToken = readNextTokenInternal(result)
  result.peekToken = readNextTokenInternal(result)

# Advances the lexer to the next token.
# The parser should call this to consume the current token.
proc nextToken*(l: var Lexer) =
  l.currentToken = l.peekToken
  l.peekToken = readNextTokenInternal(l)

# Example usage (for testing, can be removed later):
when isMainModule:
  let input = """
  int main() {
    // This is a comment
    float value = 123.45;
    if (value > 100) {
      return 0; /* multi
                  line */
    } else {
      std::cout << "Hello" << std::endl;
    }
    return 1;
  }
  """
  var l = newLexer(input)
  while l.currentToken.kind != tkEndOfFile:
    echo l.currentToken
    nextToken(l) # Use the new advance function
  # Removed the duplicated case statement from here
