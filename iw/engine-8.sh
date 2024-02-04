#!/bin/bash
mm && cp hsViewerQT /cluster/ && for f in shady moggy wally ; do rsync -av /cluster/* $f:/cluster; done  &&  /home/wald/opt/bin/mpirun -n 8 -host trinity:2,shady:2,moggy:2,wally:2 /cluster/hsViewerQT raw://8@/cluster/engine_256x256x128_uint8.raw:format=uint8:dims=256,256,128 -xf /cluster/engine.xf --camera 316.657 220.631 67.4258 128.608 103.132 38.6924 0 0 1 -fovy 60 -ndg 8
