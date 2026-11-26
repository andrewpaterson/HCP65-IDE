# Git

The IDE shows what git knows of the solutions' files as IntelliJ IDEA does: the branch on the toolbar, file names
coloured by their state, change bars in the editors' gutters, and diffs.  What the user sees is on
[Git](../git.md).  This page is how it is built: GitLib, which runs git, and the IDE's [`CIDEGitStatus`] and
[`CIDEDiffs`], which show what it read.

## GitLib: the git command line

GitLib (`HCP65-IDE/Libraries/GitLib`) runs the git program, as IntelliJ IDEA does, rather than linking a library or
reading `.git` itself: the user's own git, with its settings, credentials, hooks and attributes, is the one that
answers.  [`CGitCommand`] is one run of it in a repository:

- `#!text git -C <repository> --no-pager --no-optional-locks -c core.quotepath=off`, so its output is the same
  whoever runs it (never paged or coloured, file names as they are), and looking at a repository takes no locks
  that would get in the way of the user's own commands;
- its output kept byte for byte and its errors apart ([`CProcess::StartCapture`], [`CProcess::PollCapture`]), since
  much of it is data: a file's text, or names split by `'\0'`;
- run without waiting (Start, then Poll from a window's tick) or waited for (Run);
- with empty input: nothing that might ask for a password (fetch, push) is run.

| Class | Is |
|---|---|
| [`CGitRepositories`] | The repositories the solutions' files are in, which can be several (IntelliJ's version control roots): a solution's projects can come from other repositories.  A repository is added the first time a file in it is asked about ([`FindGitRoot`]: the closest folder up with a `.git`, no git run), and each folder's repository is cached. |
| [`CGitRepository`] | One repository: its status, read again whenever asked, and its files' text at a revision ([`CGitRepository::ReadHeadText`], [`CGitRepository::ReadRevisionText`]: `#!text cat-file --filters`, so the text is as checked out, line endings and all).  Also its branches, whether a file is tracked, and the three writes the IDE makes: `mv`, `add` and `rm --cached`. |
| [`CGitStatus`] | `#!text git status --porcelain=v2 -z --branch --ignored` parsed: the branch, head, upstream, ahead and behind, and each file listed with its [`EGitFileState`].  git lists a folder of only untracked or only ignored files as the folder, so a file's state is found from it or the folders it is in. |
| [`CGitLineChanges`] | A file's text, edited or not, compared line by line with its text in the last commit ([`CTextDiffProducer`], whitespace counted): the change bars. |

Solution and project paths are relative and contain `..`, so every GitLib entry point makes its paths full first
([`MakeFullGitPath`]); git is given paths relative to the repository's top.

## Statuses

[`CIDEGitStatus`] finds the repositories from the open solutions' files ([`CIDEGitStatus::FindRepositories`]) and
reads their statuses at once, waiting, so the first look at Solution Explorer is coloured.  After that it reads
them without waiting ([`CIDEGitStatus::PollGit`] from the content's tick): every 5 s (`IDE_GIT_REFRESH`), looking
every 20 ms while a read is under way; at once for a file's repository when it is saved
([`CIDEGitStatus::GitFileSaved`]), then 5 s from then; and when the window is activated, since git may have been
used outside it ([`CIDEGitStatus::WindowActivated`]).  A read asked for while one is under way is made again after
it ([`CGitRepository::RefreshStatus`]): the one under way may be from before the file changed.  Each repository
counts the statuses it has read, and only a new one is shown.

A file's colour is IntelliJ's for its state ([`GetGitStateColour`]); Solution Explorer colours files, and folders,
projects and the solution with changed files in them (not untracked or ignored ones), with
[`CSolutionExplorer::ColourFiles`].  The editor tabs take the same font ([`GetGitStateFont`]).  The branch label
follows the selected file's repository.

## Change bars

An open file's text in the last commit is read when the file is opened, and again when its state or its
repository's last commit changes ([`CIDEGitStatus::ReadGitBase`]).  An untracked or ignored file has none and no
bars; one added since the last commit has an empty one, so every line is added.  At each tick, an editor whose
change count isn't the one its bars were made for is compared again ([`CIDEEditorTab::NeedsChangeBars`],
[`CIDEGitStatus::UpdateChangeBars`]), so the bars follow typing; the whole file is compared each time.  The bars
are WindowLib's ([`CCodeEditor::AddChangeBar`]; [The Text Area](../../libraries/windowlib/text-area.md)).

A click on one ([`CCodeEditor::GetChangeBarClickEvent`]) shows a popup with the old lines, Show Diff and Rollback
([`CIDEGitStatus::ShowChange`]).  Rollback replaces the lines in the editor ([`CIDEGitStatus::RollbackChange`]), so
it is one undo like any edit.  A scripted run queues a change to show, roll back or diff for the tick after the
editor is laid out (`HCP65_IDE_CHANGE`, [`CIDEGitStatus::QueueChange`]), as a click would find it.

## Diffs

A diff is a document tab of its own ([`CIDEDiffTab`]) showing WindowLib's [`CDiffView`] of two sides, each a file
at a revision git knows or the file as it is (its editor's text if open, else the disk's).  [`CIDEDiffs`] opens
them: Compare with HEAD, a branch, a revision, or another file.  A tab follows its sides: 400 ms after an edit in a
side's editor it is made again ([`CIDEDiffTab::NeedsRefresh`]), and a revision is read again when its repository's
last commit changes ([`CIDEDiffs::RefreshMovedDiffs`]).  The view itself, and its decisions, are on [The Diff
View](../../libraries/windowlib/diff-view.md).

## What the IDE writes to git

Only what keeps git's index in step with what the refactorings do to files: [Rename
File](../refactoring/rename.md#rename-file) renames a tracked file with `git mv` ([`CGitRepository::Move`]), and a
file a refactoring or New makes is staged ([`CGitRepository::Add`]) and taken out of the index again when that is
undone ([`CGitRepository::Unstage`], [`CIDEMoveClass::RemoveCreatedFile`]).  Nothing commits, pushes or pulls.

## Tests

TestGitLib parses statuses as git writes them, compares lines, and makes a real repository under its `Output`
folder to run git on: paths, statuses, the repositories a solution's files are in, commands, and `git mv`.  The IDE
checks `git-and-diffs`, `git-change-popup-closes` and `git-dialogs` drive the statuses, bars, popup and diffs ([The
IDE Checks](checks.md)).

## Decisions

- GitLib lives in HCP65-IDE/Libraries, and runs the git command line only ([`CProcess`]): no libgit2 and no reader
  of `.git` of its own, for now (the user, 2026-11-05).
- The first features were the status colours with the branch, and the gutter's change bars (the user, 2026-11-05);
  diffs followed the same day, unaligned and read only as IntelliJ's default (see the diff view's page).
- Rename File uses `git mv` for a tracked file (the user, with Rename File, 2026-11-13).

## Not done

- A commit window, history and annotate, branches (checking out, making), fetch, pull and push: none started.
  Commands that might ask for a password aren't run at all yet.
- An editable right side in a diff ([The Diff View](../../libraries/windowlib/diff-view.md#not-done)).
- [`CProcess`]'s capture on Windows was written with the Linux half but, as of 2026-11-05, never run.
