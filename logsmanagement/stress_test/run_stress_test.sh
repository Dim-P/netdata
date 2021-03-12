echo "=========================================="
echo "Killing existing netdata-logs instances..."
killall netdata-logs
echo "=========================================="
echo "Removing existing ../test_data/*.log files"
rm ../test_data/*.log
echo "Removing DBs ../databases/*"
rm -rf ../databases
echo "=========================================="
echo "Building netdata-logs with DEBUG_LEV=1..."
echo "=========================================="
cd .. && make -f Makefile_standalone_build.mk clean; make -B -j4 DEBUG_LEV=1 STRESS_TEST=1 -f Makefile_standalone_build.mk && cd stress_test
echo -e "\n=========================================="
echo "Building and executing stress_test..."
echo "=========================================="
gcc stress_test.c -luv -o stress_test && ./stress_test
