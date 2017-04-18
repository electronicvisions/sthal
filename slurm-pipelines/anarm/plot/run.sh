#!/bin/bash -e

# parse input arguments and set variables
. ../parse_input.sh $@

PLOT_SCRIPT=sthal_wafer_AnaRM_overview_plot
WANG_RESULT_DIR="${OUTDIR}/${TAG:-test}"

#Obtain sthal's tools path where plot script is located
pythonpaths=$(echo $PYTHONPATH | tr ":" "\n")
for toolpaths in $pythonpaths
do
   if [[ $toolpaths == *"/lib"* ]]; then
      toolpath="${toolpaths/\/lib/\/bin}"
   fi
   done

if [ -z "${toolpath}" ]; then
   echo "A sthal project should be present in PYTHONPATH"
   exit 7
fi

singularity exec --app $CONTAINER_APP_NMPM_SOFTWARE $CONTAINER_IMAGE_NMPM_SOFTWARE \
   python "${toolpath}"/"${PLOT_SCRIPT}" \
   --data-output-dir "${WANG_RESULT_DIR}"/data

# Publish results to wang
ln -sf "${WANG_RESULT_DIR}" "${OUTDIR}/latest"
