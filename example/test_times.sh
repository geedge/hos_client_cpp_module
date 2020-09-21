#########################################################################
# File Name: test_times.sh
# Author: pxz
# Created Time: Mon 21 Sep 2020 04:43:35 PM CST
#########################################################################
#!/bin/bash
test_times=(1, 10, 100, 1000, 10000)
num=0

while((${num} < 5))
do
    ./singleThread mybucket my-file.txt ${test_times[$num]}
    let "num++"
done

