"""Avoid false positives while enforcing the project's expanded C++ style."""

import importlib.util
from pathlib import Path

import pytest

PATH = Path(__file__).parents[1] / "scripts/check_readability.py"
SPEC = importlib.util.spec_from_file_location("readability", PATH)
readability = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(readability)


@pytest.mark.parametrize(
    "source",
    [
        'const char *Label = "condition ? yes : no"; // x = y = 0;',
        "/* condition ? a : b */\nbool Ready = First == Second;",
    ],
)
def test_literals_comments_and_equality_are_not_control_flow(source):
    assert readability.cpp_ternary_lines(source) == []
    assert not readability.CPP_CHAINED_ASSIGNMENT.search(readability.cpp_code_only(source))


@pytest.mark.parametrize(
    "source",
    [
        "float Left = 0.0F, Right = 0.0F;",
        "const float First = Compute(1, 2), Second = 3;",
    ],
)
def test_multiple_declarations_require_separate_statements(source):
    assert readability.has_multiple_declarations(source)


@pytest.mark.parametrize(
    "source",
    [
        "const float Values[] = {1.0F, 2.0F};",
        "const float Result = Compute(1.0F, 2.0F);",
    ],
)
def test_initializer_and_argument_commas_are_allowed(source):
    assert not readability.has_multiple_declarations(source)


def test_chained_member_assignments_and_ternaries_are_rejected():
    source = "Aircraft->Input = Aircraft->Target = 1.0F;\nreturn enabled ? a : b;"
    assert readability.CPP_CHAINED_ASSIGNMENT.search(readability.cpp_code_only(source))
    assert readability.cpp_ternary_lines(source) == [2]
