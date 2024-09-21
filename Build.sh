# Capture the user's current directory
ORIGINAL_DIR=$(pwd)

# Get the directory of the current script
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$DIR"

mkdir -p Build

SRC="Code/Main.c Code/DatabaseModule.c Code/Postgres.c"
LIBS="-lpq"

if [ "$1" = "release" ]; then
    CFLAGS="-O3 -march=native -DNDEBUG"
elif [ "$1" = "relwdb" ]; then
    CFLAGS="-O2 -g -DNDEBUG"
else
    CFLAGS="-g -O0 -Wall -Wextra -pedantic -DDEBUG"
fi

clang $CFLAGS $SRC -o Build/SensorDB $LIBS

cd "$ORIGINAL_DIR"
