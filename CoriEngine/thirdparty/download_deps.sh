#!/bin/sh
set -e

# If you passed args, use them; otherwise default to a shallow clone
if [ $# -gt 0 ]; then
    CLONE_ARGS="$*"
else
    CLONE_ARGS="--depth 1"
fi

# make sure we’re at the repo root
cd "$(dirname "$0")/../.."

# Read .gitmodules line by line
while IFS= read -r line; do
    case "$line" in
        \[submodule*)
            # beginning of a new submodule stanza
            unset path url branch
            ;;
        path\ =\ *)
            path=${line#* = }
            ;;
        url\ =\ *)
            url=${line#* = }
            ;;
        branch\ =\ *)
            branch=${line#* = }
            # once we have path, url & branch, do the clone
            if [ -z "$path" ] || [ -z "$url" ] || [ -z "$branch" ]; then
                echo "⚠️  malformed entry in .gitmodules, skipping"
            elif [ -d "$path/.git" ]; then
                echo "✔️  '$path' already exists, skipping"
            else
                echo "⏬ Cloning '$path' from '$url' (branch '$branch')"
                git clone \
                    --filter=blob:none \
                    $CLONE_ARGS \
                    --branch "$branch" \
                    --recurse-submodules \
                    "$url" "$path"
            fi
            ;;
    esac
done < .gitmodules
