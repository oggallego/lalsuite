BUILT_SOURCES =
EXTRA_DIST =
MOSTLYCLEANFILES =
include $(top_srcdir)/gnuscripts/lalapps.am
include $(top_srcdir)/gnuscripts/lalsuite_python.am
include $(top_srcdir)/gnuscripts/lalsuite_help2man.am

BURSTPROGS = lalapps_binj
lalapps_binj_SOURCES = binj.c
if LALINSPIRAL
INSPPROGS = lalapps_power
lalapps_power_SOURCES = power.c
endif

bin_PROGRAMS = $(BURSTPROGS) $(INSPPROGS)

if HAVE_PYTHON
pybin_scripts = \
	lalapps_cafe \
	lalapps_power_likelihood_pipe \
	lalapps_power_pipe \
	$(END_OF_LIST)
endif

pkgdata_DATA = power_pipe.ini

EXTRA_DIST += \
	H1_L1_dt125_df128.png \
	Z_histogram.png \
	clustering_example.png \
	conditioning.png \
	freqdomainfilter.png \
	h_histogram.png \
	power_pipeline.png \
	plotbinj_L1_4.png \
	plotbinj_L1_5.png \
	plotburca2-P-793154128-2525061.png \
	power.dox \
	power_pipe.ini \
	psds.png \
	sk_histogram.png \
	sksk.png \
	tiles_histogram.png \
	tiles_histogram_adjusted.png \
	timedomainfilter_04hz.png \
	timedomainfilter_16hz.png \
	windows.png \
	$(END_OF_LIST)

MOSTLYCLEANFILES += \
	$(END_OF_LIST)
