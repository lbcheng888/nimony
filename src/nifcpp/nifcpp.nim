import std / [parseopt, strutils, streams, os, syncio, options]
# Removed 'except nil' as it's incorrect syntax. syncio.readFile should be found.
import lexer
import parser
import parser_impl # Import parser implementation
import transformer
import ast
import ../lib / [nifbuilder, lineinfos] # Corrected import path

proc main(inputFile: string) =
  if not fileExists(inputFile):
    echo "Error: Input file not found: ", inputFile
    quit(1)

  let content = readFile(inputFile) # Now should resolve to syncio.readFile
  var L = newLexer(content) # Use newLexer with content string

  # Call parser.parse which handles initialization and returns result/errors
  let (parseResult, errors) = parser.parse(L)

  if errors.len > 0:
    echo "Parsing errors:"
    for err in errors:
      echo "- ", err
    quit(1)

  if parseResult.isNone():
    echo "Error: Parsing failed without specific errors."
    quit(1)

  let cppAst = parseResult.get() # Get the TranslationUnit AST

  # --- Transformation ---
  var builder = nifbuilder.open(1024) # Initialize NIF builder
  var ctx = transformer.newTransformerContext() # Initialize transformer context

  # Add NIF header
  builder.addHeader("nifcpp", "cpp") # Add standard NIF header

  # Perform transformation by calling transformNode on the root AST node
  transformer.transformNode(cppAst, builder, ctx)

  # Check for transformation errors
  if ctx.errors.len > 0:
    echo "Transformation errors:"
    for err in ctx.errors:
      echo "- ", err
    # Decide if transformation errors should halt execution
    # quit(1) # Optional: quit on transformation errors

  # --- Output ---
  # Extract the final NIF string from the builder
  let nifOutput = builder.extract()
  echo nifOutput # Print the generated NIF code


when isMainModule:
  var p = initOptParser()
  var inputFile = ""
  for kind, key, val in p.getopt():
    case kind
    of cmdArgument:
      if inputFile.len == 0:
        inputFile = key
      else:
        echo "Error: Only one input file is supported."
        quit(1)
    else:
      echo "Error: Unknown option: ", key
      quit(1)

  if inputFile.len == 0:
    echo "Usage: nifcpp <input_cpp_file>"
    quit(1)

  main(inputFile)
