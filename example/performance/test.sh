#########################################################################
# File Name: test.sh
# Author: pxz
# Created Time: Tue 13 Oct 2020 02:28:31 PM CST
#########################################################################
#!/bin/bash
#size=(0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 30 40 50 60 70 80 90 100 110 120 130 140 150 200 300 400 500)
#size=(100 200 300 400 500 600 700 800 900 1k 1500 10k 100k 1M 2M 3M 4M 5M 10M 20M)
size=(1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32)

num=0

while((${num} < 100))
do
    #./HosClientPerformance -a ${size[$num]}
    #echo "./HosClientPerformance -a ${size[$num]} -A -f ./file/100k.data"
    #./HosClientPerformance -a ${size[$num]} -A -f ./file/100k.data
    #echo ./HosClientPerformance -f ./file/${size[$num]}.data -F
    #./HosClientPerformance -f ./file/${size[$num]}.data -F
    #echo ./HosClientPerformance -a 100 -A -f ./file/100k.data -t ${size[$num]} -S 1024
    #./HosClientPerformance -a 100 -A -f ./file/100k.data -t ${size[$num]} -S 1024
    #echo ./HosClientPerformance -a 100 -A -f ./file/100k.data -S ${size[$num]} -t 32
    #./HosClientPerformance -a 100 -A -f ./file/100k.data -S ${size[$num]} -t 32
    echo ./HosClientPerformance -P1 -t32
    ./HosClientPerformance -P1 -t32
    let "num++"
done
