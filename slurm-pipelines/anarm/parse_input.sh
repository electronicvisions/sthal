#!/bin/bash -e
# from https://stackoverflow.com/questions/192249/how-do-i-parse-command-line-arguments-in-bash
# - eval is used to strip single parentheses from SP_ORIGINAL_ARGS (slurm-pipeline variable)
# - slurm-pipeline currently only accepts --scriptArgs parameters in the form "parameter=<value>"
#    (i.e. without hyphen, e.g. "-w=20" is not accepted)

for arg in $SP_ORIGINAL_ARGS
do
eval arg=$arg
case $arg in
    t=*|tag=*)
    TAG="${arg#*=}"
    ;; 
    w=*|wafer=*)
    WAFER="${arg#*=}"
    ;;
    d=*|dnc=*)
    DNC="${arg#*=}"
    ;;
    o=*|outdir=*)
    OUTDIR="${arg#*=}"
    ;;
    hwdb=*)
    HWDB="${arg#*=}"
    ;;
    skip-trigger-test)
    SKIP_TRIGGER_TEST=true
    ;;
esac
done

if [ -z ${TAG} ] ; then
    echo "TAG is needed (string)"
    exit 3
fi
if ! [[ ${WAFER} -gt -1 && -n ${WAFER} ]] ; then
    echo "WAFER is wrong (-w x, x in [0,inf))"
    exit 3
fi
if ! [[ ${DNC} -gt -1 && ${DNC} -lt 48 && -n ${DNC} ]] ; then
    echo "DNC is wrong (-d x, x in [0,47])"
    exit 3
fi
if [ -z ${HWDB} ]; then
    export HWDB="/wang/data/bss-hwdb/db.yaml" 
fi
if [ -z ${OUTDIR} ]; then
    echo "OUTDIR is needed (--outdir "path/to/dir")"
    exit 3
fi
