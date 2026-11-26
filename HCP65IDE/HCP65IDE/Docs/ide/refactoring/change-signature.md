# Change Signature

**Ctrl+F6**, or Refactor, Change Signature...: a function's parameters added, removed, put in another order,
renamed, and given other types and default values; what it returns changed; and its name changed.  All of it
in every declaration and definition of the function, in the functions that override it and that it overrides,
in the members a template's body calls with it by one name, and in every call.

## The dialog

```text
Name         [ Fill                         ]
Return type  [ int                          ]

  Type        | Name     | Default value | Value in calls
  ------------+----------+---------------+---------------
  int         | iAmount  |               |
  bool        | bForce   | false         |
  CGauge*     | pcGauge  |               | NULL          <- new
                                        [Add] [Remove] [Up] [Down]

  int Fill(int iAmount, bool bForce = false, CGauge* pcGauge)
```

The parameters are a table whose cells are edited where they are.  **Add** makes a new parameter's row;
**Remove** takes the selected one out, and **Up** and **Down** move it.  For a new parameter, **Value in calls** is what the
calls there already are pass for it.  Under the table is the signature as it would be declared, changing as you
type.  **Refactor** isn't allowed while what is written can't be a signature, and the reason is shown.  Cancel
changes nothing.

## What it changes

- **Each declaration and definition keeps what it wrote.**  A parameter that is kept stays as that file wrote
  it: its type as that file names it, its own name (unless it is renamed), its default value where it has one.
  A new parameter is its type and name, with its default value in the first declaration only.
- **What it returns** is changed where each declaration writes it.
- **The name** is changed by [Rename](rename.md), after the rest.
- **A renamed parameter** is renamed in the body of each definition too, and a parameter ends up with one name
  everywhere.
- **Each call's arguments** are put in the new order; a removed one is taken out; a new one is the value given
  for it, or nothing if it has a default value and nothing comes after it.  An argument a call left to its
  default value stays left out where it still can be, and is written out where an argument now follows it.
- **A constructor's calls** include every place an object of its class is made with arguments: a variable's
  `(` and `{`, `new`, a temporary, a base class's and a member's initialiser.  These are found by how many
  arguments they have.  An object made with no brackets (`CLid cLid;`, `new CLid`) is given them when it gets
  an argument.
- **Hand written assembly**: a function's label has its parameters' types in it where it is overloaded or isn't a
  member.  The compiler is asked for each changed function's label before and after, for each processor (an
  `int` is 2 bytes on one and 4 on the other), and the labels are changed from one to the other.

```{.cpp .no-check}
// Before                                    // After: iAmount and bForce swapped, pcGauge added (NULL in calls)
int CTank::Fill(int iAmount, bool bForce)    int CTank::Fill(bool bForce, int iAmount, CGauge* pcGauge)

cTank.Fill(3, true);                         cTank.Fill(true, 3, NULL);
pcTank->Fill(iLevel);                        pcTank->Fill(false, iLevel, NULL);
```

## Problems Detected

What would go wrong is listed first, and **Continue** goes ahead anyway:

- a removed parameter that a body still uses;
- the function named without being called (its address taken: what it is used as there isn't changed);
- an argument taken out of a call that does something (a call, an assignment), which goes with it;
- a parameter with a default value before one without;
- a new parameter with no value for the calls there are;
- another function of the same name that already has the new parameters;
- a call whose value is used, when the function is to return nothing.

## Unify Parameter Names

Refactor, Unify Parameter Names: the function's parameters are named in every declaration and definition as they
are named where the caret is.  On the declaration, the definition takes the declaration's names; on the
definition, the declaration takes the definition's; on a call, the first declaration's are used.  A definition's
body uses the new names too.  It is Change Signature with nothing changed but the names, and the status says what
was done, or that they are the same already.

## Not done

- An explicit specialisation of a function template is another symbol to the index, and isn't changed with the
  template.
- A function type the function is assigned to elsewhere (a pointer to it) is left, and said.
