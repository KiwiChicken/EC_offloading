#!/bin/bash
duration=1
static_workloads=('datamining' 'enterprise')
dynamic_workloads=('uniform' 'pareto')
log_dir="/mydata/log-ns-lat/"

for i in $(seq 0.1 0.1 1.0)
do
    # break
    for f in 50000 100000 10000000
    do
        for l in "${dynamic_workloads[@]}"
        do
            echo "$i $l $f"
            /users/cs740-proj/EC_offloading/htsim --expt=4 --duration=$duration --flowdist=$l --utilization=$i --flowsize=$f 2>&1 > "$log_dir"ec_"$i"_"$l"_"$f".log &
            /users/cs740-proj/EC_offloading/htsim --expt=5 --duration=$duration --flowdist=$l --utilization=$i --flowsize=$f 2>&1 > "$log_dir"ec_offload_"$i"_"$l"_"$f".log &
            #./htsim --expt=2 --duration=$duration --flowdist=$l --utilization=$i --flowsize=$f 2>&1 > "$log_dir"conga_"$i"_"$l"_"$f".log &
            #./htsim --expt=4 --duration=$duration --flowdist=$l --utilization=$i --flowsize=$f 2>&1 > "$log_dir"ecmp_"$i"_"$l"_"$f".log &
            # break
        done
        # break
    done
    # break
    #sleep 36000
done
# sleep 3600
# for i in $(seq 0.1 0.1 1.0)
# do
#     for l in "${static_workloads[@]}"
#     do
#         echo "$i $l"
#         /users/cs740-proj/EC_offloading/htsim --expt=4 --duration=$duration --flowdist=$l --utilization=$i 2>&1 >  "$log_dir"ec_"$i"_"$l"_"$f".log &
#         /users/cs740-proj/EC_offloading/htsim --expt=5 --duration=$duration --flowdist=$l --utilization=$i 2>&1 > "$log_dir"ec_offload_"$i"_"$l"_"$f".log &
#         #./htsim --expt=2 --duration=$duration --flowdist=$l --utilization=$i --flowsize=$f 2>&1 > "$log_dir"conga_"$i"_"$l"_"$f".log &
#         #./htsim --expt=4 --duration=$duration --flowdist=$l --utilization=$i --flowsize=$f 2>&1 > "$log_dir"ecmp_"$i"_"$l"_"$f".log &
#         # break
#     done
#     # break
#     #sleep 36000
# done