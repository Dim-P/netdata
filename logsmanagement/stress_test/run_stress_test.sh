#!/bin/bash
if [ $# -eq 0 ]
then
	num_log_sources=1
else
	num_log_sources=$1
fi

echo "=========================================="
echo "Killing existing netdata instances and stress tests..."
sudo systemctl stop netdata
sudo killall stress_test

echo "=========================================="
echo "Removing existing simulated netdata logs-management files..."
sudo rm -rf /tmp/netdata_log_management_stress_test_data
echo "Removing DBs /tmp/netdata/var/cache/netdata/logs_management_db/* ..."
sudo rm -rf /tmp/netdata/var/cache/netdata/logs_management_db
echo "Removing potential existing /tmp/netdata/etc/netdata/log_management.conf ..."
sudo rm -f /tmp/netdata/etc/netdata/log_management.conf
echo "Removing /tmp/netdata-logs-stress-test pipe ..."
sudo rm -f /tmp/netdata-logs-stress-test

echo "=========================================="
echo "Rebuilding Netdata with stress tests for logs-management enabled..."
echo "=========================================="
cd ../../..
yes | sudo ./netdata-uninstaller.sh --yes --env /tmp/netdata/etc/netdata/.environment
sudo rm -rf /tmp/netdata/etc/netdata # Remove /etc/netdata if it persists for some reason
cd netdata
sudo CFLAGS="-O1 -ggdb -DNETDATA_INTERNAL_CHECKS=1 -DLOGS_MANAG_DEBUG_LEV=2 -DLOGS_MANAGEMENT_STRESS_TEST=1" ./netdata-installer.sh --dont-start-it --disable-cloud --disable-ebpf --disable-lto --enable-logsmanagement --install /tmp
# make && sudo make install

cd logsmanagement/stress_test
sudo cp log_management.conf /tmp/netdata/etc/netdata

sudo -u netdata -g netdata mkdir /tmp/netdata_log_management_stress_test_data
#for (( i = 0; i < $num_log_sources; i++ )) 
#do
#	sudo -u netdata -g netdata echo -e "First line dummy data!!\n" > /tmp/netdata_log_management_stress_test_data/"$i".log
#done

echo -e "\n=========================================="
echo "Building and executing stress_test..."
echo "=========================================="

gcc stress_test.c -luv -o stress_test 
sudo -u netdata -g netdata ./stress_test "$num_log_sources" &
sleep 2
#sudo systemctl start netdata
sudo -u netdata -g netdata -s gdb -ex=run --args /tmp/netdata/usr/sbin/netdata -D