# New Files and Templates

Every file the IDE makes, and every piece of code its refactorings write, comes from a **template** that
can be changed, as IntelliJ IDEA's File and Code Templates do.  A new class's header, the licence comment
at the top of a file, the banner of slashes over a function, a getter's body: change the template and
everything made from then on follows it.

## New files

Alt+Insert, or File, New, shows a menu of the Files templates: C++ Class..., C++ Header..., C++ Source...,
and any you have added.  In Solution Explorer the files go where the selection is: a project's directory,
a filter's files' directory (and into that filter), or a file's directory and filter.  In the editor they
go beside the active file.

The dialog asks for the name, and for any variable the template uses that has no value.

| Template | Name typed | Files made |
|---|---|---|
| C++ Class | `CKeg` | `Keg.h` with the class, `Keg.cpp` with its `Init` and `Kill` |
| C++ Header | `Keg` | `Keg.h` |
| C++ Source | `Keg` | `Keg.cpp` |

The files are written, added to the project and its filters, added to any `CMakeLists.txt` that lists the
files beside them, staged in git, and opened.  One undo takes them all away again.  A name another file of
the solution already has is refused.

On a class in the editor, Alt+Insert also offers [Copy Class...](refactoring/move.md#copy-class).

## The templates

File, Settings, **File and Code Templates** shows them, in three tabs:

| Tab | Templates |
|---|---|
| Files | What New makes, and what the refactorings that make files write: **C++ Class** (the header) with its child **C++ Class Source**, **C++ Header**, **C++ Source**. |
| Includes | Pieces the others pull in with `#parse`: **File Header** (the licence comment), **Include Guard Open**, **Include Guard Close**. |
| Code | What the refactorings write inside files: **Definition Banner** (the slashes over a function), **New Function Body**, **Getter Body**, **Setter Body**, **Template Interface Comment**. |

A template has a name, an extension and a file name (Files only), and its text.  A child template (C++
Class Source) makes another file with its parent.  On the page:

- the list shows each tab's templates, one your scheme has changed in the accent colour, a child under
  its parent;
- **Add** makes a new template, **Remove** takes one you added away, **Copy** copies the one selected,
  and **Reset** gives a changed one back the text it had;
- the text is edited in a code editor that colours `${...}` and the directives, with any error shown
  under it as you type (an `#if` with no `#end`, an unknown method), at its line and column;
- under it are the variables the template can use, and what each is;
- **Author** is what `${AUTHOR}` is; left empty, it is git's `user.name`.

OK keeps the changes and writes them; Cancel throws them away.  The built-in templates write exactly what
the IDE always wrote, so nothing changes until you change one.

### Schemes

A template is looked for in two **schemes**, then among the built-in ones:

| Scheme | Where it is kept | For |
|---|---|---|
| Workspace | `fileTemplates\` beside the `.hcp65workspace` (or the solution) | This workspace, shared through version control. |
| Default | `fileTemplates\` in your [settings directory](settings.md#where-settings-are-kept) | Every workspace you open. |

Each template is a file there, `<Tab>\<Name>.ft` (a Files template's `<Name>.<extension>.ft`), with the
list of them in `Templates.xml`, so they can be read and compared like code.

## The template language

Templates are written in a subset of **Apache Velocity**, the language IntelliJ IDEA and CLion use, so a
template written for either works here and the other way.

| Written | Means |
|---|---|
| `${NAME}`, `$NAME` | A variable's value.  With no value it is written as it is (`$NAME`). |
| `$!{NAME}` | A variable's value, or nothing. |
| `#set ($X = "...")` | Gives a variable a value. |
| `#if (...)` `#elseif (...)` `#else` `#end` | Chooses, with `==`, `!=`, `<`, `>`, `&&`, <code>&#124;&#124;</code> and `!` (or `eq`, `ne`, `and`, `or`, `not`). |
| `#foreach ($M in $MEMBERS)` ... `#end` | Repeats for each item of a list. |
| `#parse("File Header")` | Puts in an Includes template. |
| `## ...`, `#* ... *#` | Comments, not written. |
| `\$`, `\#` | A `$` or `#` itself.  Write a C++ `#else` or `#end` as `\#else`, since both are Velocity's. |

A line with nothing on it but directives and white space leaves no blank line behind (Velocity 2's
"lines" whitespace), so a template can be laid out readably.

A value can have methods called on it, as Java strings do: `toUpperCase()`, `toLowerCase()`, `trim()`,
`length()`, `isEmpty()`, `substring(i, j)`, `replace(a, b)`, `startsWith(s)`, `endsWith(s)`, `contains(s)`,
`equals(s)`, `indexOf(s)`.  The IDE adds three of its own:

| Method | Example |
|---|---|
| `toUpperSnakeCase()` | `IDEMoveClass` gives `IDE_MOVE_CLASS` (the include guards' rule) |
| `toLowerSnakeCase()` | `IDEMoveClass` gives `ide_move_class` |
| `withoutPrefix()` | `CKeg` gives `Keg` |

### Variables

Every template can use:

| Variable | Value |
|---|---|
| `${NAME}` | The file's name without its extension. |
| `${FILE_NAME}` | The file's name. |
| `${HEADER_FILENAME}` | The header's name (`Keg.h`), for a source file's `#include`. |
| `${INCLUDE_GUARD}` | `__KEG_H__`, from the file's name. |
| `${DIRECTORY}` | The directory the file is made in. |
| `${CLASS_NAME}`, `${CLASS_STEM}` | The class's name (`CKeg`) and the same without its prefix (`Keg`). |
| `${PROJECT_NAME}` | The project's name (`WindowLib`). |
| `${LICENCE}` | The project's licence: `LGPL 3`, `AGPL 3` or `None`. |
| `${AUTHOR}`, `${USER}` | The Author setting (or git's `user.name`), and the login name. |
| `${YEAR}`, `${MONTH}`, `${DAY}`, `${HOUR}`, `${MINUTE}` | Now. |
| `${MONTH_NAME_SHORT}`, `${MONTH_NAME_FULL}`, `${DAY_NAME_SHORT}`, `${DAY_NAME_FULL}` | Their names. |
| `${DATE}`, `${TIME}` | `2026-11-16`, `14:05`. |
| `${PRODUCT_NAME}` | `HCP65 IDE`. |
| `${NAMESPACES_OPEN}`, `${NAMESPACES_CLOSE}` | Empty, for now. |

What a refactoring gives the template it uses:

| Variable | In | Value |
|---|---|---|
| `${INCLUDES}` | C++ Header, C++ Source | The new file's `#include` lines. |
| `${BODY}` | C++ Header, C++ Source, New Function Body | The class and what it defines outside itself; or the statements a new function is made of. |
| `${FUNCTION_NAME}`, `${CLASS_NAME}`, `${RETURN_TYPE}` | New Function Body | The new function's. |
| `${FIELD}`, `${PARAMETER}`, `${IS_OBJECT}` | Getter Body, Setter Body | The field, the setter's parameter, and whether the getter gives a pointer to it. |
| `${PARAMETER}`, `${MEMBERS}`, `${INDENT}` | Template Interface Comment | The template parameter, the members it needs, and the template's indent. |

This is the built-in C++ Class template:

```text
## New's C++ Class: the class's header (its source is the C++ Class Source under it).
#parse("File Header")
#parse("Include Guard Open")


class ${CLASS_NAME}
{
public:
	void	Init(void);
	void	Kill(void);
};


#parse("Include Guard Close")
```

## A project's licence

The File Header writes the licence comment that every new file starts with.  Which licence is a project's
own setting: in Solution Explorer, right-click the project, **Properties...**, and choose LGPL 3, AGPL 3 or
None.  It is kept in `<project>.hcp65project` beside the `.vcxproj`; a project with no such file is LGPL 3.

## Save File as Template

File, Save File as Template... makes a Files template of the file in the editor, in the Workspace scheme (the
Default one with no workspace).  Its `$` and `#` that Velocity would read are escaped, its include guard
becomes `${INCLUDE_GUARD}`, its header's name `${HEADER_FILENAME}`, and its class's name `${CLASS_NAME}`
(its files then named by `${CLASS_STEM}`).  Finish it on the File and Code Templates page.
