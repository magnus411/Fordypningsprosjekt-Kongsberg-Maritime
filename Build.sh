# Capture the user's current directory
ORIGINAL_DIR=$(pwd)

# Get the directory of the current script
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$DIR"

mkdir -p build

SRC="src/Main.c src/DatabaseModule.c src/Postgres.c"
LIBS="-lpq"

if [ "$1" = "release" ]; then
    CFLAGS="-O2 -march=native  -Wextra -pedantic -Wno-unused-function -DNDEBUG"
elif [ "$1" = "relwdb" ]; then
    CFLAGS="-O2 -g -DNDEBUG -Wno-unused-function"
else
    CFLAGS="-g -O0 -Wall -Wno-unused-function -DDEBUG"
fi

clang $CFLAGS $SRC -o build/SensorDB $LIBS

cd "$ORIGINAL_DIR"
