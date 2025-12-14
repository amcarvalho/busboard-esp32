import pytest

from main import _truncate_destination


def test_truncate_with_comma():
    assert _truncate_destination("Victoria Station, via Oxford Street") == "Victoria Station"


def test_truncate_without_comma():
    assert _truncate_destination("Wembley Central") == "Wembley Central"


def test_non_string_returns_same():
    assert _truncate_destination(None) is None
    assert _truncate_destination(123) == 123
