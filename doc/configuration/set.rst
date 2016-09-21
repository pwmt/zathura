set - Changing options
======================

In addition to the built-in *:set* command zathura offers more options
to be changed and makes those changes permanent. To overwrite an option
you just have to add a line structured like the following

::

    set <option> <new value>

The *option* field has to be replaced with the name of the option that
should be changed and the *new value* field has to be replaced with the
new value the option should get. The type of the value can be one of the
following:

-  INT - An integer number
-  FLOAT - A floating point number
-  STRING - A character string
-  BOOL - A boolean value ("true" for true, "false" for false)

In addition we advice you to check the options to get a more detailed
view of the options that can be changed and which values they should be
set to.

The following example should give some deeper insight of how the *set*
command can be used

::

    set option1 5
    set option2 2.0
    set option3 hello
    set option4 hello\ world
    set option5 "hello world"
    set option6 "#00BB00"

