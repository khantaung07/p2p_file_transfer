#!/bin/bash

echo -e "\n=================================================="
echo -e "               Running Part 1 Tests              "
echo -e "==================================================\n"

count=0
total=0

# Loop through all '.args' and '.out' test files in 'p1tests' directory
for test in p1tests/*.args; do
	test_id=$(basename $test .args)
	expected=p1tests/$test_id.out
    echo "Running $test_id..."

    cat $test | xargs ./pkgmain | diff - $expected

    if [[ $? -eq 0 ]]; then
        echo -e "\033[32mTest $test_id successful!\033[0m\n"
        count=$((count+1))
    else
        echo -e "\033[31mTest $test_id failed!\033[0m\n"
    fi

	total=$((total+1))
done

# Seperate test to check file creation in test 8
echo "Running 8_file_check..."

diff p1tests/test8.data p1tests/test8.expected

if [[ $? -eq 0 ]]; then
        echo -e  "\033[32mTest 8_file_check successful!\033[0m\n"
        count=$((count+1))
    else
        echo -e "\033[31mTest 8_file_check failed!\033[0m\n"
    fi

total=$((total+1))
# Remove created file
rm -rf p1tests/test8.data

echo -e "=================================================="
echo -e "           \033[34m$count/$total tests ran successfully!\033[0m"
echo -e "==================================================\n"