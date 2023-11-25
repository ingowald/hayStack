#!/bin/bash
common='./hsViewerQT ~/models/unstructured/lander-small/surface.obj ~/per-rank/lander-small-vort_mag-9000.*umesh  -ndg 2 -xf ~/models/unstructured/lander-small/lander-small-2.xf   `cat ~/models/unstructured/lander-small/lander-small-2.cam` --measure'

for threshold in 2 4 8 12; do
    echo "make -C bin$threshold" 
    echo rm "bin$threshold/lander-*png"
    echo rm "bin$threshold/out-lander-*txt"
    for mum in "-mum" "--no-mum"; do

	echo "BARNEY_UMESH=macro-cells bin$threshold/$common $mum 2>&1 | tee bin$threshold/out-lander-$mum-mc.txt"
	echo "mv hayStack.png bin$threshold/lander-$mum-mc.png"
	
	echo "BARNEY_UMESH=object-space bin$threshold/$common $mum 2>&1 | tee bin$threshold/out-lander-$mum-os.txt"
	echo "mv hayStack.png bin$threshold/lander-$mum-os.png"
	
	for awtDepth in 12 10 8 6 4 2; do
	    echo "BARNEY_UMESH=awt AWT_MAX_DEPTH=$awtDepth bin$threshold/$common $mum 2>&1 | tee bin$threshold/out-lander-$mum-awt$awtDepth.txt"
	    echo "mv hayStack.png bin$threshold/lander-$mum-awt$awtDepth.png"
	done
    done
done
    


common='./hsViewerQT ~/barney/exajet-2_*umesh  -ndg 2 -xf ~/barney/exajet-teaser.xf    `cat ~/barney/exajet.cam` --measure'

for threshold in 2 4 8 12; do
    echo "make -C bin$threshold" 
    echo rm "bin$threshold/exajet-*png"
    echo rm "bin$threshold/out-exajet-*txt"
    for mum in "--no-mum"; do

	echo "BARNEY_UMESH=macro-cells bin$threshold/$common $mum 2>&1 | tee bin$threshold/out-exajet-$mum-mc.txt"
	echo "mv hayStack.png bin$threshold/exajet-$mum-mc.png"
	
	echo "BARNEY_UMESH=object-space bin$threshold/$common $mum 2>&1 | tee bin$threshold/out-exajet-$mum-os.txt"
	echo "mv hayStack.png bin$threshold/exajet-$mum-os.png"
	
	for awtDepth in 12 10 8 6 4 2; do
	    echo "BARNEY_UMESH=awt AWT_MAX_DEPTH=$awtDepth bin$threshold/$common $mum 2>&1 | tee bin$threshold/out-exajet-$mum-awt$awtDepth.txt"
	    echo "mv hayStack.png bin$threshold/exajet-$mum-awt$awtDepth.png"
	done
    done
done
    
