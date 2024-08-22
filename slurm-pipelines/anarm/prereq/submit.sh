#!/bin/bash -e

# parse input arguments and set variables
. ../parse_input.sh $@

task=prereq-${TAG}
name=$task-w${WAFER}d${DNC}

log_prefix=submit_w${WAFER}_d${DNC}_run
log=../${log_prefix}.log
exec > >(tee -a $log)
exec 2>&1

prefix=$name

if ! mkdir -p $OUTDIR ; then
	echo "cannot create workspace $OUTDIR"
	exit 1
fi

if [[ ! -w $OUTDIR ]]; then
	echo "workspace $OUTDIR is not writable"
	exit 1
fi

# OUTDIR could be created, so we can move the submission log
mv $log ${OUTDIR}/${log_prefix}.log
log=${OUTDIR}/${log_prefix}.log
exec > >(tee -a $log)

if [[ -z $CONTAINER_APP_NMPM_SOFTWARE || -z $CONTAINER_IMAGE_NMPM_SOFTWARE ]]; then
    echo "Both CONTAINER_APP_NMPM_SOFTWARE and CONTAINER_IMAGE_NMPM_SOFTWARE must be set"
    exit 6
fi

echo "$prefix: prereq/submit.sh running at `date -u`"
if [[ ! -z "$SP_DEPENDENCY_ARG" ]]; then
	echo "$prefix: dependencies are $SP_DEPENDENCY_ARG"
fi

if [[ $SP_SIMULATE = "1" ]]; then
	echo "$prefix: this is a simulation"
	echo
	echo "TASK: $task"
	exit 0
fi

export SINGULARITYENV_PREPEND_PATH=$PATH

jobid=$(sbatch --parsable --time=00:02:00 -o "${OUTDIR}/slurm_${name}_run.log" -p batch -J ${name} --skip-hicann-init \
                           --kill-on-invalid-dep=yes ${SP_DEPENDENCY_ARG} \
                           --wrap "singularity exec --app $CONTAINER_APP_NMPM_SOFTWARE $CONTAINER_IMAGE_NMPM_SOFTWARE ./run.sh $SP_ORIGINAL_ARGS")
if [[ -z "${jobid}" ]]; then
	echo "${prefix}: submission failed"
	exit 1
fi
echo "${prefix}: submitted job ${jobid}"
echo "TASK: ${task} ${jobid}"
echo
