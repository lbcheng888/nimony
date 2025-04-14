import argparse
import sys

# Adjust import paths based on how the script is run
try:
    from .parser import parse, ParseError
    from .ast_nodes import print_ast, Program
    # Import SemanticAnalyzer and its error, Environment is needed by both SA and CG
    from .semantic_analyzer import SemanticAnalyzer, SemanticError, Environment
    from .macro_expander import MacroExpander, MacroExpansionError, expand_macros
    # Assuming generate_code is the primary entry point for the C generator now
    from .code_generator import CodeGenerationError, generate_c_code as generate_code # Alias if needed
except ImportError:
    # Fallback for running script directly
    from parser import parse, ParseError
    from ast_nodes import print_ast, Program
    from semantic_analyzer import SemanticAnalyzer, SemanticError, Environment
    from macro_expander import MacroExpander, MacroExpansionError, expand_macros
    from code_generator import CodeGenerationError, generate_c_code as generate_code # Alias if needed

def compile_hrt_lisp(source_code: str, print_stages: bool = False) -> str:
    """
    Compiles HRT-Lisp source code to Python code.
    Orchestrates parsing, macro expansion, semantic analysis (optional), and code generation.
    """
    try:
        # 1. Parsing
        if print_stages: print("\n--- Parsing ---")
        ast = parse(source_code)
        if print_stages:
            print("Initial AST:")
            print_ast(ast)
            print("---------------")
        if not isinstance(ast, Program):
             raise TypeError("Parsing did not return a Program node.")

        # 2. Initial Semantic Analysis (for macro definitions)
        if print_stages: print("\n--- Initial Semantic Analysis (for Macros) ---")
        initial_analyzer = SemanticAnalyzer()
        # Analyze to define macros in the environment
        initial_analyzer.analyze(ast)
        macro_env = initial_analyzer.current_env # Get env with macros defined
        if print_stages: print("Macro definitions processed.")
        if print_stages: print("---------------------------------------------")

        # 3. Macro Expansion
        if print_stages: print("\n--- Macro Expansion ---")
        # Pass the environment containing macro definitions to the expander
        expanded_ast = expand_macros(ast, macro_env)
        if print_stages:
            print("Expanded AST:")
            print_ast(expanded_ast)
            print("---------------------")

        # 4. Full Semantic Analysis (Scopes and Types on Expanded AST)
        # Create a new analyzer instance for the final pass after expansion.
        if print_stages: print("\n--- Full Semantic Analysis (Scopes & Types) ---")
        final_analyzer = SemanticAnalyzer()
        try:
            # Analyze the expanded AST. This populates final_analyzer.current_env
            # with scope and type information.
            final_analyzer.analyze(expanded_ast)
            if print_stages: print("Full semantic analysis successful.")
        except SemanticError as e:
            # Halt compilation on semantic errors found after expansion
            print(f"Semantic Error during final analysis: {e}", file=sys.stderr)
            raise
        if print_stages: print("------------------------------------------------")


        # 5. Code Generation (C Code)
        if print_stages: print("\n--- C Code Generation ---")
        # Pass the expanded AST and the *analyzer instance* (which contains the environment
        # and expression types) from the final analysis pass to the C code generator.
        generated_code = generate_code(expanded_ast, final_analyzer) # Pass the analyzer instance
        if print_stages: print("-----------------------")

        return generated_code

    except ParseError as e:
        print(f"Parse Error: {e}", file=sys.stderr)
        sys.exit(1)
    except SemanticError as e:
        print(f"Semantic Error: {e}", file=sys.stderr)
        sys.exit(1)
    except MacroExpansionError as e:
        print(f"Macro Expansion Error: {e}", file=sys.stderr)
        sys.exit(1)
    except CodeGenerationError as e:
        print(f"Code Generation Error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"An unexpected error occurred: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)


def main():
    """Command-line interface."""
    parser = argparse.ArgumentParser(description="Compile HRT-Lisp code to Python.")
    parser.add_argument(
        "input_file",
        nargs='?', # Makes the argument optional
        help="Path to the HRT-Lisp input file (.hrtpy). Reads from stdin if not provided."
    )
    parser.add_argument(
        "-o", "--output",
        help="Path to the output Python file. Prints to stdout if not provided."
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Print AST and stages during compilation."
    )
    parser.add_argument(
        "-e", "--execute",
        action="store_true",
        help="Execute the generated Python code after compilation."
    )

    args = parser.parse_args()

    source_code = ""
    input_source_name = "stdin"
    if args.input_file:
        input_source_name = args.input_file
        try:
            with open(args.input_file, 'r', encoding='utf-8') as f:
                source_code = f.read()
        except FileNotFoundError:
            print(f"Error: Input file not found: {args.input_file}", file=sys.stderr)
            sys.exit(1)
        except IOError as e:
            print(f"Error reading input file {args.input_file}: {e}", file=sys.stderr)
            sys.exit(1)
    else:
        if sys.stdin.isatty():
             print("Enter HRT-Lisp code, press Ctrl+D (Unix) or Ctrl+Z then Enter (Windows) to finish:")
        source_code = sys.stdin.read()
        if not source_code:
             print("Error: No input provided via stdin.", file=sys.stderr)
             parser.print_help()
             sys.exit(1)


    if args.verbose:
        print(f"Compiling source from: {input_source_name}")

    generated_python_code = compile_hrt_lisp(source_code, print_stages=args.verbose)

    if args.output:
        try:
            with open(args.output, 'w', encoding='utf-8') as f:
                f.write(generated_python_code)
            if args.verbose:
                print(f"\nGenerated Python code written to: {args.output}")
        except IOError as e:
            print(f"Error writing output file {args.output}: {e}", file=sys.stderr)
            sys.exit(1)
    else:
        # Print to stdout if no output file specified and not executing directly
        if not args.execute:
             print("\n--- Generated Python Code ---")
             print(generated_python_code)
             print("---------------------------")

    if args.execute:
        if args.verbose:
            print("\n--- Executing Generated Code ---")
        try:
            # Execute in a dictionary to capture global scope changes if needed
            exec_globals = {}
            exec(generated_python_code, exec_globals)
        except Exception as e:
            print(f"\nExecution Error: {e}", file=sys.stderr)
            import traceback
            traceback.print_exc()
            sys.exit(1)
        if args.verbose:
            print("------------------------------")

if __name__ == "__main__":
    main()
