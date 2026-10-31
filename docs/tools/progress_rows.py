"""The progress files' status rule, shared by the progress tools (progress_stats.py, progress_index_regen.py,
progress_score.py).

A row's status is the second cell of a table row:

* ``done``: as the original, checked against it;
* ``chosen``: done the way the user chose, a dated chosen difference (counts as done);
* ``partial NN%``: part of it is done, NN of 100 (counts that share); a bare ``partial`` has not been measured yet and
  counts 0 until it is;
* ``inferred``: written from a guess or an inference, not checked against the original (counts 0, shown apart);
* ``pending``: not done;
* ``device``: the original's behaviour that only a force-feedback device reaches (the original's IMMERSION_EXISTS gate
  answers no without one, as openblack's does), counted apart and in no score (user, 2026-10-27).

The older words are still read: ``todo`` as pending, and ``n/a`` (behaviour the original does not have, rows removed
since 2026-10-21), which is counted apart and in no score.

The score is (done + chosen + the partial rows' shares) / (done + chosen + partial + inferred + pending).
"""

from __future__ import annotations

import re

STATUSES = ("done", "chosen", "partial", "inferred", "pending")
LEGACY = ("n/a",)
# Counted apart and in no score: what only a force-feedback device reaches (user, 2026-10-27)
DEVICE = ("device",)
# The older word for pending, which branches written before 2026-10-19 still use; it is read the same.
ALIASES = {"todo": "pending"}
PARTIAL_RX = re.compile(r"partial(?:\s*\(?\s*(\d{1,3})\s*%\s*\)?)?")
# The keys of a count: the statuses, the partial rows with a measured share and the sum of their shares
KEYS = (*STATUSES, *LEGACY, *DEVICE, "partialMeasured", "partialDone")


def parse(cell: str) -> tuple[str, float | None] | None:
    """A status cell as (status, share): the share is a partial row's measured fraction, None when not measured or
    not partial; None for a cell that is no status."""
    s = cell.strip().lower()
    s = ALIASES.get(s, s)
    if s in STATUSES or s in LEGACY or s in DEVICE:
        return s, None
    m = PARTIAL_RX.fullmatch(s)
    if m:
        return "partial", (min(int(m.group(1)), 100) / 100 if m.group(1) else None)
    return None


def empty() -> dict:
    c = dict.fromkeys(KEYS, 0)
    c["partialDone"] = 0.0
    return c


def add(c: dict, status: str, share: float | None) -> None:
    c[status] += 1
    if status == "partial" and share is not None:
        c["partialMeasured"] += 1
        c["partialDone"] += share


def merge(into: dict, other: dict) -> None:
    for k in KEYS:
        into[k] += other.get(k, 0)


def counted(c: dict) -> int:
    """The rows a score is taken over: every status but the legacy n/a and the device-only rows."""
    return sum(c[k] for k in STATUSES)


def done_share(c: dict) -> float:
    """What the rows count for: done and chosen as one each, a partial row as its measured share."""
    return c["done"] + c["chosen"] + c["partialDone"]


def score(c: dict) -> int:
    n = counted(c)
    return round(100 * done_share(c) / n) if n else 0
