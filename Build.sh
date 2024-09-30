# Capture the user's current directory
ORIGINAL_DIR=$(pwd)

# Get the directory of the current script
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$DIR"

mkdir -p build
mkdir -p tests/build

LIBS="-lpq"
SRC="src/DatabaseModule.c src/Postgres.c"
TEST_SRC="tests/Main.c tests/databases/Postgres.c"

if [ "$1" = "release" ]; then
    CFLAGS="-O2 -march=native  -Wextra -pedantic -Wno-unused-function -Wno-cpp -DNDEBUG"
elif [ "$1" = "relwdb" ]; then
    CFLAGS="-O2 -g -DNDEBUG -Wno-unused-function -Wno-cpp"
else
    CFLAGS="-g -O0 -Wall -Wno-unused-function -Wno-cpp -DDEBUG"
fi

printf "\033[0;32m\nBuilding Sensor DB\n\033[0m"
gcc $CFLAGS src/Main.c $SRC -o build/SensorDB $LIBS
printf "\033[0;32mFinished building Sensor DB\n\033[0m"

printf "\033[0;32m\nBuilding Sensor DB tests\n\033[0m"
gcc $CFLAGS $TEST_SRC $SRC -o tests/build/SensorDB $LIBS
printf "\033[0;32mFinished building Sensor DB tests\n\033[0m"

cd "$ORIGINAL_DIR"
