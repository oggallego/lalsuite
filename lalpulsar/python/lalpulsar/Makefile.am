BUILT_SOURCES =
MOSTLYCLEANFILES =
EXTRA_DIST =
include $(top_srcdir)/gnuscripts/lalsuite_python.am
include $(top_srcdir)/gnuscripts/lalsuite_vcs_info.am

vcs_info_sources = git_version.py

if HAVE_PYTHON

pymoduledir = $(pkgpythondir)

pymodule_PYTHON = \
	__init__.py \
	git_version.py \
	lineFileParser.py \
	simulateCW.py \
	NstarTools.py \
	PulsarParametersWrapper.py \
	simulateHeterodynedCW.py \
	knope_utils.py \
	pulsarhtmlutils.py \
	pulsarpputils.py \
	public_sft_directory.py \
	$(END_OF_LIST)

endif
