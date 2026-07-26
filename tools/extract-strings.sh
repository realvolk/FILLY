#!/bin/sh
# Extract translatable strings from FILLY source
POT="po/filly.pot"
mkdir -p po
> "$POT"

find src plugins -name '*.c' | while read f; do
    grep -n '_("[^"]*")' "$f" | while read line; do
        str=$(echo "$line" | sed 's/.*_("\([^"]*\)").*/\1/')
        file=$(echo "$line" | cut -d: -f1)
        num=$(echo "$line" | cut -d: -f2)
        printf '#: %s:%s\nmsgid "%s"\nmsgstr ""\n\n' "$file" "$num" "$str" >> "$POT"
    done
done
sort -u -o "$POT" "$POT"
echo "Generated $POT"