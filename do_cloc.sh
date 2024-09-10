#!/usr/bin/env bash
echo "BBAE:"
cloc src --quiet --hide-rate
printf "BBAE lines that are only punctuation: "
grep -rohI '^[ \t{}()]*$' src -r | grep -hv "^[ \t]*$" | wc -l
echo "BBEL:"
cloc bbel --quiet --hide-rate
printf "BBEL lines that are only punctuation: "
grep -rohI '^[ \t{}()]*$' bbel -r | grep -hv "^[ \t]*$" | wc -l
