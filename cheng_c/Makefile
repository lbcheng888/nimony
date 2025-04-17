CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude -std=c11 -O0
# Define directories
SRC_DIR = src
TEST_DIR = tests
INCLUDE_DIR = include
BIN_DIR = bin
OBJ_DIR = obj
BUILD_DIR = . # Where generated C files go

# Runtime sources (needed by both stage 0 and stage 1+)
RUNTIME_SRCS = $(SRC_DIR)/l0_arena.c $(SRC_DIR)/l0_types.c $(SRC_DIR)/l0_parser.c \
               $(SRC_DIR)/l0_env.c $(SRC_DIR)/l0_primitives.c $(SRC_DIR)/l0_eval.c \
               $(SRC_DIR)/l0_codegen.c
RUNTIME_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(RUNTIME_SRCS))

# Stage 0 Compiler sources (Handwritten C)
STAGE0_MAIN_SRC = $(SRC_DIR)/main.c
STAGE0_MAIN_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(STAGE0_MAIN_SRC))
STAGE0_EXE = $(BIN_DIR)/cheng_compiler_c.exe

# Stage 1 Compiler (generated C from L0 source compiled by Stage 0)
STAGE1_GEN_C = compiler_stage1_final.c
STAGE1_GEN_OBJ = $(OBJ_DIR)/compiler_stage1_final.o
STAGE1_EXE = $(BIN_DIR)/compiler_stage1_final.exe

# Stage 2 Compiler (generated C from L0 source compiled by Stage 1)
STAGE2_GEN_C = compiler_stage2.c
STAGE2_GEN_OBJ = $(OBJ_DIR)/compiler_stage2.o
STAGE2_EXE = $(BIN_DIR)/compiler_stage2.exe

# Stage 3 Compiler (generated C from L0 source compiled by Stage 2)
STAGE3_GEN_C = compiler_stage3.c
STAGE3_GEN_OBJ = $(OBJ_DIR)/compiler_stage3.o
STAGE3_EXE = $(BIN_DIR)/compiler_stage3.exe

# Stage 6 Compiler (generated C from L0 source compiled by Stage 5)
STAGE6_GEN_C = compiler_stage6.c
STAGE6_GEN_OBJ = $(OBJ_DIR)/compiler_stage6.o
STAGE6_EXE = $(BIN_DIR)/compiler_stage6.exe

# Stage 7 Compiler (macro fix, generated C from L0 source compiled by Stage 6)
STAGE7_MACRO_GEN_C = compiler_stage7_macro.c # Assuming this is the C file generated previously
STAGE7_MACRO_GEN_OBJ = $(OBJ_DIR)/compiler_stage7_macro.o
STAGE7_MACRO_EXE = $(BIN_DIR)/compiler_stage7_macro.exe


# L0 source for the compiler itself
L0_COMPILER_SRC = ../src/l0_compiler/compiler.l0 # Adjusted path relative to cheng_c

# Test sources
TEST_ARENA_SRC = $(TEST_DIR)/test_arena.c
TEST_PARSER_SRC = $(TEST_DIR)/test_parser.c
TEST_EVAL_SRC = $(TEST_DIR)/test_eval.c
TEST_ARENA_EXE = $(BIN_DIR)/test_arena.exe
TEST_PARSER_EXE = $(BIN_DIR)/test_parser.exe
TEST_EVAL_EXE = $(BIN_DIR)/test_eval.exe

# Headers (simplified dependency)
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)

.PHONY: all clean stage0 stage1 stage2 stage3 tests bootstrap_stage1 bootstrap compare prepare_dirs

# Target to ensure directories exist
prepare_dirs:
	@echo "Ensuring directories $(BIN_DIR) and $(OBJ_DIR) exist..."
	-@cmd /c mkdir $(BIN_DIR) 2> nul || ver > nul
	-@cmd /c mkdir $(OBJ_DIR) 2> nul || ver > nul

all: prepare_dirs stage0 stage6 stage7_macro tests # Add stage7_macro to default build and depend on prepare_dirs

stage0: $(STAGE0_EXE)

stage1: $(STAGE1_EXE)

stage2: $(STAGE2_EXE)

stage3: $(STAGE3_EXE) # Added stage3 target

stage6: $(STAGE6_EXE) # Added stage6 target

stage7_macro: $(STAGE7_MACRO_EXE) # Added stage7_macro target

tests: $(TEST_ARENA_EXE) $(TEST_PARSER_EXE) $(TEST_EVAL_EXE)

# Rule to build runtime objects and stage0 main object
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to build Stage 0 compiler executable
$(STAGE0_EXE): $(STAGE0_MAIN_OBJ) $(RUNTIME_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# Rule to build Stage 1 compiler object from generated C
# Depends on the generated C file existing
$(STAGE1_GEN_OBJ): $(STAGE1_GEN_C) $(HEADERS)
	@echo "Compiling generated Stage 1 C code: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to build Stage 1 compiler executable
# Depends on the Stage 1 object file and runtime objects
$(STAGE1_EXE): $(STAGE1_GEN_OBJ) $(RUNTIME_OBJS)
	@echo "Linking Stage 1 executable: $@"
	$(CC) $(CFLAGS) $^ -o $@

# Rule to build Stage 2 compiler object from generated C
# Depends on the generated C file existing
$(STAGE2_GEN_OBJ): $(STAGE2_GEN_C) $(HEADERS)
	@echo "Compiling generated Stage 2 C code: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to build Stage 2 compiler executable
# Depends on the Stage 2 object file and runtime objects
$(STAGE2_EXE): $(STAGE2_GEN_OBJ) $(RUNTIME_OBJS)
	@echo "Linking Stage 2 executable: $@"
	$(CC) $(CFLAGS) $^ -o $@

# Rule to build Stage 3 compiler object from generated C
# Depends on the generated C file existing
$(STAGE3_GEN_OBJ): $(STAGE3_GEN_C) $(HEADERS)
	@echo "Compiling generated Stage 3 C code: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to build Stage 3 compiler executable
# Depends on the Stage 3 object file and runtime objects
$(STAGE3_EXE): $(STAGE3_GEN_OBJ) $(RUNTIME_OBJS)
	@echo "Linking Stage 3 executable: $@"
	$(CC) $(CFLAGS) $^ -o $@

# Rule to build Stage 6 compiler object from generated C
# Depends on the generated C file existing
$(STAGE6_GEN_OBJ): $(STAGE6_GEN_C) $(HEADERS)
	@echo "Compiling generated Stage 6 C code: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to build Stage 6 compiler executable
# Depends on the Stage 6 object file and runtime objects
$(STAGE6_EXE): $(STAGE6_GEN_OBJ) $(RUNTIME_OBJS)
	@echo "Linking Stage 6 executable: $@"
	$(CC) $(CFLAGS) $^ -o $@

# Rule to build Stage 7 (macro fix) compiler object from generated C
# Depends on the generated C file existing
$(STAGE7_MACRO_GEN_OBJ): $(STAGE7_MACRO_GEN_C) $(HEADERS)
	@echo "Compiling generated Stage 7 (macro fix) C code: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to build Stage 7 (macro fix) compiler executable
# Depends on the Stage 7 object file and runtime objects
$(STAGE7_MACRO_EXE): $(STAGE7_MACRO_GEN_OBJ) $(RUNTIME_OBJS)
	@echo "Linking Stage 7 (macro fix) executable: $@"
	$(CC) $(CFLAGS) $^ -o $@


# --- Test Executables ---
$(TEST_ARENA_EXE): $(TEST_ARENA_SRC) $(OBJ_DIR)/l0_arena.o $(HEADERS)
	$(CC) $(CFLAGS) $^ -o $@

$(TEST_PARSER_EXE): $(TEST_PARSER_SRC) $(OBJ_DIR)/l0_arena.o $(OBJ_DIR)/l0_types.o $(OBJ_DIR)/l0_parser.o $(HEADERS)
	$(CC) $(CFLAGS) $^ -o $@

$(TEST_EVAL_EXE): $(TEST_EVAL_SRC) $(RUNTIME_OBJS) $(HEADERS)
	$(CC) $(CFLAGS) $^ -o $@

# --- Bootstrap Steps ---

# Generate Stage 1 C code using Stage 0 compiler
# Depends on the Stage 0 exe and the L0 source
$(STAGE1_GEN_C): $(STAGE0_EXE) $(L0_COMPILER_SRC)
	@echo "Generating Stage 1 C code using Stage 0 compiler..."
	./$(STAGE0_EXE) $(L0_COMPILER_SRC) $@

# Generate Stage 2 C code using Stage 1 compiler
# Depends on the Stage 1 exe and the L0 source
$(STAGE2_GEN_C): $(STAGE1_EXE) $(L0_COMPILER_SRC)
	@echo "Generating Stage 2 C code using Stage 1 compiler..."
	$(BIN_DIR)/compiler_stage1_final.exe $(L0_COMPILER_SRC) $@

# Generate Stage 3 C code using Stage 2 compiler
# Depends on the Stage 2 exe and the L0 source
$(STAGE3_GEN_C): $(STAGE2_EXE) $(L0_COMPILER_SRC)
	@echo "Generating Stage 3 C code using Stage 2 compiler..."
	$(BIN_DIR)/compiler_stage2.exe $(L0_COMPILER_SRC) $@

# Generate Stage 7 (macro fix) C code using Stage 6 compiler
# Depends on the Stage 6 exe and the L0 source
$(STAGE7_MACRO_GEN_C): $(STAGE6_EXE) $(L0_COMPILER_SRC)
	@echo "Generating Stage 7 (macro fix) C code using Stage 6 compiler..."
	$(BIN_DIR)/compiler_stage6.exe $(L0_COMPILER_SRC) $@

# Bootstrap target: Build stage 0, generate stage 1 C, build stage 1 exe
# We need to ensure the generated C file exists before trying to build the object/exe
bootstrap_stage1: $(STAGE1_GEN_C) $(STAGE1_EXE)

# Full bootstrap: Build stage 0, generate stage 1 C, build stage 1 exe, generate stage 2 C, build stage 2 exe
bootstrap: $(STAGE2_EXE)

# Compare stage 2 and stage 3 executables
compare: $(STAGE2_EXE) $(STAGE3_EXE)
	@echo "Comparing Stage 2 and Stage 3 executables..."
	@fc /b $(subst /,\,$(STAGE2_EXE)) $(subst /,\,$(STAGE3_EXE)) > nul && echo "Success: Stage 2 and Stage 3 executables are identical." || (echo "Failure: Executables differ." && exit 1)


clean:
	-cmd /c del /Q /F "$(subst /,\,$(OBJ_DIR))\*.o" "$(subst /,\,$(BIN_DIR))\*" "compiler_stage1_final.c" "compiler_stage2.c" "compiler_stage3.c" "compiler_stage6.c" "compiler_stage7_macro.c" 2> nul || ver > nul
	@echo Cleaned object files, binaries, and generated C files.
