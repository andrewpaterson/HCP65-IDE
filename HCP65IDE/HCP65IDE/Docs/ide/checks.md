# Checking the IDE

The IDE is tested by running it: `HCP65-IDE/HCP65IDE/Checks/IDEChecks.py` starts it scripted, has it open a solution,
press keys, run refactorings, answer dialogs and print what it did, and checks what it printed.  There are over 380
checks, one or more for every feature on these pages.

```text
IDEChecks.py [ide executable] [-native] [-outputs <directory>] [-open <file>...] [check name...]
```

| Option | Meaning |
|---|---|
| ide executable | The IDE to run.  Without it, the Release build: `HCP65IDERef` (below), then a few smoke checks on the windowed IDE. |
| `-native` | Runs every check on the IDE in a real window instead. |
| `-outputs <directory>` | Writes what each check printed to `<directory>/<check>.txt`, to compare runs. |
| `-open <file>` | Opens a workspace, a solution, or several solutions (one `-open` each) instead of the default ModPlay workspace.  `-open user` opens whatever you had open last. |
| check name... | Runs only those checks. |

Checks normally run on `HCP65IDERef`, the IDE drawn on a reference screen in memory: no display is needed, and nothing you
type or click can reach it.  Build it as the Reference configuration (Reference|x64 on Windows).

A check never changes what you had open: each run is given its own recent solutions, and anything that changed your
workspace's file or layout is put back when the checks end.  Most checks work on a copy of a small solution of their own,
in `Checks/Input`, so they are quick and their results don't move as other code changes.

## How a check is written

Each check is a name, the IDE's settings for it (`HCP65_IDE_*` environment variables: the solution to open, the file and
caret, the script of steps, the answers to dialogs), the lines it must print in order, and lines it mustn't:

```python
('move-namespace-function', dict(MakeNamespacesCopy(), **{
	'WAIT_INDEX': '1', 'OPEN': 'Shapes.h',
	'DEBUG': 'edit:renamesearch:---s,edit:caret:21:10,edit:movetonamespace:Shapes,printdisklines:Shapes.h'}),
	[r'^Move to Namespace status: Moved Geometry::Area to Shapes: 1 use; 1 label in assembly changed\.$',
	 r'^24\|\tGeometry::TLength\tArea\(Geometry::CPoint\* pcPoint\);$'],
	[r'Problems Detected']),
```

The script's steps (`HCP65_IDE_DEBUG`, `CIDEScript` in `IDEScript.cpp`) move the caret, select text, run commands and
refactorings, press keys, and print the IDE's state: menus, keys, tabs, files on disk, the status line.  A dialog the IDE
shows is answered with the button named in `DIALOG_ANSWER`, and can be typed into first with `DIALOG_KEYS`.

With `HCP65_IDE_VERIFY_REDRAW`, `HCP65_IDE_VERIFY_LAYOUT` or `HCP65_IDE_VERIFY_PRESENT` set, every paint, layout or
present is checked as well, and one that went wrong fails its check.
