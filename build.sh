# Capture the user's current directory
ORIGINAL_DIR=$(pwd)

# Get the directory of the current script
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$DIR"

mkdir -p build

if [ "$1" = "release" ]; then
    CFLAGS="-O3 -march=native -DNDEBUG"
elif [ "$1" = "relwdb" ]; then
    CFLAGS="-O2 -g -DNDEBUG"
else
    CFLAGS="-g -O0 -Wall -Wextra -pedantic -DDEBUG"
fi

clang $CFLAGS src/main.c -o build/main

cd "$ORIGINAL_DIR"
