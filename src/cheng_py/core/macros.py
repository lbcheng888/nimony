# src/cheng_py/core/macros.py

from typing import List, TYPE_CHECKING
from .ast import ChengExpr, Symbol, Pair, Nil

# Forward declaration for type hinting
if TYPE_CHECKING:
    from .environment import Environment

class Macro:
    """Represents a user-defined macro."""
    def __init__(self, params: Pair, body: ChengExpr, definition_env: 'Environment'):
        # Similar parameter validation as Closure
        self.params: List[Symbol] = []
        curr = params
        while isinstance(curr, Pair):
            if not isinstance(curr.car, Symbol):
                raise TypeError(f"Macro parameters must be symbols, got {type(curr.car).__name__}")
            self.params.append(curr.car)
            curr = curr.cdr
        if curr is not Nil:
            raise TypeError("Macro parameters must form a proper list")

        self.body = body # Macro body expression (to be evaluated during expansion)
        self.definition_env = definition_env # Environment where the macro was defined

    def __repr__(self):
        param_names = ' '.join(p.value for p in self.params)
        return f"<Macro ({param_names})>"
