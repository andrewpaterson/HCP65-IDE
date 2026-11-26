# The Claude Tab

The Claude tab under the editor sends requests to [Claude Code](https://claude.com/claude-code) and shows
what it does, so you can ask for a change ("make this function a member of CTank", "write the tests for
CPipe") without leaving the IDE.  It runs your own `claude` command, signed in as you are, with your
settings and the solution folder's `CLAUDE.md`, so Claude Code must be installed and on the path.

## Asking

Type the request in the box at the bottom and press **Send**.  With it, Claude is told which file is
active and where its caret and selection are, so "this function" and "the selected lines" mean what you
can see.  Before a request is sent the IDE saves every file, so Claude reads what you see.

What Claude writes is shown as it writes it.  Each tool it uses is a line of its own (the tool, and the
file or command it used it on, named from the solution's folder); a tool's result is shown only when it
fails.  A line under the conversation says how the request is going.  **Stop** ends a request.

The conversation carries on from one request to the next, as a conversation with Claude Code does.
**New Conversation** starts a fresh one.

**Ask Claude...**, in the editor's right-click menu, shows the tab and puts the keys in the request box,
so a selection can be asked about without reaching for the tab.  Right-clicking in the conversation gives
Copy, Copy All, Select All, Stop and New Conversation.

## What Claude may do

No one can answer a question from a request run this way, so Claude may read and change files in the
solution's folder without asking (Claude Code's `acceptEdits` mode).  Anything it would have to ask about,
such as running a command, is refused, and the finished request lists what was refused.

When Claude changes a file that is open, the editor reads it again as it changes, so the changes show up in
its tab.  They can be undone there like any other edit.

## Limits

- The request goes on the command line, which Windows limits to about 32,000 characters: ask about a file
  and its lines rather than pasting long text into the box.
- The IDE's fonts only have ASCII, so the punctuation Claude writes in Unicode (curly quotes, dashes,
  arrows) is shown as the nearest ASCII, and other characters as `?`.
