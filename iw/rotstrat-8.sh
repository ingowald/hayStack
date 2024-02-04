#!/bin/bash
mm && cp hsViewerQT /cluster/ && for f in shady moggy wally ; do rsync -av /cluster/* $f:/cluster; done  &&  /home/wald/opt/bin/mpirun -n 8 -host trinity:2,shady:2,moggy:2,wally:2 /cluster/hsViewerQT -ndg 8 raw://8@/cluster/rotstrat_temperature_4096x4096x4096_float32.raw:format=float:dims=4096,4096,4096
#-xf /cluster/rotstrat_temperature.xf
