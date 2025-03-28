#
#
#           NIFC Compiler
#        (c) Copyright 2024 Andreas Rumpf
#
#    See the file "copying.txt", included in this
#    distribution, for details about the copyright.
#

## NIFC driver program.

import std / [parseopt, strutils, os, osproc, tables, assertions, syncio, hashes, sets, sequtils, options]
import codegen, noptions, mangler

import nifc_model

import "../lib" / [nifreader, nifstreams, nifcursors, bitabs, lineinfos, nifprelude, symparser]
import ".." / models / [nifc_tags]
import "../gear2" / modnames

when defined(windows):
  import bat
else:
  import makefile

when defined(enableAsm):
  import amd64 / genasm

when defined(cpp):
  import cpp / cpp_backend

when defined(js):
  import js / js_backend

const
  Version = "0.2"
  Usage = "NIFC Compiler. Version " & Version & """

  (c) 2024 Andreas Rumpf
Usage:
  nifc [options] [command] [arguments]
Command:
  c|cpp|js|n file.nif [file2.nif]  convert NIF files to C|C++|JavaScript|ASM

Options:
  -r, --run                 run the makefile and the compiled program
  --compileOnly             compile only, do not run the makefile and the compiled program
  --isMain                  mark the file as the main program
  --cc:SYMBOL               specify the C compiler
  --opt:none|speed|size     optimize not at all or for speed|size
  --jsopt:none|simple|advanced|aggressive    JavaScript optimization level
  --sourcemap:on|off        generate source maps for JavaScript
  --lineDir:on|off          generation of #line directive on|off
  --bits:N                  `(i -1)` has N bits; possible values: 64, 32, 16
  --nimcache:PATH           set the path used for generated files
  --typescript:on|off       generate TypeScript instead of JavaScript
  --npm:none|flat|src|dist|src-dist  generate NPM package with specified structure
  --npm-name:NAME           set NPM package name (defaults to project name)
  --npm-version:VERSION     set NPM package version (defaults to 0.1.0)
  --npm-description:DESC    set NPM package description
  --npm-author:AUTHOR       set NPM package author
  --npm-license:LICENSE     set NPM package license (defaults to MIT)
  --bundler:none|webpack|rollup|esbuild|parcel  generate bundler configuration
  --bundler-entry:FILE      set bundler entry file (defaults to src/index.js)
  --bundler-output:PATH     set bundler output path (defaults to dist)
  --bundler-css:on|off      enable CSS support (defaults to on)
  --bundler-css-modules:on|off  enable CSS modules (defaults to off)
  --bundler-assets:on|off   enable asset support (defaults to on)
  --bundler-dev-server:on|off  configure dev server (defaults to off)
  --bundler-plugins:PLUGINS  add bundler plugins (comma-separated list)
  --react:none|react|nextjs  generate React.js or Next.js configuration
  --react-cssinjs:on|off     use CSS-in-JS with styled-components (defaults to on)
  --react-state:LIBRARY      specify state management library (redux, mobx, recoil, zustand)
  --react-routing:on|off     enable routing (defaults to on)
  --react-testing:on|off     add testing tools (defaults to on)
  --react-i18n:on|off        enable internationalization (defaults to off)
  --react-storybook:on|off   add Storybook (defaults to off)
  --react-components:COMPS   components to generate (comma-separated list)
  --react-pages:PAGES        pages to generate for Next.js (comma-separated list)
  --react-api:API            API routes to generate for Next.js (comma-separated list)
  --version                 show the version
  --help                    show this help
"""

proc writeHelp() = quit(Usage, QuitSuccess)
proc writeVersion() = quit(Version & "\n", QuitSuccess)

proc genMakeCmd(config: ConfigRef, makefilePath: string): string =
  when defined(windows):
    result = expandFilename(makefilePath)
  else:
    result = "make -f " & makefilePath

proc generateJsBackend(s: var State; files: seq[string]; flags: set[GenFlag]) =
  if files.len == 0:
    quit "command takes a filename"
  
  s.config.backend = backendJs
  let destExt = if s.config.useTypescript: ".ts" else: ".js"
  
  # Process each file
  for inp in files:
    let moduleName = splitFile(inp).name
    let ast = load(inp)
    
    when defined(js):
      var options = Options(
        projectName: moduleName,
        outDir: s.config.nifcacheDir,
        optimizeLevel: s.config.optimizeLevel
      )
      
      # Set typescript flag
      options["typescript"] = if s.config.useTypescript: "true" else: "false"
      
      # Set JS optimization level
      case s.config.jsOptimizeLevel
      of jsOptNone:
        options["jsoptimize"] = "none"
      of jsOptSimple:
        options["jsoptimize"] = "simple"
      of jsOptAdvanced:
        options["jsoptimize"] = "advanced"
      of jsOptAggressive:
        options["jsoptimize"] = "aggressive"
      
      # Set sourcemap flag
      options["sourcemap"] = if s.config.jsSourceMap: "true" else: "false"
      
      # Set NPM package options
      if s.config.npmPackage != npmNone:
        case s.config.npmPackage
        of npmFlat:
          options["npmpackage"] = "flat"
        of npmSrc:
          options["npmpackage"] = "src"
        of npmDist:
          options["npmpackage"] = "dist"
        of npmSrcDist:
          options["npmpackage"] = "srcdist"
        else:
          discard
        
        # Pass additional NPM options if provided
        if s.config.npmName.len > 0:
          options["npmname"] = s.config.npmName
        else:
          options["npmname"] = moduleName
          
        if s.config.npmVersion.len > 0:
          options["npmversion"] = s.config.npmVersion
        
        if s.config.npmDescription.len > 0:
          options["npmdescription"] = s.config.npmDescription
        
        if s.config.npmAuthor.len > 0:
          options["npmauthor"] = s.config.npmAuthor
        
        if s.config.npmLicense.len > 0:
          options["npmlicense"] = s.config.npmLicense
        
        # 添加bundler相关选项
        if s.config.bundlerMode != bundlerNone:
          case s.config.bundlerMode
          of bundlerWebpack:
            options["bundlertype"] = "webpack"
          of bundlerRollup:
            options["bundlertype"] = "rollup"
          of bundlerEsbuild:
            options["bundlertype"] = "esbuild"
          of bundlerParcel:
            options["bundlertype"] = "parcel"
          else:
            discard
          
          options["bundlerentry"] = s.config.bundlerEntry
          options["bundleroutput"] = s.config.bundlerOutput
          options["bundlercss"] = if s.config.bundlerCss: "true" else: "false"
          options["bundlercssmodules"] = if s.config.bundlerCssModules: "true" else: "false"
          options["bundlerassets"] = if s.config.bundlerAssets: "true" else: "false"
          options["bundlerdevserver"] = if s.config.bundlerDevServer: "true" else: "false"
          
          if s.config.bundlerPlugins.len > 0:
            options["bundlerplugins"] = s.config.bundlerPlugins.join(",")
            
        # 添加React/Next.js相关选项
        if s.config.reactMode != reactNone:
          case s.config.reactMode
          of reactStandard:
            options["reactmode"] = "react"
          of reactNextJs:
            options["reactmode"] = "nextjs"
          else:
            discard
          
          options["reactcssinjs"] = if s.config.reactCssInJs: "true" else: "false"
          options["reactstatemanagement"] = s.config.reactStateManagement
          options["reactrouting"] = if s.config.reactRouting: "true" else: "false"
          options["reacttesting"] = if s.config.reactTesting: "true" else: "false"
          options["reacti18n"] = if s.config.reactI18n: "true" else: "false"
          options["reactstorybook"] = if s.config.reactStorybook: "true" else: "false"
          
          if s.config.reactComponents.len > 0:
            options["reactcomponents"] = s.config.reactComponents.join(",")
            
          if s.config.reactPages.len > 0:
            options["reactpages"] = s.config.reactPages.join(",")
            
          if s.config.reactApi.len > 0:
            options["reactapi"] = s.config.reactApi.join(",")
      
      if gfMainModule in flags and inp == files[^1]:
        options.isMainModule = true
        
      discard jsBackendMain(ast, options)
    else:
      quit "wasn't built with JavaScript backend support"

proc generateBackend(s: var State; action: Action; files: seq[string]; flags: set[GenFlag]) =
  if action == atJs:
    generateJsBackend(s, files, flags)
    return
  
  assert action in {atC, atCpp}
  if files.len == 0:
    quit "command takes a filename"
  
  s.config.backend = if action == atC: backendC else: backendCpp
  let destExt = if action == atC: ".c" else: ".cpp"
  
  # Process non-last files
  for i in 0..<files.len-1:
    let inp = files[i]
    let outp = s.config.nifcacheDir / splitFile(inp).name.mangleFileName & destExt
    
    when defined(cpp) and false: # Temporarily disabled until backend is more stable
      if action == atCpp:
        # Use C++ backend for .cpp files
        let moduleName = splitFile(inp).name
        let ast = load(inp)
        let options = Options(
          projectName: moduleName,
          outDir: s.config.nifcacheDir,
          optimizeLevel: s.config.optimizeLevel
        )
        discard cppBackendMain(ast, options)
      else:
        # Use regular backend for C files
        generateCode s, inp, outp, {}
    else:
      # Fall back to standard code generation
      generateCode s, inp, outp, {}
  
  # Process last file
  let inp = files[^1]
  let outp = s.config.nifcacheDir / splitFile(inp).name.mangleFileName & destExt
  
  when defined(cpp) and false: # Temporarily disabled until backend is more stable
    if action == atCpp:
      # Use C++ backend for .cpp files
      let moduleName = splitFile(inp).name
      let ast = load(inp)
      let options = Options(
        projectName: moduleName,
        outDir: s.config.nifcacheDir,
        optimizeLevel: s.config.optimizeLevel
      )
      if gfMainModule in flags:
        options.isMainModule = true
      discard cppBackendMain(ast, options)
    else:
      # Use regular backend for C files
      generateCode s, inp, outp, flags
  else:
    # Fall back to standard code generation
    generateCode s, inp, outp, flags

proc handleCmdLine() =
  var toRun = false
  var compileOnly = false
  var isMain = false
  var currentAction = atNone

  var actionTable = initActionTable()

  var s = State(config: ConfigRef(), bits: sizeof(int)*8)
  when defined(macos): # TODO: switches to default config for platforms
    s.config.cCompiler = ccCLang
  else:
    s.config.cCompiler = ccGcc
  s.config.nifcacheDir = "nimcache"
  s.config.jsOptimizeLevel = jsOptNone
  s.config.jsSourceMap = false
  s.config.npmPackage = npmNone
  s.config.npmName = ""
  s.config.npmVersion = "0.1.0"
  s.config.npmDescription = "Generated by Nimony JavaScript backend"
  s.config.npmAuthor = ""
  s.config.npmLicense = "MIT"
  s.config.bundlerMode = bundlerNone
  s.config.bundlerEntry = "src/index.js"
  s.config.bundlerOutput = "dist"
  s.config.bundlerCss = true
  s.config.bundlerCssModules = false
  s.config.bundlerAssets = true
  s.config.bundlerDevServer = false
  s.config.bundlerPlugins = @[]
  s.config.reactMode = reactNone
  s.config.reactCssInJs = true
  s.config.reactStateManagement = "zustand"
  s.config.reactRouting = true
  s.config.reactTesting = true
  s.config.reactI18n = false
  s.config.reactStorybook = false
  s.config.reactComponents = @[]
  s.config.reactPages = @[]
  s.config.reactApi = @[]

  for kind, key, val in getopt():
    case kind
    of cmdArgument:
      case key.normalize:
      of "c":
        currentAction = atC
        if not hasKey(actionTable, atC):
          actionTable[atC] = @[]
      of "cpp":
        currentAction = atCpp
        if not hasKey(actionTable, atCpp):
          actionTable[atCpp] = @[]
      of "n":
        currentAction = atNative
        if not hasKey(actionTable, atNative):
          actionTable[atNative] = @[]
      of "js":
        currentAction = atJs
        if not hasKey(actionTable, atJs):
          actionTable[atJs] = @[]
      else:
        case currentAction
        of atC:
          actionTable[atC].add key
        of atCpp:
          actionTable[atCpp].add key
        of atJs:
          actionTable[atJs].add key
        of atNative:
          actionTable[atNative].add key
        of atNone:
          quit "invalid command: " & key
    of cmdLongOption, cmdShortOption:
      case normalize(key)
      of "bits":
        case val
        of "64": s.bits = 64
        of "32": s.bits = 32
        of "16": s.bits = 16
        else: quit "invalid value for --bits"
      of "help", "h": writeHelp()
      of "version", "v": writeVersion()
      of "run", "r": toRun = true
      of "compileonly": compileOnly = true
      of "ismain": isMain = true
      of "cc":
        case val.normalize
        of "gcc":
          s.config.cCompiler = ccGcc
        of "clang":
          s.config.cCompiler = ccCLang
        else:
          quit "unknown C compiler: '$1'. Available options are: gcc, clang" % [val]
      of "opt":
        case val.normalize
        of "speed":
          s.config.optimizeLevel = Speed
        of "size":
          s.config.optimizeLevel = Size
        of "none":
          s.config.optimizeLevel = None
        else:
          quit "'none', 'speed' or 'size' expected, but '$1' found" % val
      of "linedir":
        case val.normalize
        of "", "on":
          s.config.options.incl optLineDir
        of "off":
          s.config.options.excl optLineDir
        else:
          quit "'on', 'off' expected, but '$1' found" % val
      of "nimcache":
        s.config.nifcacheDir = val
      of "typescript":
        case val.normalize
        of "", "on":
          s.config.useTypescript = true
        of "off":
          s.config.useTypescript = false
        else:
          quit "'on', 'off' expected, but '$1' found" % val
      of "jsopt":
        case val.normalize
        of "none":
          s.config.jsOptimizeLevel = jsOptNone
        of "simple":
          s.config.jsOptimizeLevel = jsOptSimple
        of "advanced":
          s.config.jsOptimizeLevel = jsOptAdvanced
        of "aggressive":
          s.config.jsOptimizeLevel = jsOptAggressive
        else:
          quit "'none', 'simple', 'advanced', or 'aggressive' expected, but '$1' found" % val
      of "sourcemap":
        case val.normalize
        of "", "on":
          s.config.jsSourceMap = true
        of "off":
          s.config.jsSourceMap = false
        else:
          quit "'on', 'off' expected, but '$1' found" % val
      of "npm":
        case val.normalize
        of "none":
          s.config.npmPackage = npmNone
        of "flat":
          s.config.npmPackage = npmFlat
        of "src":
          s.config.npmPackage = npmSrc
        of "dist":
          s.config.npmPackage = npmDist
        of "src-dist":
          s.config.npmPackage = npmSrcDist
        else:
          quit "'none', 'flat', 'src', 'dist', or 'src-dist' expected, but '$1' found" % val
      of "npm-name":
        s.config.npmName = val
      of "npm-version":
        s.config.npmVersion = val
      of "npm-description":
        s.config.npmDescription = val
      of "npm-author":
        s.config.npmAuthor = val
      of "npm-license":
        s.config.npmLicense = val
      of "bundler":
        case val.normalize
        of "none":
          s.config.bundlerMode = bundlerNone
        of "webpack":
          s.config.bundlerMode = bundlerWebpack
        of "rollup":
          s.config.bundlerMode = bundlerRollup
        of "esbuild":
          s.config.bundlerMode = bundlerEsbuild
        of "parcel":
          s.config.bundlerMode = bundlerParcel
        else:
          quit "'none', 'webpack', 'rollup', 'esbuild', or 'parcel' expected, but '$1' found" % val
      of "bundler-entry":
        s.config.bundlerEntry = val
      of "bundler-output":
        s.config.bundlerOutput = val
      of "bundler-css":
        case val.normalize
        of "", "on":
          s.config.bundlerCss = true
        of "off":
          s.config.bundlerCss = false
        else:
          quit "'on', 'off' expected, but '$1' found" % val
      of "bundler-css-modules":
        case val.normalize
        of "", "on":
          s.config.bundlerCssModules = true
        of "off":
          s.config.bundlerCssModules = false
        else:
          quit "'on', 'off' expected, but '$1' found" % val
      of "bundler-assets":
        case val.normalize
        of "", "on":
          s.config.bundlerAssets = true
        of "off":
          s.config.bundlerAssets = false
        else:
          quit "'on', 'off' expected, but '$1' found" % val
      of "bundler-dev-server":
        case val.normalize
        of "", "on":
          s.config.bundlerDevServer = true
        of "off":
          s.config.bundlerDevServer = false
        else:
          quit "'on', 'off' expected, but '$1' found" % val
      of "bundler-plugins":
        # 处理逗号分隔的插件列表
        s.config.bundlerPlugins = val.split(',')
      of "react":
        case val.normalize
        of "none":
          s.config.reactMode = reactNone
        of "react":
          s.config.reactMode = reactStandard
        of "nextjs":
          s.config.reactMode = reactNextJs
        else:
          quit "'none', 'react', or 'nextjs' expected, but '$1' found" % val
      of "react-cssinjs":
        case val.normalize
        of "", "on":
          s.config.reactCssInJs = true
        of "off":
          s.config.reactCssInJs = false
        else:
          quit "'on', 'off' expected, but '$1' found" % val
      of "react-state":
        s.config.reactStateManagement = val
      of "react-routing":
        case val.normalize
        of "", "on":
          s.config.reactRouting = true
        of "off":
          s.config.reactRouting = false
        else:
          quit "'on', 'off' expected, but '$1' found" % val
      of "react-testing":
        case val.normalize
        of "", "on":
          s.config.reactTesting = true
        of "off":
          s.config.reactTesting = false
        else:
          quit "'on', 'off' expected, but '$1' found" % val
      of "react-i18n":
        case val.normalize
        of "", "on":
          s.config.reactI18n = true
        of "off":
          s.config.reactI18n = false
        else:
          quit "'on', 'off' expected, but '$1' found" % val
      of "react-storybook":
        case val.normalize
        of "", "on":
          s.config.reactStorybook = true
        of "off":
          s.config.reactStorybook = false
        else:
          quit "'on', 'off' expected, but '$1' found" % val
      of "react-components":
        # 处理逗号分隔的组件列表
        s.config.reactComponents = val.split(',')
      of "react-pages":
        # 处理逗号分隔的页面列表
        s.config.reactPages = val.split(',')
      of "react-api":
        # 处理逗号分隔的API路由列表
        s.config.reactApi = val.split(',')
      of "out", "o":
        s.config.outputFile = val
      else: writeHelp()
    of cmdEnd: assert false, "cannot happen"

  createDir(s.config.nifcacheDir)
  if actionTable.len != 0:
    for action in actionTable.keys:
      case action
      of atC, atCpp:
        let isLast = (if compileOnly: isMain else: currentAction == action)
        var flags = if isLast: {gfMainModule} else: {}
        if isMain:
          flags.incl gfProducesMainProc
        generateBackend(s, action, actionTable[action], flags)
      of atJs:
        var flags: set[GenFlag] = {}
        if isMain:
          flags.incl gfMainModule
          flags.incl gfProducesMainProc
        generateBackend(s, action, actionTable[action], flags)
      of atNative:
        let args = actionTable[action]
        if args.len == 0:
          quit "command takes a filename"
        else:
          when defined(enableAsm):
            for inp in items args:
              let outp = changeFileExt(inp, ".S")
              generateAsm inp, s.config.nifcacheDir / outp
          else:
            quit "wasn't built with native target support"
      of atNone:
        quit "targets are not specified"

    if s.selects.len > 0:
      var h = open(s.config.nifcacheDir / "select_any.h", fmWrite)
      for x in s.selects:
        write h, "#include \"" & extractFilename(x) & "\"\n"
      h.close()
    let appName = actionTable[currentAction][^1].splitFile.name.mangleFileName
    if s.config.outputFile == "":
      s.config.outputFile = appName

    if not compileOnly:
      when defined(windows):
        let makefilePath = s.config.nifcacheDir / "Makefile." & appName & ".bat"
        generateBatMakefile(s, makefilePath, s.config.outputFile, actionTable)
      else:
        let makefilePath = s.config.nifcacheDir / "Makefile." & appName
        generateMakefile(s, makefilePath, s.config.outputFile, actionTable)
      if toRun:
        let makeCmd = genMakeCmd(s.config, makefilePath)
        let (output, exitCode) = execCmdEx(makeCmd)
        if exitCode != 0:
          quit "execution of an external program failed: " & output
        if execCmd("./" & appName) != 0:
          quit "execution of an external program failed: " & appName
  else:
    writeHelp()

when isMainModule:
  handleCmdLine()
