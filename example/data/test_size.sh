#########################################################################
# File Name: test_times.sh
# Author: pxz
# Created Time: Mon 21 Sep 2020 04:43:35 PM CST
#########################################################################
#!/bin/bash
test_size=("1k" "10k" "100k" "1M" "2M" "3M" "4M" "5M" "10M" "20M" "30M")
num=0

while((${num} < 11))
do
    #echo ./hos_write_complete mybucket ${test_size[$num]}.data 10000
    #./hos_write_complete mybucket ${test_size[$num]}.data 10000
    echo ./hos_upload_complete mybucket ${test_size[$num]}.data 10000
    ./hos_upload_complete mybucket ${test_size[$num]}.data 10000
    let "num++"
done

