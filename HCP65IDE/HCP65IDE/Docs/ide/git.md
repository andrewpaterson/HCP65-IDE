# Git

The IDE shows what git knows of the solutions' files as IntelliJ IDEA does.  It reads each repository the
solutions' files are in when a solution opens, when a file is saved and every few seconds after, and
whenever the IDE's window is activated (git may have been used outside it).  The toolbar shows the branch
that is checked out.

## Colours

A file's name, in Solution Explorer and on its editor tab, is coloured by what has happened to it since the
last commit:

| Colour | The file is |
|---|---|
| The text colour | Unchanged. |
| Blue | Changed, or renamed. |
| Green | Added (staged). |
| Grey | Deleted. |
| Red | Not tracked. |
| Olive | Ignored. |
| Pale red | In conflict: both sides of a merge changed it. |

A folder or project with changed files in it is blue too.

## Change bars

The lines changed since the last commit are marked in the editor's gutter, beside the line numbers: a bar
beside lines changed or added, and a mark where lines were removed.  Click one to see what changed and the
lines as they were in the last commit, with:

- **Rollback**, which puts them back as they were (one undo);
- **Show Diff**, which opens the file's diff against HEAD.

The bars follow your edits as you type.

## Diffs

| Command | Compares |
|---|---|
| Git, Compare with HEAD | The selected file with its last commit. |
| Git, Compare with Branch... | With the file on a branch chosen from a list. |
| Git, Compare with Revision... | With the file at any revision git understands (`HEAD~1` to begin with, a commit, a tag). |
| File, Compare with File... | With another file chosen from the disk. |

A diff opens in a document tab of its own, as IntelliJ IDEA's diff viewer shows one: the old text on the
left, the new on the right, each with its line numbers, and a band behind each changed block in the colour
of its kind (inserted, deleted or changed), joined across the divider to the lines it became.  Within a
changed block the characters that differ are a stronger shade.  The two sides scroll together.  It is read
only, but its text can be selected and copied.

Its toolbar has Previous and Next Difference (Shift+F7 and F7), how whitespace is compared, how changes are
highlighted, and how many differences there are.  A diff follows its files: edit a side in its editor and
the diff is shown again a moment later, and a revision is read again when its repository gets a new
commit.

## What the IDE does to git

The IDE doesn't commit, push or pull.  It only keeps git's index in step with what the refactorings do to
files:

- a file a refactoring or New makes is staged (`git add`), and taken out of the index again if that is
  undone;
- [Rename File](refactoring/rename.md#rename-file) renames a tracked file with `git mv`.
