#!/usr/bin/env python
import sys,os, re, logging, argparse

from pywrap.wrapper import Wrapper
from pywrap import containers, namespaces, matchers, classes
from pyplusplus.module_builder import call_policies

wrap = Wrapper()
mb = wrap.mb

# Collect namespaces
ns_std      = mb.namespace('::std')
ns_sthal    = mb.namespace('::sthal')
included_ns = [ns_sthal]
already_exposed_ns = [ mb.namespace('::HMF') ]

for ns in ['::boost::serialization', '::boost::archive']:
    try:
        mb.namespace(ns).exclude()
    except matchers.declaration_not_found_t: pass

for ns in already_exposed_ns:
    try:
        ns.exclude()
    except matchers.declaration_not_found_t: pass

# Special fix up
containers.extend_std_containers(mb)
namespaces.include_default_copy_constructors(mb)

list_of_configurators = []

for ns in included_ns:
    ns.include()
    namespaces.extend_array_operators(ns)

    for c in ns.classes(allow_empty=True):
        c.include()
        if c.name.endswith("Configurator") or c.name=="ReadFloatingGates":
            list_of_configurators.append(c.name)
            classes.add_omp_safe_virtual_functions(c, allow_empty=True)
        # propagate "explictness" to python :)
        c.constructors(lambda c: c.explicit == True, allow_empty=True).allow_implicit_conversion = False

        classes.add_comparison_operators(c)

f = ns_sthal.class_("Settings").member_function("get")
f.call_policies = call_policies.custom_call_policies("bp::return_value_policy<bp::reference_existing_object>")

cls = ns_sthal.class_("AnalogRecorder")
for f_name in ["trace", "traceRaw", "getTimestamps" ]:
    f = cls.member_function(f_name)
    f.call_policies = call_policies.custom_call_policies(
        "::pywrap::ReturnNumpyPolicy", "pywrap/return_numpy_policy.hpp")

cls = ns_sthal.class_("HICANN")
for f_name in ("receivedSpikes", "sentSpikes"):
    f = cls.member_function(f_name)
    f.call_policies = call_policies.custom_call_policies(
        "::pywrap::ReturnNumpyPolicy", "pysthal/return_numpy_policy.hpp")

for cls in ['SynapseProxy', 'SynapseRowProxy']:
    c = ns_sthal.class_(cls)
    for var in c.variables():
        var.getter_call_policies = call_policies.return_internal_reference()

for cls in ['Wafer', 'HICANN', 'HICANNData', 'DNC', 'FPGA', 'Status', 'ADCChannel', 'Spike',
            'SynapseArray', 'FGStimulus', 'FloatingGates', 'FGConfig']:
    c = ns_sthal.class_(cls)
    classes.add_pickle_suite(c)

for cls in ['vector<sthal::Spike>']:
    c = ns_std.class_(cls)
    classes.add_pickle_suite(c)
    c.add_registration_code('def(bp::self_ns::str(bp::self_ns::self))')

# Because e.g. HICANNData does not provide operator<
class_names = ["ParallelHICANNv4Configurator", "HICANNConfigurator"]
for name in class_names:
    for td in ns_sthal.class_(name).typedefs(allow_empty=True):
        cls = td.target_decl
        container = cls.indexing_suite
        if container is None:
            continue

        element_type = container.element_type
        if not element_type.decl_string.startswith("::boost::shared_ptr"):
            continue

        container.disable_methods_group("reorder")

for td in ns_sthal.typedefs():
    decl = getattr(td.type, "declaration", None)
    if decl is not None and decl.ignore:
        td.exclude()

# Add python import callback
mb.add_registration_code(
    'bp::import("pysthal_patch").attr("patch")(bp::scope());')

# somehow the typedefs in sthal::Status had ignore=True,
# force include them here as a workaround
for cls in ['Status']:
    c = ns_sthal.class_(cls)
    for td in c.typedefs():
        td.target_decl.include()

for cls in ['ParallelHICANNv4Configurator']:
    f = matchers.access_type_matcher_t('public')
    for td in ns_sthal.class_(cls).typedefs(f):
        td.target_decl.include()
        td.include()

for cls in list_of_configurators:
    # in case the configurator is not a trivially constructed one, we need to
    # pass the parameters through here
    if cls not in ["HICANNReadoutConfigurator"]:
        custom_create = 'def("__init__", boost::python::make_constructor(+[]() { return boost::make_shared<%s_wrapper>(); }))'%cls
    elif cls == "HICANNReadoutConfigurator":
        custom_create = 'def("__init__", boost::python::make_constructor(+[](sthal::Wafer& wafer, std::vector< ::halco::hicann::v2::HICANNOnWafer> read_hicanns) { return boost::make_shared<%s_wrapper>(wafer, read_hicanns); }))'%cls
    c = ns_sthal.class_(cls)
    c.add_registration_code(custom_create)



# expose only public interfaces
namespaces.exclude_by_access_type(mb, ['variables', 'calldefs', 'classes', 'typedefs'], 'private')
namespaces.exclude_by_access_type(mb, ['variables', 'calldefs', 'classes', 'typedefs'], 'protected')

wrap.set_number_of_files(10)  # 10 is more or less randomly choosen:It is about twice as fast as 0.
                              # 20 doesn't reduce the build time further. I tested with release_with_sanitize

wrap.finish()
