# C++ Abstract Syntax Tree (AST) Node Definitions for nifcpp

import std/tables
import std/options # Import Option type
import ./lexer # Import Token types etc.

type
  # Forward declaration for recursive types if needed later
  CppAstNode* = ref object of RootObj
    line*, col*: int # Source code location (approximated for now)

  # Represents a single attribute argument (simplified for now)
  AttributeArgument* = ref object of CppAstNode
    # TODO: Define structure more precisely. For now, maybe just store tokens?
    # Let's represent it as a generic Expression for simplicity initially.
    value*: Expression

  # Represents a single attribute (e.g., nodiscard, deprecated("reason"))
  Attribute* = ref object of CppAstNode
    scope*: Identifier # Optional namespace/scope (e.g., 'gnu' in [[gnu::const]])
    name*: Identifier
    arguments*: seq[AttributeArgument] # Arguments in parentheses (...)

  # Represents a group of attributes within double brackets [[...]]
  AttributeGroup* = ref object of CppAstNode
    attributes*: seq[Attribute]
    # line*, col* inherited from CppAstNode

  # Base type for all statements
  Statement* = ref object of CppAstNode
    # line*, col* inherited from CppAstNode
    attributes*: seq[AttributeGroup] # NEW: Optional list of attribute groups

  # Enum for C++ access specifiers
  AccessSpecifier* = enum
    asDefault, # Default (private for class, public for struct) - or maybe use asNone?
    asPublic,
    asPrivate,
    asProtected

  # Base type for all expressions
  Expression* = ref object of CppAstNode

  # Base type for all type expressions
  TypeExpr* = ref object of CppAstNode
    # line*, col* inherited from CppAstNode
    attributes*: seq[AttributeGroup] # NEW: Optional list of attribute groups

  # Specific Node Types (Examples - to be expanded significantly)

  # Represents an identifier (variable name, function name, etc.)
  Identifier* = ref object of Expression
    name*: string

  # Represents an integer literal
  IntegerLiteral* = ref object of Expression
    value*: int64 # Use int64 for wider range

  # Represents a floating-point literal
  FloatLiteral* = ref object of Expression
    value*: float64

  # Represents a string literal
  StringLiteral* = ref object of Expression
    value*: string

  # Represents a character literal
  CharLiteral* = ref object of Expression
    value*: char

  # Represents a base type name (e.g., int, void, float, custom_type)
  BaseTypeExpr* = ref object of TypeExpr
    name*: string # The name of the type

  # Represents a pointer type (e.g., int*, MyClass**)
  PointerTypeExpr* = ref object of TypeExpr
    baseType*: TypeExpr # The type being pointed to

  # Represents a reference type (e.g., int&, const MyClass&)
  ReferenceTypeExpr* = ref object of TypeExpr
    baseType*: TypeExpr # The type being referenced
    isConst*: bool     # True if it's a reference to const (e.g., const T&)

  # Represents a const-qualified type (e.g., const int, int* const)
  ConstQualifiedTypeExpr* = ref object of TypeExpr
    baseType*: TypeExpr # The type being qualified as const

  # Represents a volatile-qualified type (e.g., volatile int, int* volatile)
  VolatileQualifiedTypeExpr* = ref object of TypeExpr
    baseType*: TypeExpr # The type being qualified as volatile

  # Represents an initializer list expression (e.g., {1, 2, 3})
  InitializerListExpr* = ref object of Expression
    values*: seq[Expression] # The expressions within the braces

  # Represents a binary operation (e.g., a + b)
  BinaryExpression* = ref object of Expression
    left*: Expression
    op*: TokenType # e.g., tkPlus, tkMinus, tkAssign
    right*: Expression

  # Represents a unary operation (e.g., -x, !flag, ++i, i++, *ptr, &var)
  UnaryExpression* = ref object of Expression
    op*: TokenType    # e.g., tkMinus, tkLogicalNot, tkIncrement, tkDecrement, tkStar, tkBitwiseAnd
    operand*: Expression
    isPrefix*: bool  # true for prefix (++i), false for postfix (i++)

  # Represents a function call
  CallExpression* = ref object of Expression
    function*: Expression # Usually an Identifier
    arguments*: seq[Expression]

  # Represents member access (e.g., obj.member, ptr->member)
  MemberAccessExpression* = ref object of Expression # Added export *
    targetObject*: Expression # Renamed from 'object'
    member*: Identifier # The member being accessed
    isArrow*: bool     # True for '->', false for '.'

  # Represents array subscripting (e.g., arr[index])
  ArraySubscriptExpression* = ref object of Expression # Added export *
    array*: Expression # The array expression
    index*: Expression # The index expression

  # Represents an expression statement (e.g., functionCall(); )
  ExpressionStatement* = ref object of Statement
    expression*: Expression

  # Represents a block of statements { ... }
  BlockStatement* = ref object of Statement
    statements*: seq[Statement]

  # Represents a variable declaration (e.g., int x = 5;)
  # Needs significant expansion for const, static, pointers, references etc.
  VariableDeclaration* = ref object of Statement
    access*: AccessSpecifier # Access level within a class/struct
    varType*: TypeExpr # Type information, potentially including array/function details
    name*: Identifier
    # arrayDimensions removed, now part of varType if it's ArrayTypeExpr
    initializer*: Expression # Optional, can be nil (incl. aggregate initializers like {1,2})

  # Represents a function definition
  # Needs significant expansion for parameters, templates etc.
  FunctionDefinition* = ref object of Statement # Or maybe a top-level declaration node?
    access*: AccessSpecifier # Access level within a class/struct
    returnType*: TypeExpr # Use TypeExpr
    name*: Identifier
    parameters*: seq[Parameter] # Placeholder for Parameter type
    body*: BlockStatement

  # Represents a function parameter
  Parameter* = ref object of CppAstNode # Similar to VariableDeclaration but specific context
    paramType*: TypeExpr
    name*: Identifier
    defaultValue*: Expression # Optional default value, can be nil

  # Represents a return statement (e.g., return x;)
  ReturnStatement* = ref object of Statement
    returnValue*: Expression # Optional, can be nil for 'return;'

  # Represents an if statement (if (...) ... [else ...])
  IfStatement* = ref object of Statement
    condition*: Expression
    thenBranch*: Statement
    elseBranch*: Statement # Optional, can be nil

  # Represents a while loop (while (...) ...)
  WhileStatement* = ref object of Statement
    condition*: Expression
    body*: Statement

  # Represents a for loop (for (init; condition; update) body)
  # Note: init can be a declaration or an expression statement.
  # We might need a union type or a base type for 'init' if we want strict typing,
  # or use Statement and check its kind later. Using Statement for now.
  ForStatement* = ref object of Statement
    initializer*: Statement  # Can be VariableDeclaration, ExpressionStatement, or nil
    condition*: Expression   # Optional, can be nil
    update*: Expression      # Optional, can be nil
    body*: Statement

  # Represents a break statement (e.g., break;)
  BreakStatement* = ref object of Statement # Renamed from BreakStmt for consistency

  # Represents a continue statement (e.g., continue;)
  ContinueStatement* = ref object of Statement # Renamed from ContinueStmt for consistency

  # Represents a case label within a switch statement (e.g., case 1:, default:)
  CaseStatement* = ref object of Statement # Treat case/default labels as statements
    value*: Expression     # Constant expression for the case, nil for default
    # Body removed - statements follow sequentially within the switch's BlockStatement

  # Represents a switch statement (switch (...) { ... })
  SwitchStatement* = ref object of Statement
    controlExpr*: Expression # The expression being switched on
    body*: BlockStatement    # The block containing case/default labels and statements

  # Represents a class/struct definition
  # Needs significant expansion for inheritance, members, access specifiers etc.
  ClassDefinition* = ref object of Statement # Or top-level declaration node?
    kind*: TokenType # tkClass or tkStruct
    name*: Identifier
    isFinal*: bool # True if 'final' specifier is present
    bases*: seq[InheritanceInfo] # List of base classes and their access specifiers
    members*: seq[Statement] # Simplified member list (vars, funcs, nested types)

  # Represents information about a single base class in an inheritance list
  InheritanceInfo* = ref object of CppAstNode
    access*: AccessSpecifier # public, private, protected (or default)
    baseName*: Identifier    # Name of the base class

  # Enum for the kind of template parameter
  TemplateParameterKind* = enum
    tpTypename, # typename T / class T
    tpType,     # Specific type parameter, e.g., int N
    tpTemplate  # Template template parameter

  # Represents a single parameter in a template declaration (e.g., typename T, int N)
  TemplateParameter* = ref object of CppAstNode
    kind*: TemplateParameterKind # Use the named enum
    name*: Identifier # Name of the parameter (e.g., T, N), can be nil
    paramType*: TypeExpr # Type for non-type parameters (e.g., int for N), can be nil
    defaultValue*: Expression # Optional default value for non-type params (e.g., = 10), can be nil
    defaultType*: TypeExpr # Optional default type for type params (e.g., = int), can be nil
    templateParams*: seq[TemplateParameter] # Parameters for template template parameters (e.g., the <typename U> in template<typename U> class C), nil otherwise. Handling is part of parser/transformer.

  # Represents a template declaration wrapping another declaration (class, function, etc.)
  TemplateDecl* = ref object of Statement # Or a distinct top-level node kind? Using Statement for now.
    parameters*: seq[TemplateParameter] # The template parameters <...>
    declaration*: Statement # The actual declaration being templated (e.g., ClassDefinition, FunctionDefinition)

  # Represents a namespace definition (namespace name? { ... })
  NamespaceDef* = ref object of Statement # Treat as a statement/declaration for now
    name*: Identifier # Optional name (nil for anonymous namespaces)
    declarations*: seq[Statement] # Declarations within the namespace

  # Represents the root of the AST for a translation unit (a .cpp/.h file)
  TranslationUnit* = ref object of CppAstNode
    declarations*: seq[Statement] # Top-level declarations (functions, classes, global vars)

  # Represents a single enumerator within an enum (e.g., RED = 1)
  Enumerator* = ref object of CppAstNode
    name*: Identifier
    value*: Expression # Optional assigned value, can be nil

  # Represents an enum or enum class definition
  EnumDefinition* = ref object of Statement # Treat as a statement/declaration for now
    name*: Identifier # Optional name (can be anonymous)
    isClass*: bool    # True for 'enum class', false for 'enum'
    baseType*: TypeExpr # Optional underlying type (e.g., : int), can be nil
    enumerators*: seq[Enumerator]

  # Represents a type alias declaration (typedef int MyInt; using MyFloat = float;)
  TypeAliasDeclaration* = ref object of Statement # Treat as a statement/declaration
    name*: Identifier     # The new type name (MyInt, MyFloat)
    aliasedType*: TypeExpr # The original type being aliased (int, float)
    # Note: Differentiating between typedef and using might require a flag or be implicit

  # --- Complex Type Nodes ---

  # Represents an array type (e.g., int[10], char[][5])
  ArrayTypeExpr* = ref object of TypeExpr
    elementType*: TypeExpr # The type of the elements
    size*: Expression     # Optional size expression (e.g., 10), can be nil for []

  # Enum for reference qualifiers on member functions
  ReferenceQualifier* = enum
    rqNone, # No qualifier
    rqLValue, # & qualifier
    rqRValue  # && qualifier

  # Represents a function type (e.g., int(float, bool)) - Used for function pointers/references
  FunctionTypeExpr* = ref object of TypeExpr
    returnType*: TypeExpr
    parameters*: seq[Parameter] # Reuse Parameter node for parameter types/names
    isConst*: bool             # Member function const qualifier
    isVolatile*: bool          # Member function volatile qualifier
    refQualifier*: ReferenceQualifier # Member function ref-qualifier (& or &&)
    isNoexcept*: bool          # Function exception specification (noexcept)

  # --- Exception Handling Nodes ---

  # Represents a throw expression (e.g., throw std::runtime_error("Error");)
  # Note: 'throw;' without an expression is also valid within a catch block.
  ThrowExpression* = ref object of Expression # Can also be used as a statement
    expression*: Expression # Optional, can be nil for rethrow ('throw;')

  # Represents a single catch clause (catch (...) block)
  CatchClause* = ref object of CppAstNode # Not a Statement itself, part of TryStatement
    exceptionDecl*: VariableDeclaration # Declaration of the caught exception (e.g., const std::exception& e), can be nil for catch(...)
    isCatchAll*: bool # True if this is catch(...)
    body*: BlockStatement # The block executed for this catch

  # Represents a try-catch block
  TryStatement* = ref object of Statement
    tryBlock*: BlockStatement # The main try block
    catchClauses*: seq[CatchClause] # List of associated catch clauses

  # Represents a C++ lambda expression
  LambdaExpression* = ref object of Expression
    captureDefault*: Option[TokenType] # tkAssign for [=], tkBitwiseAnd for [&], none for []
    captures*: seq[Identifier]       # Explicit captures like [var1, &var2] (Simplified for now)
    parameters*: seq[Parameter]       # Lambda parameters
    returnType*: TypeExpr             # Optional explicit return type (nil if deduced)
    body*: BlockStatement             # Lambda body
    # TODO: mutable, exception specifiers, attributes, template parameters for generic lambdas

  # Represents a range-based for loop (for (declaration : range_expr) body)
  RangeForStatement* = ref object of Statement
    declaration*: VariableDeclaration # The loop variable declaration (e.g., auto x, const auto& x)
    rangeExpr*: Expression          # The expression being iterated over (e.g., container, initializer_list)
    body*: Statement                # The loop body

  # Represents a decltype(...) type specifier
  DecltypeSpecifier* = ref object of TypeExpr
    expression*: Expression # The expression inside decltype(...)

# Helper functions for creating nodes (optional but can be useful)
proc newIdentifier*(name: string, line, col: int): Identifier =
  Identifier(name: name, line: line, col: col)

# ... add more helper functions as needed ...
