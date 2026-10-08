# Script debugger

The debug bar's Scripts window and the editor's Log tab: a reader and debugger for the game's compiled challenge
scripts, as they run in openblack's virtual machine. The scripts and their functions are in ../story/ and the virtual
machine in ../engine/.

**Progress: 18/21 done, 1 partial — 88%**

How the original does it, in our wiki: [openblack internals](../../bw1-notes/openblack-internals.md).

## Browsing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lists the loaded program's scripts by kind, searched, with how many tasks run each | done | `src/Editor/Panels/ScriptsPanel.cpp`, `src/Editor/Scripts/ScriptModel.cpp`; tests `EditorScripts.*` |
| Opens another compiled program, or goes back to the game's own | todo | the panel shows the game's own program only (`src/Editor/Panels/ScriptsPanel.cpp`) |
| A script's details, and starting it as the game starts scripts | done | "Script" side tab, "Start a task" |

## Reading code

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The code coloured, every name filled in | done | `ScriptModel` (the "Disassembly" tab) |
| Clicking a jump or a call follows it, with Back and Next | done | `ScriptsPanel.cpp` (Back and "Next") |
| A search in the code | done | `ScriptsPanel.cpp` |
| The source beside the code, decompiled into the scripts' own language | done | the "Source" tab, `src/Editor/Scripts/Decompiler.cpp` with `components/lhvmdecompiler` |
| Editing the source and compiling it into the running program, other scripts keeping their places | done | "Compile" (`src/Editor/Scripts/Decompiler.cpp`, `components/lhvmcompiler`) |
| The original source files of the scripts, when they are there | partial | statements can be put on their recorded source lines (`lhvmtool --source-lines`); the panel shows decompiled source |

## Debugging

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Breakpoints set in the margin | done | the margin's breakpoints (`ScriptsPanel.cpp`); test `DebuggerTest.BreakpointsHoldTasksBeforeTheirInstruction` |
| The running tasks with their stacks, variables and what they wait for | done | the tasks list (`ScriptsPanel.cpp`) |
| Holding, stepping and continuing a task, or all of them; stopping a task or a script's tasks | done | "Hold", "Step", "Continue", "Hold all", "Continue all", "Stop", "Stop its tasks"; test `DebuggerTest.SteppingRunsOneInstructionAtATime` |
| The code view following the picked task's next instruction | done | "Follow task" |
| Global variables changed in place | done | "Globals" |
| The native functions the scripts call, and which openblack has written | done | "Natives" (with what openblack has written) |
| The program's data | done | "Data" |
| Exception handlers of a task | done | "Handlers" |

## Log

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scripts' output and errors from when the editor starts, kept in a ring | done | `src/Editor/Panels/LogPanel.cpp`, `src/Editor/Scripts/LogRing.h` |
| Levels to show, a search, and following the newest lines | done | `src/Editor/Panels/LogPanel.cpp` |
| The number of errors in the tab's title | done | `src/Editor/Panels/LogPanel.cpp` |

## Land scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's own setup script (its features and map commands) shown and stepped | todo | land scripts run once on load; only the console can run their commands (`src/Debug/Console.cpp`) |
