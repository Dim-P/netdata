sudo rm -rf /var/cache/netdata/logs_management_db 
sudo rm -rf /tmp/netdata_log_management_stress_test_data 
sudo -u netdata -g netdata mkdir /tmp/netdata_log_management_stress_test_data 
sudo ./stress_test 3