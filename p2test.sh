#!/bin/bash

echo -e "\n=================================================="
echo -e "               Running Part 2 Tests              "
echo -e "==================================================\n"

test_dir="./p2tests"
#initial_dir=$(pwd)

count=0
total=0

for dir in "$test_dir"/*/
do
    # Go into the directory
    dir=${dir%*/}

    # Perform test
    test_id=$(basename $dir)
    args="$dir/$test_id.args"
    input="$dir/$test_id.in"
    expected="$dir/$test_id.out"
    echo "Running $test_id..."

    ./btide $(cat $args) < $input | diff - $expected

    if [[ $? -eq 0 ]]; then
        echo -e "\033[32mTest $test_id successful!\033[0m\n"
        count=$((count+1))
    else
        echo -e "\033[31mTest $test_id failed!\033[0m\n"
    fi

    total=$((total+1))

done

# Run packet tests
./packet_dump ./p2_packet_tests
chmod +x ./resources/pktval

for packet in p2_packet_tests/*.pkt; do
    packet_id=$(basename $packet .pkt)
    expected="./p2_packet_tests/$packet_id.out"
    echo "Running $packet_id test..."

    ./resources/pktval $packet | diff - $expected

    if [[ $? -eq 0 ]]; then
        echo -e "\033[32mTest $packet_id successful!\033[0m\n"
        count=$((count+1))
    else
        echo -e "\033[31mTest $packet_id failed!\033[0m\n"
    fi

    total=$((total+1))

    # Remove the generated packet
    rm -rf $packet

done

echo -e "=================================================="
echo -e "           \033[34m$count/$total tests ran successfully!\033[0m"
echo -e "==================================================\n"
