import dataclasses
from typing import Tuple

# --- Basic Type System ---
@dataclasses.dataclass(frozen=True) # Make types immutable
class Type:
    """Base class for all types."""
    name: str

    def __str__(self):
        return self.name

# Singleton instances for common primitive types
VoidType = Type("void") # For functions that don't return a value explicitly
IntType = Type("int") # Placeholder, needs refinement (i32, i64 etc.)
FloatType = Type("float") # Placeholder, needs refinement (f32, f64)
BoolType = Type("bool")
StringType = Type("str") # Represents stack/static strings
AnyType = Type("any") # Represents unknown or polymorphic type during analysis
ErrorType = Type("error") # Represents a type error state

@dataclasses.dataclass(frozen=True)
class FunctionType(Type):
    """Represents the type of a function."""
    param_types: Tuple['Type', ...] # Using tuple for immutability
    return_type: 'Type'
    # name is inherited from Type

    def __str__(self):
        # Note: name is inherited, __str__ uses param/return types
        params = ", ".join(map(str, self.param_types))
        # Use the inherited name for the function itself if needed,
        # but the standard representation focuses on signature.
        # Example: return f"{self.name}: ({params}) -> {self.return_type}"
        return f"({params}) -> {self.return_type}"

# TODO: Add StructType, EnumType, etc. later
