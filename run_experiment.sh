#! /bin/bash

apppath=./graph_analytics/
datasetpath=/home/xcgoner/lab/jmlr2015/dataset/
resultpath=/home/xcgoner/lab/jmlr2015/result/

nmachines=24
niterations=100


for appname in test_partition test_cc kcore approximate_diameter
do
	for dataset in web-google/web-Google lj2008/lj2008_a wiki/wiki_a arabic-2005/arabic2005_ twitter/twitter_ /uk/uk-union
	do
		for ingress in random random2 grid libra
		do
			mpirun -n $nmachines $apppath$appname --graph=$datasetpath$dataset --graph_opts ingress=$ingress --iterations=$niterations --format=snap  --result_file=$resultpath$appname.txt
		done
		mpirun -n $nmachines $apppath$appname --graph=$datasetpath$dataset --graph_opts ingress=hybrid,threshold=150 --iterations=$niterations --format=snap  --result_file=$resultpath$appname.txt
		
		mpirun -n $nmachines $apppath$appname --graph=$datasetpath$dataset --graph_opts ingress=hybrid_ginger,threshold=150,interval=100,nedges=15,nverts=1 --iterations=$niterations --format=snap  --result_file=$resultpath$appname.txt
		
		mpirun -n $nmachines $apppath$appname --graph=$datasetpath$dataset --graph_opts ingress=zodiac,threshold=96,interval=2000 --iterations=$niterations --format=snap  --result_file=$resultpath$appname.txt
		
		mpirun -n $nmachines $apppath$appname --graph=$datasetpath$dataset --graph_opts ingress=constell,interval=2000 --iterations=$niterations --format=snap  --result_file=$resultpath$appname.txt
	done
done


powerlaw=10000000

for appname in test_partition test_cc kcore approximate_diameter
do
	for alpha in 2.2 2.1 2.0 1.9
	do
		for beta in 2.2 2.1 2.0 1.9
		do
			for ingress in random random2 grid libra
			do
				mpirun -n $nmachines $apppath$appname --powerlaw=$powerlaw --alpha=$alpha --beta=$beta --graph_opts ingress=$ingress --iterations=$niterations --format=snap  --result_file=$resultpath$appname.txt
			done
			mpirun -n $nmachines $apppath$appname --powerlaw=$powerlaw --alpha=$alpha --beta=$beta --graph_opts ingress=hybrid,threshold=150 --iterations=$niterations --format=snap  --result_file=$resultpath$appname.txt
			
			mpirun -n $nmachines $apppath$appname --powerlaw=$powerlaw --alpha=$alpha --beta=$beta --graph_opts ingress=hybrid_ginger,threshold=150,interval=100,nedges=15,nverts=1 --iterations=$niterations --format=snap  --result_file=$resultpath$appname.txt
			
			mpirun -n $nmachines $apppath$appname --powerlaw=$powerlaw --alpha=$alpha --beta=$beta --graph_opts ingress=zodiac,threshold=96,interval=2000 --iterations=$niterations --format=snap  --result_file=$resultpath$appname.txt
			
			mpirun -n $nmachines $apppath$appname --powerlaw=$powerlaw --alpha=$alpha --beta=$beta --graph_opts ingress=constell,interval=2000 --iterations=$niterations --format=snap  --result_file=$resultpath$appname.txt
		done
	done
done


for nmachines in 4 8 12 16 20 24
do
	for appname in test_partition test_cc kcore approximate_diameter
	do
		for dataset in twitter/twitter_ /uk/uk-union
		do
			for ingress in random random2 grid libra
			do
				mpirun -n $nmachines $apppath$appname --graph=$datasetpath$dataset --graph_opts ingress=$ingress --iterations=$niterations --format=snap  --result_file=$resultpath$appname.txt
			done
			mpirun -n $nmachines $apppath$appname --graph=$datasetpath$dataset --graph_opts ingress=hybrid,threshold=150 --iterations=$niterations --format=snap  --result_file=$resultpath$appname.txt
			
			mpirun -n $nmachines $apppath$appname --graph=$datasetpath$dataset --graph_opts ingress=hybrid_ginger,threshold=150,interval=100,nedges=15,nverts=1 --iterations=$niterations --format=snap  --result_file=$resultpath$appname.txt
			
			mpirun -n $nmachines $apppath$appname --graph=$datasetpath$dataset --graph_opts ingress=zodiac,threshold=96,interval=2000 --iterations=$niterations --format=snap  --result_file=$resultpath$appname.txt
			
			mpirun -n $nmachines $apppath$appname --graph=$datasetpath$dataset --graph_opts ingress=constell,interval=2000 --iterations=$niterations --format=snap  --result_file=$resultpath$appname.txt
		done
	done
done