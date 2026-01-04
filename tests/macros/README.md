# Macros

Macros can be used for end-to-end testing of UI interactions in Zathura.

## Setup 

Create shortcuts for the various macro commands.
Below is an example config.

```
set macro-filter "<F1><F2><F3><F4>"
set macro-filename "tests/macros/girara.macro"

map <F1> set "macro-record true"
map <F2> set "macro-record false"
map <F3> macro_assert
map <F4> macro_breakpoint
map <F5> macro_load "./tests/girara.macro"
map <F6> macro_run

map <F7> feedkeys "<F8><F9><F10>gg"
map <F8> set "pages-per-row 1"
map <F9> set "first-page-column 1:2"
map <F10> zoom specific
```

## Record A Macro

Firstly a macro needs to be recorded.
Set `macro-record` variable to `true` to begin recording.
The macro will be written to the `macro-filename` file.

Use `macro_assert` to add an assertion to the recording, more details on assertions below.
Use `macro_breakpoint` to add a breakpoint to the recording, more details on breakpoints below.

Finally set `macro-record` to false to finish the recording and write the output to file.

### File Format

Macros are stored in a simple text format.
The file begins with `[x]` where `x` is an integer denoting the version.
Single line comments begin with `#` and are ignored during parsing.
There are 3 identifiers, `keypress`, `assert`, and `breakpoint`.
`keypress` is followed by 2 integers, `state` and `keyval`. 
`assert` is followed by 2 strings, `name`, `value`.
`breakpoint` has no arguments.

### Notes 

Care should be taken with file opening during macro recording.
Tab completions may change between recording and replay, 
file structure may change, and it is good practice to add a breakpoint 
after opening a file to ensure the document has loaded before continuing.

## Execute Macro

To run a macro, first load the macro using `:macro_load <filename>`.
To start the macro execution, run the shortcut `macro_run`.

## Assertions

The shortcut `assert` will record assertion for the current position and size of the document.
Assertions are checked when the macro is replayed, with the Pass/Fail result printed to stdout.
When an assertion fails, macro execution stops. 
Execution can be resumed by calling `macro_run` again.

## Breakpoints

Similarly to failed assertions, if a breakpoint is reached, macro execution is stopped.
Execution can be resumed by calling `macro_run` again.

