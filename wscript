#!/usr/bin/env python
import sys, os, copy
from subprocess import check_output, CalledProcessError
from waflib import Utils
from waflib.extras import symwaf2ic
from waflib.extras.gtest import summary

APPNAME='sthal'

def depends(ctx):
    ctx('halbe')
    ctx('calibtic')
    ctx('redman')
    ctx('logger')
    ctx('hwdb')

def options(opt):
    opt.load('compiler_cxx')
    opt.load('boost')
    opt.load('gtest')
    opt.load('doxygen')


def configure(cfg):
    cfg.load('compiler_cxx')
    cfg.load('boost')
    cfg.load('gtest')
    cfg.load('doxygen')

    cfg.check_cxx(lib='rt', uselib_store='RT')
    cfg.check_cxx(lib='gomp', cxxflags='-fopenmp', linkflags='-fopenmp', uselib_store='OPENMP4STHAL')
    cfg.check_cxx(lib='tbb', uselib_store='TBB4STHAL', mandatory=1)


def build(bld):

    bld.add_post_fun(summary)


    bld(
        target          = 'sthal_inc',
        use             = [
            'halbe_coordinate_inc',
            'halbe_container_inc',
            'halbe_handle_inc',
            'calibtic_inc',
            'redman_inc',
            'hwdb4cpp_inc',
        ],
        export_includes = ['.'],
    )

    sthal_post = ['calibtic_xml', 'redman', 'redman_xml']

    # always re-check git version, user could have changed stuff at arbitrary times
    try:
        cmd = [bld.env.get_flat('GIT'), 'describe', '--long', '--dirty', '--always', '--abbrev=32']
        git_version = check_output(cmd, cwd=bld.path.abspath()).rstrip()
    except CalledProcessError:
        git_version = "unversioned_build"

    bld(
        rule = 'echo \'char const * STHAL_GIT_VERSION = "%s";\' > ${TGT}' % git_version,
        target = 'sthal_git_version.h',
        export_includes = [bld.path.get_bld()],
       )

    sthal_sources = bld.path.ant_glob('sthal/**/*.cpp', excl='sthal/**/*ESS*.cpp sthal/**/*Ess*.cpp')
    if bld.env.build_ess:
        sthal_sources += bld.path.ant_glob('sthal/**/*ESS*.cpp sthal/**/*Ess*.cpp')
    else:
        # ESSConfig is always needed by PyMarocco
        sthal_sources += bld.path.ant_glob('sthal/ESSConfig.cpp')

    datadir = os.path.abspath(os.path.join(bld.options.prefix, 'share'))
    datadirsrc = bld.path.find_dir('share')
    bld.install_files(
        datadir,
        files=datadirsrc.ant_glob('**/*'),
        cwd=datadirsrc,
        relative_trick=True,
    )

    bld.shlib(
        target          = 'sthal',
        features        = 'pyembed post_task',
        source          = sthal_sources,
        export_includes = ['.'],
        install_path    = '${PREFIX}/lib',  # avoid lib/lib64 problems
        use             = [
            'sthal_inc',
            'halbe',
            'hmf_calibration',
            'sthal_git_version.h',
            'hwdb4cpp',
            'TBB4STHAL',
            'redman'
        ],
        post_task       = sthal_post,
        defines         = ['DATADIR="{}"'.format(datadir)],
    )

    bld(
        target       = 'sthal_tests',
        features     = 'gtest cxx cxxprogram pyembed',
        source       =  bld.path.ant_glob('tests/**/sthal_test_*.cpp'),
        use          =  ['sthal', 'logger_obj', 'hwtest_obj', 'RT'],
        install_path='${PREFIX}/bin',
        test_environ = {
            'NMPM_DATADIR': datadirsrc.abspath(),
        },
    )

    bld(
        target        = 'sthal_multi_fpga_loopback_hwtest',
        features      = 'cxx cxxprogram pyembed',
        source        = 'tests/sthal_multi_fpga_loopback_hwtest.cpp',
        use           = ['sthal', 'logger_obj', 'hwtest_obj', 'RT'],
        install_path  = '${PREFIX}/bin',
    )


    bld(
        target       = 'sthal_hwtests',
        features     = 'cxx cxxprogram pyembed gtest',
        source       = bld.path.ant_glob('tests/**/sthal_hwtest_*.cpp'),
        use          =  ['sthal', 'logger_obj', 'hwtest_obj', 'RT', 'GTEST'],
        install_path ='${PREFIX}/bin',
        skip_run     = True,
    )

    if bld.env.build_ess:
        bld(
            target       = 'run_waferdump_in_ess',
            features     = 'pyembed cxx cxxprogram',
            source       = 'tools/run_waferdump_in_ess.cpp',
            use          = ['sthal', 'BOOST4TOOLS'],
            install_path='${PREFIX}/bin',
        )

    bld.install_files(
        '${PREFIX}/bin',
        bld.path.ant_glob('tools/*', excl='tools/*.cpp'),
        relative_trick=False,
        chmod=Utils.O755,
    )
    bld.symlink_as('${PREFIX}/bin/cube_sbatch', 'cube_srun')

    if bld.env.build_python_bindings:
        bld(
                target         = '_pysthal',
                features       = 'cxx cxxshlib pypp pyembed pyext post_task',
                script         = 'pysthal/generate.py',
                gen_defines    = 'PYPLUSPLUS __STRICT_ANSI__',
                defines        = 'PYBINDINGS',
                headers        = 'pysthal/pysthal.h',
                use            = ['sthal', 'pyhalbe', 'pyhwdb'],
                install_path   = '${PREFIX}/lib',
                post_task      = ['pysthal_tests'],
                cxxflags       = [
                    '-Wno-unused-local-typedefs',
                    '-fvisibility=hidden',
                ],
        )

        bld(
                target          = 'pysthal',
                features        = 'py post_task',
                source          = bld.path.ant_glob('pysthal/pysthal/**/*.py'),
                post_task       = ['_pysthal'],
                install_from    = 'pysthal',
                install_path    = '${PREFIX}/lib'
        )

        bld.install_files(
                '${PREFIX}/lib',
                [ 'pysthal/pysthal_patch.py', 'tests/PysthalTest.py' ],
                relative_trick=False
        )

        bld(
            name            = "pysthal_tests",
            tests           = bld.path.ant_glob('tests/sthal_test_*.py'),
            features        = 'use pytest',
            use             = ['pysthal', '_pysthal', 'pyhalbe', '_pyhalbe',
                               'pylogging', 'pyhalbe_tests'],
            install_path    = '${PREFIX}/bin',
            test_timeout    = 45,
            prepend_to_path = ["tools"],
            pythonpath      = ["pysthal", "tests"],
            test_environ    = {
                'NMPM_DATADIR': datadirsrc.abspath(),
            },
        )

        bld.install_files(
            '${PREFIX}/bin',
            bld.path.ant_glob('tests/sthal_hwtest_*.py'),
            relative_trick=False,
            chmod=Utils.O755,
        )

    bld.add_post_fun(summary)


def doc(dox):
    '''build documentation (doxygen)'''
    dox(features  = 'doxygen',
        doxyfile  = 'doc/doxyfile',
        pars={ 'INPUT': dox.path.get_src().abspath() + '/sthal', })
