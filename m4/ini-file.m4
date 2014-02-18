# -*- mode: autoconf -*-
# Copyright (c) 2014 Simon Feltman
#
# This file is free software; the author(s) gives unlimited
# permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#

# For usage within autoconf, requires m4sugar to be loaded.

# _ini_split_on_index_exclusive(STRING, INDEX)
# --------------------------------------------------------------------
# Split a string on the given INDEX excluding the character at INDEX
# returning an array.
#
m4_define([_ini_split_on_index_exclusive],
[m4_quote(m4_substr([$1], 0, [$2]), m4_substr([$1], m4_eval([$2]+1)))])


# _ini_split_key_value(STRING)
# --------------------------------------------------------------------
# Split a string in the form of [key=value] into a stripped array
# of [key, value]. If the '=' seperator is not found, -1 is passed to
# _ini_split_on_index_exclusive which will return [[], STRING] which
# turns into a no-op for m4_define, effectively skipping blank lines
# and ini style section headers.
#
m4_define([_ini_split_key_value],
[_ini_split_on_index_exclusive([$1], m4_index([$1], [=]))])


# _ini_define(STRING):
# --------------------------------------------------------------------
# Takes a string in the form of "KEY=VALUE" and parses it into
# m4_define(KEY, VALUE)
#
m4_define([_ini_define],
[m4_apply([m4_define], _ini_split_key_value([$1]))])


# _ini_read_lines(FILENAME):
# --------------------------------------------------------------------
# Read in the contents of FILENAME and split on newlines returning the
# result as a list.
#
m4_define([_ini_read_lines],
[m4_split(m4_include([$1]), m4_newline)])


# ini_include(FILENAME)
# --------------------------------------------------------------------
# Read in the contents of FILENAME and define each KEY=VALUE pair as
# a new m4 macro define.
#
m4_define([ini_include],
[m4_foreach([kv],
            _ini_read_lines([$1]),
            [_ini_define(kv)])])

