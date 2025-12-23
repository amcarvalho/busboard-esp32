import os
import sys
import pytest

# Ensure repository root is on sys.path so `from main import ...` works in CI
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if ROOT not in sys.path:
    sys.path.insert(0, ROOT)

from main import _truncate_destination


def test_truncate_with_comma():
    assert _truncate_destination("Victoria Station, via Oxford Street") == "Victoria Station"


def test_truncate_without_comma():
    assert _truncate_destination("Wembley Central") == "Wembley Central"


def test_non_string_returns_same():
    assert _truncate_destination(None) is None
    assert _truncate_destination(123) == 123
