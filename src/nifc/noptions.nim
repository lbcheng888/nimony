import std/[tables]

type
  Backend* = enum
    backendInvalid = "" # for parseEnum
    backendC = "c"
    backendCpp = "cpp"
    backendJs = "js"

  Option* = enum
    optLineDir

  OptimizeLevel* = enum
    None, Speed, Size

  JsOptimizeLevel* = enum
    jsOptNone, jsOptSimple, jsOptAdvanced, jsOptAggressive
    
  SystemCC* = enum
    ccNone, ccGcc, ccCLang

  Action* = enum
    atNone, atC, atCpp, atJs, atNative

  NpmPackageMode* = enum
    npmNone,       # Don't generate NPM package
    npmFlat,       # All files in root directory
    npmSrc,        # Source in src/ directory
    npmDist,       # Output in dist/ directory
    npmSrcDist     # Source in src/, output in dist/
    
  BundlerMode* = enum
    bundlerNone,     # Don't generate bundler configuration
    bundlerWebpack,  # Generate webpack configuration
    bundlerRollup,   # Generate rollup configuration
    bundlerEsbuild,  # Generate esbuild configuration
    bundlerParcel    # Generate parcel configuration
    
  ReactMode* = enum
    reactNone,      # Don't generate React.js configuration
    reactStandard,  # Generate standard React.js configuration
    reactNextJs     # Generate Next.js configuration
    
  ConfigRef* {.acyclic.} = ref object ## every global configuration
    cCompiler*: SystemCC
    backend*: Backend
    options*: set[Option]
    optimizeLevel*: OptimizeLevel
    jsOptimizeLevel*: JsOptimizeLevel
    nifcacheDir*: string
    outputFile*: string
    useTypescript*: bool
    jsSourceMap*: bool     # Generate source maps for JavaScript
    npmPackage*: NpmPackageMode  # NPM package generation mode
    npmName*: string        # NPM package name
    npmVersion*: string     # NPM package version
    npmDescription*: string # NPM package description
    npmAuthor*: string      # NPM package author
    npmLicense*: string     # NPM package license
    bundlerMode*: BundlerMode  # Bundler configuration mode
    bundlerEntry*: string    # Bundler entry file
    bundlerOutput*: string   # Bundler output path
    bundlerCss*: bool        # Enable CSS support
    bundlerCssModules*: bool # Enable CSS modules
    bundlerAssets*: bool     # Enable asset support
    bundlerDevServer*: bool  # Configure dev server
    bundlerPlugins*: seq[string]  # Bundler plugins
    reactMode*: ReactMode    # React.js/Next.js mode
    reactCssInJs*: bool      # Use CSS-in-JS (styled-components)
    reactStateManagement*: string # State management library (redux, mobx, etc.)
    reactRouting*: bool      # Enable routing
    reactTesting*: bool      # Add testing tools
    reactI18n*: bool         # Enable internationalization
    reactStorybook*: bool    # Add Storybook
    reactComponents*: seq[string] # Components to generate
    reactPages*: seq[string] # Pages to generate (Next.js)
    reactApi*: seq[string]   # API routes to generate (Next.js)

  State* = object
    selects*: seq[string] # names of modules with functions with selectany pragmas
    config*: ConfigRef
    bits*: int

  ActionTable* = OrderedTable[Action, seq[string]]

proc initActionTable*(): ActionTable {.inline.} =
  result = initOrderedTable[Action, seq[string]]()

template getoptimizeLevelFlag*(config: ConfigRef): string =
  case config.optimizeLevel
  of Speed:
    "-O3"
  of Size:
    "-Os"
  of None:
    ""

template getCompilerConfig*(config: ConfigRef): (string, string) =
  case config.cCompiler
  of ccGcc:
    ("gcc", "g++")
  of ccCLang:
    ("clang", "clang++")
  else:
    quit "unreachable"

const ExtAction*: array[Action, string] = ["", ".c", ".cpp", ".js", ".S"]

