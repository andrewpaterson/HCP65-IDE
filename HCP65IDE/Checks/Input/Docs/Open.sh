#!/bin/sh
# The checks' stand-in for the system's program (HCP65_IDE_SYSTEM_OPEN): it writes the file it was given beside
# itself, in Opened.txt.
echo "$1" > "$(dirname "$0")/Opened.txt"
