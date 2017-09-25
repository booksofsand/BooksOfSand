#!/bin/bash
# 
# -----------------------------------------------------------------------------
# AUTHOR:  Maya Montgomery
# DATE:    09/24/2017
# TITLE:   search.sh
# -----------------------------------------------------------------------------
# PURPOSE: Recursively seeks given text in all files in given path(s),
#          prints names of files containing the text.
#
#          Text currently can only be one word (i.e. no spaces)
#          and paths cannot have a trailing forward slash. 
# 
# USAGE:
# ./search.sh text path(s)
# ./search.sh GenericClass path1 path2
# -----------------------------------------------------------------------------

searchDir () {
    # 1: dir path
    
    for name in $1/*; do
	# for every file
	if [ -f "$name" ]; then

	    # check for text
	    result=$(cat "$name" | grep "$text")
	    if [ "$result" != "" ]; then
		echo $name
	    fi

	# for every directory
	elif [ -d "$name" ]; then
	    searchDir "$name"
	fi
    done
}

# check argument count
if (( $# < 2 )); then
    echo "Error: Not enough arguments."
    echo "Usage: ./search.sh text path(s)"
    exit 1
fi

# save given text
text="$1"
echo "Searching for $1"
echo
shift

# search through each given path
while (( $# > 0 )); do
    searchDir "$1"
    shift
done
