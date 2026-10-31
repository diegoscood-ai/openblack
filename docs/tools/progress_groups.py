"""How the dashboard groups the progress rows (progress_stats.py reads it); no row's status changes here, only where
it is counted. The rules are written down in wiki/project/progress/README.md#how-the-dashboard-groups-the-rows.

* Online, parked (user, 2026-10-24: online waits until the game works offline): the files and sections of ONLINE are
  counted on a line of their own, outside every score. Firestorm, a copy of one of the game's online maps
  (Online Maps/mpm_3p_2.map), is parked with them: its page and every section of the playground scripts' page whose
  heading starts "Firestorm:" (2026-10-26).
* Save and load: the whole of SAVE_FILES, and any other row whose behaviour cell is about the saved game (SAVE_RX),
  leave their domain for the Save and load family, in one of three halves by their own words (save_half).
* The story by land: each row of the story domain goes to a land (story_land) and a kind (story_kind), gold scroll,
  silver scroll or other, from the indexes gold_scrolls.md and silver_scrolls.md and the section headings.
* Done with a test: a done row whose notes name a test of test/ (tested).
* Done in play: a done row whose notes cite a played or real-input run's log, refs/story/played/ or refs/realinput/
  (played).
"""

from __future__ import annotations

import re
from pathlib import Path

# ---------- online, parked ----------

# Files wholly online (network play, its rules and ranking, the online extras, the online map Firestorm), the online
# sections of a file that also has offline ones, and the heading prefixes that mark a run of online sections.
ONLINE_FILES = {"multiplayer/network_play.md", "multiplayer/multiplayer_rules.md", "multiplayer/online_services.md",
                "multiplayer/maps/firestorm.md"}
ONLINE_SECTIONS = {"pc_integration/online_services.md": {"Connecting", "Uploading the creature",
                                                         "Creatures in online games", "Maps from the internet"}}
ONLINE_SECTION_PREFIXES = {"scripts/playground_scripts.md": ("Firestorm:",)}


def is_online(rel: str, heading: str | None) -> bool:
    """A row of a parked online file, or of a parked section, named or by its heading's prefix (a file read from
    INDEX only, with no heading, is online only when the whole file is)."""
    if rel in ONLINE_FILES:
        return True
    if heading is None:
        return False
    return heading in ONLINE_SECTIONS.get(rel, ()) or heading.startswith(ONLINE_SECTION_PREFIXES.get(rel, ()))


# ---------- save and load ----------

# The file that is all about saving and loading games.
SAVE_FILES = {"engine/saving_and_loading.md"}
# A behaviour cell about the saved game: what is saved and loaded with it, kept in it, a saved game itself, the quick
# saves and the automatic saves.
SAVE_RX = re.compile(
    r"\bsaved games?\b|\bsaved and loaded\b|\bsaved with (?:the|a) game\b|\bkept (?:in|with) (?:a |the )?save(?:d game)?\b"
    r"|\bsaves? the game\b|\bsave game in\b|\bquick[- ]?(?:save|saves|saved|load|loads)\b|\bauto(?:matic )?saves?\b"
    r"|\b(?:in|from|into|beside) (?:a|the) save\b|\bloading that save\b|^a save\b", re.I)
# The two halves, by the row's own words: reading a save back, or writing one. A row that names both, or keeps a thing
# "in a saved game" or "with the game" (saved and loaded alike), or names neither (what a save holds), is one row for
# both halves.
BOTH_RX = re.compile(r"\bsaved and loaded\b|\bsaved with (?:the|a) game\b|\bkept (?:in|with) (?:a |the )?save"
                     r"|\bsaved game keeps\b|\bin saved games\b|\bsaved games? (?:keeps?|stores?)\b", re.I)
LOAD_RX = re.compile(r"\b(?:re)?load(?:s|ed|ing)?\b|\bread back\b|\brestor(?:e|es|ed|ing)\b|\bputs? (?:\w+ )?back\b", re.I)
# a save being written, not "a save" or "the save" named as a thing
SAVE_VERB_RX = re.compile(r"(?<!\ba )(?<!\bthe )(?<!\beach )\bsav(?:e|es|ing)\b(?! game\b)|\b(?:is|are) saved\b"
                          r"|\bsaved (?:into|in)\b|\bwrit(?:e|es|ten|ing)\b|\bautosaves?\b", re.I)
HALVES = ("loading", "saving", "both")


def is_save(rel: str, behaviour: str) -> bool:
    return rel in SAVE_FILES or bool(SAVE_RX.search(behaviour))


def save_half(behaviour: str) -> str:
    """loading, saving or both, from the behaviour cell."""
    if BOTH_RX.search(behaviour):
        return "both"
    load, save = bool(LOAD_RX.search(behaviour)), bool(SAVE_VERB_RX.search(behaviour))
    return "loading" if load and not save else "saving" if save and not load else "both"


# ---------- the story by land ----------

LANDS = ("Land 1", "Land 2", "Land 3", "Land 4", "Land 5", "Tutorial island", "Across the lands")
ACROSS = "Across the lands"
# A story page that is in neither index, whose own first lines say its land ("the opening of the first land").
STORY_FILE_LANDS = {"story/tutorial.md": "Land 1"}
LAND_HEADING = re.compile(r"^## (Land [1-5]|Tutorial island)\s*$")
LINK = re.compile(r"\]\(([^)#\s]+\.md)")
LANDS_NAMED = re.compile(r"\blands? ([1-5](?:(?:, | and | or )[1-5])*)", re.I)


def scroll_index(path: Path) -> dict[str, str]:
    """A scroll index's files (relative to progress/), each with the one land whose section lists it; a file listed
    under more than one land is left out."""
    seen: dict[str, set[str]] = {}
    if not path.is_file():
        return {}
    land = None
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("## "):
            m = LAND_HEADING.match(line)
            land = m.group(1) if m else None
            continue
        if land and line.startswith("|"):
            for target in LINK.findall(line):
                if not target.startswith(".."):
                    seen.setdefault(f"story/{target}", set()).add(land)
    return {f: next(iter(lands)) for f, lands in seen.items() if len(lands) == 1}


class StoryLands:
    """Which land and which kind each story row is counted under."""

    def __init__(self, progress: Path):
        self.gold = scroll_index(progress / "story" / "gold_scrolls.md")
        self.silver = scroll_index(progress / "story" / "silver_scrolls.md")

    def land(self, rel: str, heading: str | None) -> str:
        """The land a row's file is listed under (or a land file, land_N.md); else the one land its section heading
        names; else the page's own land; else across the lands."""
        if rel in self.gold or rel in self.silver:
            return self.gold.get(rel) or self.silver[rel]
        m = re.fullmatch(r"story/land_([1-5])\.md", rel)
        if m:
            return f"Land {m.group(1)}"
        if heading:
            if re.search(r"\btutorial island\b", heading, re.I):
                return "Tutorial island"
            named = {d for m in LANDS_NAMED.finditer(heading) for d in re.findall(r"[1-5]", m.group(1))}
            if len(named) == 1:
                return f"Land {named.pop()}"
        return STORY_FILE_LANDS.get(rel, ACROSS)

    def kind(self, rel: str) -> str:
        """gold (the story's quests), silver (the optional ones) or other (the land files and the shared pages)."""
        if rel.startswith("story/gold_scrolls/") or rel in self.gold:
            return "gold"
        if rel.startswith("story/silver_scrolls/") or rel in self.silver:
            return "silver"
        return "other"


# ---------- done with a test ----------

TEST_MACRO = re.compile(r"^\s*(?:TEST|TEST_F|TEST_P|TYPED_TEST|TYPED_TEST_P)\(\s*(\w+)\s*,", re.M)
# "test `X`", "tests: `X`", a test source (test/..., test_*.cpp), or a reference to a test named earlier in the file.
TEST_PHRASE = re.compile(r"\btests?\b[^|`]{0,3}`|\btest/\w|\btest_\w+|\b(?:same|these|those|the above) (?:unit )?tests?\b"
                         r"|\btests? (?:as|above|below|there)\b", re.I)
# A test's name in backticks, `Suite.Case` (or `Prefix/Suite.Case/0`, `Suite.*`): a test when Suite is one of test/'s.
TEST_NAME = re.compile(r"`(?:\w+/)?(\w+)\.[\w*]")


def test_suites(tests: Path) -> set[str]:
    """The test suites the gtest sources under test/ define."""
    suites: set[str] = set()
    if tests.is_dir():
        for path in sorted(tests.rglob("*")):
            if path.suffix in (".cpp", ".h") and path.is_file():
                suites |= set(TEST_MACRO.findall(path.read_text(encoding="utf-8", errors="replace")))
    return suites


def tested(notes: str, suites: set[str]) -> bool:
    """Whether a row's notes name a test (a testbed scenario, a scripted demo or a check run is not one)."""
    return bool(TEST_PHRASE.search(notes)) or any(s in suites for s in TEST_NAME.findall(notes))


# A hand-played run's log (refs/story/played/<run>.log) or a promoted real-input run's (refs/realinput/<scenario>.log).
PLAY_EVIDENCE = re.compile(r"refs/(?:story/played|realinput)/")


def played(notes: str) -> bool:
    """Whether a row's notes cite a run played with real input: the user's hand play or a real-input tap run."""
    return bool(PLAY_EVIDENCE.search(notes))
