#!/bin/bash
duration=1
static_workloads=('datamining' 'enterprise')
dynamic_workloads=('uniform' 'pareto')
log_dir="/mnt/log-new/k7n12/"

k=7
n=12
for i in $(seq 0.5 0.5 1.0)
do
    #break
    for l in "${dynamic_workloads[@]}"
    do
        for j in 0.00 0.01 0.02 0.03 0.04 0.05
        do
            for f in 10000 50000 100000
            do
            # for k in 3 7 24
            # do
            # for n in 5 12 28
            # do
            echo "$duration $i $l $j $f $k $n"
            /users/cs740-proj/EC_offloading/htsim --expt=4 --memlimit=0 --exectime=0 --duration=$duration --utilization=$i --flowdist=$l --ecchance=$j --flowsize=$f 2>&1 > "$log_dir"ec_"$i"_"$l"_"$j"_"$f"_k"$k"_n"$n".log &
            /users/cs740-proj/EC_offloading/htsim --expt=5 --memlimit=0 --exectime=0 --duration=$duration --utilization=$i --flowdist=$l --ecchance=$j --flowsize=$f 2>&1 > "$log_dir"ec_offload_"$i"_"$l"_"$j"_"$f"_k"$k"_n"$n".log &
            # break
        # done
        # done
        done
        done
        # break
    done
    # break
    sleep 360
done
sleep 5400
pkill -f htsim
for i in $(seq 0.5 0.5 1.0)
do
    #break
    for l in "${static_workloads[@]}"
    do
        for j in 0.00 0.01 0.02 0.03 0.04 0.05
        do
            
            # for k in 3 7 24
            # do
            # for n in 5 12 28
            # do
            echo "$duration $i $l $j $f $k $n"
            /users/cs740-proj/EC_offloading/htsim --expt=4 --memlimit=0 --exectime=0 --duration=$duration --utilization=$i --flowdist=$l --ecchance=$j 2>&1 > "$log_dir"ec_"$i"_"$l"_"$j"_k"$k"_n"$n".log &
            /users/cs740-proj/EC_offloading/htsim --expt=5 --memlimit=0 --exectime=0 --duration=$duration --utilization=$i --flowdist=$l --ecchance=$j 2>&1 > "$log_dir"ec_offload_"$i"_"$l"_"$j"_k"$k"_n"$n".log &
            # break
        # done
        # done
        done
        # break
    done
    # break
    sleep 360
done
sleep 5400
pkill -f htsim