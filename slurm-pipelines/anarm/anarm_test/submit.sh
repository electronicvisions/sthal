#!/bin/bash -e

# parse input arguments and set variables
. ../parse_input.sh $SP_ORIGINAL_ARGS

task=anarm_test-${TAG}
name=$task-w${WAFER}d${DNC}
prefix=$name

log=../anarm_test-${TAG}.log
exec > >(tee -a $log)
exec 2>&1

echo "$prefix: anarm_test/submit.sh running at `date -u`" >> $log
if [[ ! -z "$SP_DEPENDENCY_ARG" ]]; then
	echo "$prefix: dependencies are $SP_DEPENDENCY_ARG" >> $log
fi

if [[ $SP_SIMULATE = "1" ]]; then
    echo "$prefix: this is a simulation" >> $log
	echo >> $log
	echo "TASK: $task"
	exit 0
fi

export SINGULARITYENV_PREPEND_PATH=$PATH
echo "SINGULARITYENV_PREPEND_PATH: " $SINGULARITYENV_PREPEND_PATH
echo "SP_DEPENDENCY_ARG: " $SP_DEPENDENCY_ARG

jobid=$(sbatch --parsable --nice=50 -o ${OUTDIR}/$name.log -p calib -c1 --skip-hicann-init \
    --wmod $WAFER --reticle-with-aout ${DNC} -J ${name} --kill-on-invalid-dep=yes ${SP_DEPENDENCY_ARG} --time=00:30:00 \
    --wrap "singularity exec --app $CONTAINER_APP_NMPM_SOFTWARE $CONTAINER_IMAGE_NMPM_SOFTWARE \
    ./run.sh $SP_ORIGINAL_ARGS")
if [[ -z "${jobid}" ]]; then
	echo "${prefix}: submission failed" >> ${log}
	exit 1
fi
echo "${prefix}: submitted job ${jobid}" >> ${log}
echo >> ${log}

echo "TASK: ${task} ${jobid}"
