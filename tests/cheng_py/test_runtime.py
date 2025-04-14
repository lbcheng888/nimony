# tests/cheng_py/test_runtime.py

import pytest

# --- Import Runtime Components ---
from cheng_py.runtime.runtime import (
    RuntimeEnvironment, RuntimeValue, MemoryState
)

# --- Import AST Nodes needed for values ---
from cheng_py.core.ast import (
    Integer, Float, Boolean, String, Symbol, Nil, Pair # Add others as needed
)

# --- Basic Tests ---

def test_runtime_define_get():
    """Test basic variable definition and retrieval."""
    env = RuntimeEnvironment()
    val10 = Integer(10)
    env.define("x", val10)

    rt_val_x = env.get_runtime_value("x")
    assert isinstance(rt_val_x, RuntimeValue)
    assert rt_val_x.value == val10
    assert rt_val_x.state == MemoryState.VALID

    retrieved_val = env.get_value("x")
    assert retrieved_val == val10

    # Test getting non-existent variable
    with pytest.raises(NameError):
        env.get_value("y")
    with pytest.raises(NameError):
        env.get_runtime_value("y")

def test_runtime_set_value():
    """Test setting the value of an existing variable."""
    env = RuntimeEnvironment()
    val10 = Integer(10)
    val20 = Integer(20)
    env.define("x", val10)

    env.set_value("x", val20)
    rt_val_x = env.get_runtime_value("x")
    assert rt_val_x.value == val20
    assert rt_val_x.state == MemoryState.VALID

    # Test setting non-existent variable (should fail)
    with pytest.raises(NameError):
        env.set_value("y", Integer(5))

# --- Scope Tests ---

def test_runtime_nested_scopes_lookup():
    """Test variable lookup across nested scopes."""
    global_env = RuntimeEnvironment()
    global_env.define("g", String("global"))

    outer_env = RuntimeEnvironment(outer=global_env)
    outer_env.define("o", String("outer"))

    inner_env = RuntimeEnvironment(outer=outer_env)
    inner_env.define("i", String("inner"))

    # Lookup from inner scope
    assert inner_env.get_value("i") == String("inner")
    assert inner_env.get_value("o") == String("outer")
    assert inner_env.get_value("g") == String("global")

    # Lookup from outer scope
    assert outer_env.get_value("o") == String("outer")
    assert outer_env.get_value("g") == String("global")
    with pytest.raises(NameError):
        outer_env.get_value("i") # Cannot see inner scope

    # Lookup from global scope
    assert global_env.get_value("g") == String("global")
    with pytest.raises(NameError):
        global_env.get_value("o")
    with pytest.raises(NameError):
        global_env.get_value("i")

def test_runtime_shadowing():
    """Test variable shadowing in nested scopes."""
    env1 = RuntimeEnvironment()
    env1.define("x", Integer(1))

    env2 = RuntimeEnvironment(outer=env1)
    env2.define("x", Integer(2)) # Shadow global x

    env3 = RuntimeEnvironment(outer=env2)
    env3.define("x", Integer(3)) # Shadow outer x

    assert env3.get_value("x") == Integer(3)
    assert env2.get_value("x") == Integer(2)
    assert env1.get_value("x") == Integer(1)

def test_runtime_scope_exit_drop():
    """Test that variables defined in a scope are dropped when exiting."""
    env1 = RuntimeEnvironment()
    env1.define("a", Integer(1)) # Global

    env2 = RuntimeEnvironment(outer=env1)
    env2.define("b", Integer(2)) # Outer local
    env2.define("c", String("hello")) # Outer local (non-copyable potentially)

    env3 = RuntimeEnvironment(outer=env2)
    env3.define("d", Boolean(True)) # Inner local
    env3.define("b", Integer(3)) # Shadow outer b

    # Check values before exit
    assert env3.get_value("a") == Integer(1)
    assert env3.get_value("b") == Integer(3) # Inner b
    assert env3.get_value("c") == String("hello")
    assert env3.get_value("d") == Boolean(True)

    # Exit inner scope
    env3.exit_scope()

    # Check values in outer scope after inner exit
    assert env2.get_value("a") == Integer(1)
    assert env2.get_value("b") == Integer(2) # Outer b is back
    assert env2.get_value("c") == String("hello")
    with pytest.raises(NameError):
        env2.get_value("d") # d should be dropped

    # Check runtime state of dropped variables (should be MOVED)
    # Need to access the env where it *was* defined if we want to check state after drop
    # This is tricky. Let's assume drop removes it for now.
    # If we kept the entry but marked MOVED, we could check.

    # Exit outer scope
    env2.exit_scope()

    # Check values in global scope
    assert env1.get_value("a") == Integer(1)
    with pytest.raises(NameError):
        env1.get_value("b")
    with pytest.raises(NameError):
        env1.get_value("c")

# --- State Transition Tests (Initial) ---

def test_runtime_value_initial_state():
    """Test RuntimeValue initial state."""
    rv = RuntimeValue(Integer(5))
    assert rv.state == MemoryState.VALID
    assert rv.is_valid()
    assert rv.borrow_count == 0

# --- Assignment (Move vs Copy) Tests ---

def test_runtime_assign_copy():
    """Test assignment of copyable types."""
    env = RuntimeEnvironment()
    val_int = Integer(10)
    val_bool = Boolean(True)

    # Define source variables
    env.define("src_int", val_int)
    env.define("src_bool", val_bool)

    rt_src_int = env.get_runtime_value("src_int")
    rt_src_bool = env.get_runtime_value("src_bool")

    # Assign to new variables (copy)
    env.assign_value("dest_int", rt_src_int, is_copyable=True)
    env.assign_value("dest_bool", rt_src_bool, is_copyable=True)

    # Check destination values and states
    rt_dest_int = env.get_runtime_value("dest_int")
    rt_dest_bool = env.get_runtime_value("dest_bool")
    assert rt_dest_int.value == val_int
    assert rt_dest_int.state == MemoryState.VALID
    assert rt_dest_bool.value == val_bool
    assert rt_dest_bool.state == MemoryState.VALID

    # Check source values and states (should remain valid after copy)
    assert rt_src_int.state == MemoryState.VALID
    assert rt_src_int.value == val_int # Value should still be there
    assert rt_src_bool.state == MemoryState.VALID
    assert rt_src_bool.value == val_bool

    # Assign to existing variable (copy)
    env.assign_value("dest_int", rt_src_bool, is_copyable=True) # Assign bool to int var
    rt_dest_int_new = env.get_runtime_value("dest_int")
    assert rt_dest_int_new.value == val_bool
    assert rt_dest_int_new.state == MemoryState.VALID
    assert rt_src_bool.state == MemoryState.VALID # Source still valid

def test_runtime_assign_move():
    """Test assignment of non-copyable types (move semantics)."""
    env = RuntimeEnvironment()
    # Assume Pair is not copyable for this test
    val_pair = Pair(Integer(1), Nil)
    env.define("src_pair", val_pair)

    rt_src_pair = env.get_runtime_value("src_pair")

    # Assign to new variable (move)
    env.assign_value("dest_pair", rt_src_pair, is_copyable=False)

    # Check destination value and state
    rt_dest_pair = env.get_runtime_value("dest_pair")
    assert rt_dest_pair.value == val_pair # Value moved
    assert rt_dest_pair.state == MemoryState.VALID

    # Check source value and state (should be MOVED)
    assert rt_src_pair.state == MemoryState.MOVED
    assert not rt_src_pair.is_valid()
    # Accessing moved value should raise error
    with pytest.raises(ValueError, match="Attempted to access moved variable"):
        env.get_value("src_pair")

    # Assign another non-copyable (move, overwriting dest_pair)
    val_pair2 = Pair(Integer(2), Nil)
    env.define("src_pair2", val_pair2)
    rt_src_pair2 = env.get_runtime_value("src_pair2")
    env.assign_value("dest_pair", rt_src_pair2, is_copyable=False)

    # Check new destination value
    rt_dest_pair_new = env.get_runtime_value("dest_pair")
    assert rt_dest_pair_new.value == val_pair2
    assert rt_dest_pair_new.state == MemoryState.VALID

    # Check that original dest_pair value was dropped (implicitly marked MOVED by assign_value)
    # We can't easily check the *old* rt_dest_pair object's state directly after overwrite.
    # The drop simulation happens within assign_value.

    # Check that src_pair2 was moved
    assert rt_src_pair2.state == MemoryState.MOVED

# --- Borrowing Tests ---

def test_runtime_borrow_return():
    """Test borrowing and returning a value."""
    env = RuntimeEnvironment()
    val_str = String("mutable string")
    env.define("s", val_str)
    rt_s = env.get_runtime_value("s")

    # Borrow the value
    env.borrow_value("s")
    assert rt_s.state == MemoryState.BORROWED
    assert rt_s.borrow_count == 1
    assert rt_s.is_valid() # Still valid while borrowed

    # Attempting to get value while borrowed (should be allowed by runtime, checked by type checker)
    assert env.get_value("s") == val_str

    # Return the borrow
    env.return_borrowed_value("s")
    assert rt_s.state == MemoryState.VALID
    assert rt_s.borrow_count == 0

def test_runtime_borrow_nested_scopes():
    """Test borrowing across scopes and returning."""
    env1 = RuntimeEnvironment()
    val = Integer(100)
    env1.define("x", val)

    env2 = RuntimeEnvironment(outer=env1)
    rt_x_env1 = env1.get_runtime_value("x") # Get RT value from defining env

    # Borrow from inner scope
    env2.borrow_value("x")
    assert rt_x_env1.state == MemoryState.BORROWED

    # Return borrow from inner scope (should work)
    env2.return_borrowed_value("x")
    assert rt_x_env1.state == MemoryState.VALID

# --- Error Condition Tests ---

def test_runtime_error_access_moved():
    """Test accessing a moved variable raises ValueError."""
    env = RuntimeEnvironment()
    val_pair = Pair(Integer(1), Nil)
    env.define("p", val_pair)
    rt_p = env.get_runtime_value("p")
    env.assign_value("p2", rt_p, is_copyable=False) # Move p to p2
    assert rt_p.state == MemoryState.MOVED
    with pytest.raises(ValueError, match="Attempted to access moved variable 'p'"):
        env.get_value("p")

def test_runtime_error_assign_borrowed():
    """Test assigning to a borrowed variable raises RuntimeError."""
    env = RuntimeEnvironment()
    val = Integer(5)
    env.define("x", val)
    env.borrow_value("x") # Borrow x
    with pytest.raises(RuntimeError, match="Cannot assign to variable 'x' while it is borrowed"):
        env.set_value("x", Integer(10))
    # Also test assign_value
    env.define("y", Integer(20))
    rt_y = env.get_runtime_value("y")
    with pytest.raises(RuntimeError, match="Cannot assign to variable 'x' while it is borrowed"):
        env.assign_value("x", rt_y, is_copyable=True)

def test_runtime_error_assign_from_borrowed():
    """Test assigning from a borrowed variable raises RuntimeError."""
    env = RuntimeEnvironment()
    val = Integer(5)
    env.define("x", val)
    rt_x = env.get_runtime_value("x")
    env.borrow_value("x") # Borrow x
    with pytest.raises(RuntimeError, match="Cannot assign from borrowed value"):
        env.assign_value("y", rt_x, is_copyable=True) # Try to copy from borrowed
    with pytest.raises(RuntimeError, match="Cannot assign from borrowed value"):
        env.assign_value("z", rt_x, is_copyable=False) # Try to move from borrowed

def test_runtime_error_borrow_moved():
    """Test borrowing a moved variable raises ValueError."""
    env = RuntimeEnvironment()
    val_pair = Pair(Integer(1), Nil)
    env.define("p", val_pair)
    rt_p = env.get_runtime_value("p")
    env.assign_value("p2", rt_p, is_copyable=False) # Move p to p2
    assert rt_p.state == MemoryState.MOVED
    with pytest.raises(ValueError, match="Attempted to borrow moved variable 'p'"):
        env.borrow_value("p")

def test_runtime_error_move_borrowed():
    """Test moving a borrowed variable raises RuntimeError."""
    env = RuntimeEnvironment()
    val_pair = Pair(Integer(1), Nil)
    env.define("p", val_pair)
    rt_p = env.get_runtime_value("p")
    env.borrow_value("p") # Borrow p
    assert rt_p.state == MemoryState.BORROWED
    # Test move via assign_value
    with pytest.raises(RuntimeError, match="Cannot assign from borrowed value"):
         env.assign_value("p2", rt_p, is_copyable=False) # Try to move p while borrowed
    # Test move via move_value
    with pytest.raises(RuntimeError, match="Cannot move variable 'p' while it is borrowed"):
         env.move_value("p")

def test_runtime_error_double_borrow():
    """Test attempting to borrow an already mutably borrowed variable."""
    env = RuntimeEnvironment()
    val = Integer(10)
    env.define("x", val)
    env.borrow_value("x") # First borrow
    with pytest.raises(RuntimeError, match="Cannot borrow mutably while already borrowed"):
        env.borrow_value("x") # Second borrow should fail

def test_runtime_error_return_unborrowed():
    """Test returning a borrow that wasn't taken."""
    env = RuntimeEnvironment()
    val = Integer(10)
    env.define("x", val)
    with pytest.raises(RuntimeError, match="Cannot return borrow for value not mutably borrowed"):
        env.return_borrowed_value("x")
