# Documentation Pages

The IDE treats a project's Markdown pages as part of its code, as the website does.  A page is a `.md` item of a
project, kept next to the code it describes (the website gathers them; see `mkdocs.yml`'s `extra.sources`).

## References to code

A page names code with a reference, a code span in square brackets:

```text
A [`CTextArea`] is drawn by [`CTextArea::Paint`]; [`CShape::Init(int)`] picks one overload.
The header is [`WindowLib/TextArea.h`].
```

A reference names a class, function, variable, enumerator or file the way the website's build resolves it, with
the same code ([`IndexLib/ReferenceResolver.h`]): an overloaded function without its parameters is its first
declaration; an enumerator can leave out its enum; a file is named by the end of its path.  In the IDE:

| You | The IDE |
|---|---|
| Ctrl+B on a reference | Opens what it names where the website's link goes: a function's declaration in its header, a class's definition, a file. |
| Find Usages on a symbol | Lists the pages that name it in their own group, *Usage in documentation*. |
| Rename a symbol | Changes the references to it in the pages of the open solutions, as it changes code. |
| Rename File | Changes the links to the file in the pages, and the snippet paths (`--8<--`) that name it. |
| Edit a page | Its references are resolved again as you type. |

A reference that names nothing is an error, underlined and in the Problems tab, as the website's build fails on
it.  A plain code span that names a real symbol is a warning: it could be a reference.  A span that isn't C++ but
looks like it is written with a language, `` `#!text abs` ``.

What the open solutions don't have (a kernal structure named in a compiler page while only the IDE's solution is
open) is looked up in the website's symbol table, which its build keeps in `.symbols` beside its `mkdocs.yml`:
Ctrl+B still works, but Rename and Find Usages only cover the open solutions.  A page outside them that names an
old name fails the website's next build, with its line.

## Which website

The website is the `mkdocs.yml` that is an item of a project in the workspace (`Website.vcxproj` has it).
Without one, a project's **Properties...** (in Solution Explorer's menu) can name it, relative to the project's
folder; it is kept in the project's `.hcp65project`.  Without either, references are still resolved against
the open solutions, and links between pages are taken as relative to their page.

## Colours

A page is coloured as Markdown: headings, code spans, HTML comments and fences; a reference in the colour of what
it names (a class, a function...), or as an error when it names nothing; a ```` ```cpp ```` block's C++ by its
words (keywords, strings, numbers, comments).

## Building the website

`Website.sln` is the website as a Visual Studio solution: its projects are Makefile projects, and building one
runs its build command (`bash web.sh build`).  Their output goes to the Build tab, and its problems, each
`<file>:<line>:<column>: error:`, to the Problems tab.

## Files that aren't text

**Open with System Program**, in Solution Explorer's menu, opens the selected file in the program the system has
for it: a PDF, the 65C64's spreadsheet, a KiCad schematic or board, a picture.
