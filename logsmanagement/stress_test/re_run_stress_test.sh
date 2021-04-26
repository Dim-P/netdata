#!/bin/bash
sudo killall netdata
sudo rm -f /tmp/netdata/var/log/netdata/error.log
sudo rm -rf /tmp/netdata/var/cache/netdata/logs_management_db 
sudo rm -rf /tmp/netdata_log_management_stress_test_data 
sudo -u netdata -g netdata mkdir /tmp/netdata_log_management_stress_test_data 
gcc stress_test.c -luv -o stress_test 
sudo -u netdata -g netdata ./stress_test 1 &
sleep 2
#sudo systemctl start netdata
sudo -u netdata -g netdata -s gdb -ex=run --args /tmp/netdata/usr/sbin/netdata -D