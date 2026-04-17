"""Basic pytest tests for vkcube project."""
import pytest


class TestBasicArithmetic:
    """Test basic arithmetic operations."""

    def test_addition(self) -> None:
        """Test that 1 + 1 equals 2."""
        assert 1 + 1 == 2

    def test_subtraction(self) -> None:
        """Test that 5 - 3 equals 2."""
        assert 5 - 3 == 2

    def test_multiplication(self) -> None:
        """Test that 3 * 4 equals 12."""
        assert 3 * 4 == 12

    def test_division(self) -> None:
        """Test that 10 / 2 equals 5."""
        assert 10 / 2 == 5.0


class TestStringOperations:
    """Test basic string operations."""

    def test_string_concatenation(self) -> None:
        """Test string concatenation."""
        result = "Hello" + " " + "World"
        assert result == "Hello World"

    def test_string_length(self) -> None:
        """Test string length."""
        text = "ImRAD"
        assert len(text) == 5

    def test_string_upper(self) -> None:
        """Test string uppercase conversion."""
        text = "simple"
        assert text.upper() == "SIMPLE"


@pytest.mark.parametrize("x,expected", [(1, 2), (2, 4), (3, 6), (5, 10)])
def test_double_value(x: int, expected: int) -> None:
    """Test doubling values with parametrize."""
    assert x * 2 == expected


def test_sample_fixture() -> None:
    """A sample test to demonstrate basic structure."""
    value = 42
    assert value == 42
