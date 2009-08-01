"""
Classes needed for ihope.
"""

__author__ = 'Stephen Fairhurst <sfairhur@gravity.phys.uwm.edu>'
__date__ = '$Date$'
__version__ = '$Revision$'[11:-2]

##############################################################################
# import standard modules and append the lalapps prefix to the python path
import os, sys, copy, shutil, glob
import ConfigParser
import optparse
import tempfile
import urllib
import subprocess

##############################################################################
# import the modules we need to build the pipeline
from glue import segments
from glue import segmentsUtils
from glue import pipeline
from glue import lal

##############################################################################
# Functions used in setting up the dag:
def make_external_call(command, show_stdout=False, show_command=False):
  """
  Run a program on the shell and print informative messages on failure.
  """
  if show_command: print command

  stdin, out, err = os.popen3(command)
  pid, status = os.wait()

  if status != 0:
      print >>sys.stderr, "External call failed."
      print >>sys.stderr, "  status: %d" % status
      print >>sys.stderr, "  stdout: %s" % out.read()
      print >>sys.stderr, "  stderr: %s" % err.read()
      print >>sys.stderr, "  command: %s" % command
      sys.exit(status)
  if show_stdout:
      print out.read()
  stdin.close()
  out.close()
  err.close()

##############################################################################
def mkdir( newdir ):
  """
  Create a directory, don't complain if it exists

  newdir = name of directory to be created
  """
  if os.path.isdir(newdir): pass
  elif os.path.isfile(newdir):
    raise OSError("a file with the same name as the desired " \
                  "dir, '%s', already exists." % newdir)
  else: os.mkdir(newdir)

##############################################################################
def hipe_cache(ifos, usertag, gps_start_time, gps_end_time):
  """
  return the name of the hipe cache 

  ifos    = list of ifos
  usertag = usertag
  gps_start_time = start time of analysis
  gps_end_time   = end time of analysis
  """
  hipeCache = "".join(ifos) + "-INSPIRAL_HIPE" 
  if usertag:
    hipeCache += "_" + usertag 
  hipeCache += "-" + str(gps_start_time) + "-" + \
      str(gps_end_time - gps_start_time)  + ".cache"

  return hipeCache

##############################################################################
def hipe_pfn_cache(cachename,globpat):
  """
  create and return the name of a pfn cache containing files that match
  globpat. This is needed to manage the .input files that hipe creates.
  
  cachename = the name of the pfn cache file
  globpat = the pattern to search for
  """
  cache_fh = open(cachename,"w")
  for file in glob.glob(globpat):
    lfn = os.path.basename(file)
    pfn = "file://" + os.path.join(os.getcwd(),file)
    print >> cache_fh, ' '.join([lfn,pfn,' pool="local"'])
  cache_fh.close()
  return cachename

##############################################################################
def tmpltbank_cache(datafind_filename):
  """
  open and read the datafind hipe cache, find only the tmpltbank files
  return a lal.Cache class with only the tmpltbanks

  datafind_filename = datafind cache filename relative to current path
  """
  cache_file = open(datafind_filename)
  result_cache = lal.Cache.fromfile(cache_file)
  cache_file.close()

  return result_cache.sieve(description="TMPLTBANK")

##############################################################################
def symlink_tmpltbank(tmpltbank_cache, user_tag):
  """
  Symlinks the tmpltbank files from the datafind directory into the current
  directory
  Returns 

  tmpltbank_cache = lal.Cache of the tmpltbank files in the datafind dir
  usertag = playground, full_data, or the name of the injection directory
  """
  new_dir = user_tag.lower()
  file_tag = user_tag.upper()

  for entry in tmpltbank_cache:
    old_file = os.path.basename(entry.path())
    new_file = old_file.replace("DATAFIND",file_tag)
    try: # Remove file if it already exists
      os.remove(new_file)
    except: pass
    os.symlink("../datafind/" + old_file, new_file)

  new_pfnlist = [url.replace("datafind",new_dir).replace("DATAFIND",file_tag)
      for url in tmpltbank_cache.pfnlist()]

  return lal.Cache.from_urls(new_pfnlist)

##############################################################################
# Function to set up the segments for the analysis
def science_segments(ifo, config, generate_segments = True):
  """
  generate the segments for the specified ifo
  @param ifo:    the name of the ifo
  @param config: the configParser object with analysis details
  @param generate_segments: whether or not to actually make segments
  """
  start = config.get("input","gps-start-time")
  end = config.get("input","gps-end-time")

  segFindFile = ifo + "-SCIENCE_SEGMENTS-" + start + "-" + \
      str(int(end) - int(start))

  segFindXML = segFindFile + ".xml"
  segFindFile = segFindFile + ".txt"

  # if not generating segments, all we need is the name of the segment file
  if not generate_segments: return segFindFile

  whichtoanalyze = ifo.lower() + "-analyze"
  print "For " + ifo + ", analyze " +config.get("segments",whichtoanalyze)
  
  # run segFind to determine science segments
  segFindCall = ' '.join([ config.get("condor", "segfind"),
	"--query-segments",
	"--segment-url", config.get("segfind", "segment-url"),
	"--gps-start-time", start,
	"--gps-end-time", end,
	"--include-segments",
	   ''.join([ '"', config.get("segments", whichtoanalyze), '"' ]),
        "--output-file", segFindXML ])
  make_external_call(segFindCall)

  segsToTxtCall = ' '.join([ config.get("condor", "ligolw_print"),
        "--table segment --column start_time --column end_time",
        "--delimiter ' '",
        segFindXML,
        ">", segFindFile ])
  make_external_call(segsToTxtCall)

  return segFindFile

##############################################################################
# Function to set up the veto-category xml files from the vetoDefFile
def generate_veto_cat_files(config, vetoDefFile, generateVetoes):
  """
  Generate veto category xml files for each ifo using the
  veto-definer file.
  @param config: the configParser object with analysis details
  @param vetoDefFile: veto-definer xml file 
  @param generateVetoes: generate the veto files
  """
  executable = config.get("condor", "segs_from_cats")
  start = config.get("input", "gps-start-time")
  end = config.get("input", "gps-end-time")

  genVetoCall = executable
  genVetoCall = ' '.join([ genVetoCall, "--cumulative-categories", 
	"--segment-url", config.get("segfind", "segment-url"),
	"--veto-file", vetoDefFile,
	"--gps-start-time", start,
	"--gps-end-time", end ])
  
  if generateVetoes:
    print "Generating veto category xml files... this may take some time`..."
    make_external_call(genVetoCall)

##############################################################################
# Function to convert a veto-category xml file to a txt file
def convert_veto_cat_xml_to_txt(config, veto_cat_file, output_file):
  """
  Converts a veto-category xml file into a veto-segments txt file.
  Note: if the veto_cat_file doesn't exist, or if there are no
  vetoes in that veto_cat_file, the output file is still written;
  it just will contain the header with no segments listed.

  @param config: the configParser object with analysis details
  @param veto_cat_file: name (and loction) of veto_cat_file to convert
  @param output_file: name (and location) of output txt file to save to
  """
  print "Converting veto-category xml file %s to txt file %s" \
	%(veto_cat_file, output_file)
  sys.stdout.flush()
  # get xml-to-txt converter
  xml_to_txt_converter = config.get("condor", "ligolw_print")

  convertCall = ' '.join([ xml_to_txt_converter,
	"-t segment -c start_time -c end_time", veto_cat_file ])
  tempfile = os.popen(convertCall)
  
  # get times from tempfile
  times = tempfile.readlines()

  tempfile.close()

  if times:
    vetoes_available = True
  else:
    vetoes_available = False

  # open output file for writing
  file = open(output_file, "w")
  file.write("# seg\tstart    \tstop     \tduration\n")
  
  for n,line in enumerate(times):
    (start,end) = line.split(',')
    duration = str(int(end) - int(start))
    newline = "\t".join([ str(n), start, end.strip(), duration ])
    file.write( newline + "\n" )

  file.close()

  return vetoes_available

##############################################################################
# Function to set up the segments for the analysis
def veto_segments(ifo, config, categories, generateVetoes): 
  """
  generate veto segments for the given ifo

  @param ifo         : name of the ifo
  @param config	     : the configParser object with analysis details
  @param categories  : list of veto categories
  @param generateVetoes: generate the veto files
  """
  
  start = config.getint("input","gps-start-time")
  end = config.getint("input","gps-end-time")
  vetoFiles = {}
  
  for category in categories:
    veto_cat_file = ifo + "-VETOTIME_CAT" + str(category) + "-" + \
      str(start) + "-" + str(end - start) + ".xml"

    vetoFile = ifo + "-CATEGORY_" + str(category) + "_VETO_SEGS-" + \
        str(start) + "-" + str(end - start) + ".txt"

    if generateVetoes: 
      return_val = convert_veto_cat_xml_to_txt(config, veto_cat_file, vetoFile)
      if not return_val:
        print "No vetoes found for %s cat %i. %s will contain no segments." \
          %(ifo, category, vetoFile) 
      

    # if there are previous vetoes, generate combined
    if (category-1) in categories: 
      combinedFile = ifo + "-COMBINED_CAT_" + str(category) + "_VETO_SEGS-" + \
          str(start) + "-" + \
          str(end - start) + ".txt"
      vetoFiles[category] = combinedFile

      if generateVetoes: 
        previousSegs = segmentsUtils.fromsegwizard(open(vetoFiles[category-1]))
        vetoSegs = segmentsUtils.fromsegwizard(open(vetoFile)).coalesce()
        vetoSegs |= previousSegs
        segmentsUtils.tosegwizard(file(combinedFile,"w"), vetoSegs)

    else: vetoFiles[category] = vetoFile

  return vetoFiles

##############################################################################
# Function to set up the segments for the analysis
def datafind_segments(ifo, config):
  """
  generate datafind segments for the given ifo

  @param ifo         : name of the ifo
  @param config: the configParser object with analysis details
  """
  ligoIfos = ["H1","H2","L1"]

  if ifo in ligoIfos: type = config.get("input","ligo-type")
  elif ifo == "G1": type = config.get("input","geo-type")
  elif ifo == "V1": type = config.get("input","virgo-type")

  if ifo == "V1": ifo_type = type
  else: ifo_type = ifo + "_" + type

  executable = config.get("condor", "datafind")
  start = config.getint("input","gps-start-time")
  end = config.getint("input","gps-end-time")


  dataFindFile = ifo_type + "-" + str(start) + "-" + \
      str(end - start) + ".txt"

  print "Running ligo_data_find to determine available data from " + type + \
      " frames for " + ifo
  dataFindCall = executable 
  for opt,arg in config.items("datafind"):
    dataFindCall += " --" + opt + " " + arg
  dataFindCall += " --observatory=" + ifo[0] + \
      " --type=" + ifo_type + \
      " --gps-start-time=" + str(start) + \
      " --gps-end-time=" + str(end) + " --show-times > " + \
      dataFindFile
  make_external_call(dataFindCall)
  dfSegs = segmentsUtils.fromsegwizard(file(dataFindFile)).coalesce()

  return dfSegs

##############################################################################
# Function to copy the segments files
def copyCategoryFiles(config,vetoes,directory,\
                      infile,analysisDirectory):
  """
  Copy the category files to the directory specified
  """
  outfile = analysisDirectory + "/" + directory + "/" \
            + os.path.basename(infile)
  rel_outfile = "../" + directory + "/" + os.path.basename(infile)
  if "veto-file" in vetoes:
    if infile == "":
      print >>sys.stderr, "warning: " + vetoes + " left blank; proceeding "\
        "without DQ vetoes"
      outfile += vetoes + "_BLANK.txt"
      rel_outfile += vetoes + "_BLANK.txt"
      open(outfile, "w").write("")  # touch
    else:
      shutil.copy(infile, outfile)
    config.set("segments", vetoes, rel_outfile)

##############################################################################
# Function to download the VetoDefFile
def downloadVetoDefFile(config,generate_segments):
  """
  Download the vetoDefFile from a html location
  @param config      : the configParser object with analysis details
  @param generate_segments : If False do not download, just return filename 
  """

  vetoDefurl = config.get("segments", "veto-def-server-url")
  vetoDefFile = config.get("segments", "veto-def-file")

  if generate_segments:
    print "Downloading veto-definer file " + vetoDefFile + " from " \
	+ vetoDefurl
    vetoDefFile, info = urllib.urlretrieve(vetoDefurl + '/' + vetoDefFile,
	vetoDefFile)

  return vetoDefFile

##############################################################################
# Function to download the dqSegFiles
def downloadDqSegFiles(config,ifo,generate_segments):
  """
  Download the dqSegFiles from a html location
  @param config      : the configParser object with analysis details
  @param ifo         : name of the ifo
  @param generate_segments : If False do not download, just return filename            
  """
  start = config.getint("input","gps-start-time")
  end = config.getint("input","gps-end-time")
  dqSegFile = ifo + "-DQ_SEGMENTS-" + str(start) + "-" + \
      str(end - start) + ".txt"
  if generate_segments:
    dq_url = config.get("segments","dq-server-url")
    dq_segdb_file = config.get("segments", ifo.lower() + '-dq-file')
    if dq_segdb_file == "":
      print >>sys.stderr, "warning: no file provided to %s-dq-file; " \
        "running without data quality" % ifo.lower()
    else:
      print "Downloading DQ segment file " + dq_segdb_file + " from " \
            + dq_url + " to " + dqSegFile + " ...",
      sys.stdout.flush()
      dqSegFile, info = urllib.urlretrieve(dq_url + '/' + dq_segdb_file,
            dqSegFile)
      print "done"
  return dqSegFile


##############################################################################
# Function to determine the segments to analyze 
#(science segments, data quality, missing segments)
def findSegmentsToAnalyze(config, ifo, veto_categories, generate_segments=True,\
    use_available_data=False, data_quality_vetoes=False):
  """
  generate segments for the given ifo

  @param ifo         : name of the ifo
  @param config      : the configParser object with analysis details
  @param generate_segments: whether the segment files should be generated
  @param use_available_data: restrict segments to data which actually available
  @param data_quality_vetoes: generate the cat2,3,4 DQ veto segments
  """

  start = config.getint("input","gps-start-time")
  end = config.getint("input","gps-end-time")
    
  # file names
  segFile = ifo + "-SELECTED_SEGS-" + str(start) + "-" + \
      str(end - start) + ".txt"
  missedFile = ifo + "-MISSED_SEGS-" + str(start) + "-" + \
      str(end - start) + ".txt"

  if generate_segments:
    print "Generating science segments for " + ifo + " ...",
    sys.stdout.flush()
  sciSegFile = science_segments(ifo, config, generate_segments)

  # generate vetoFiles
  if generate_segments: 
    sciSegs = segmentsUtils.fromsegwizard(file(sciSegFile)).coalesce()
    print " done."
    print "Generating cat 1 veto segments for " + ifo + " ...",
    sys.stdout.flush()
    vetoFiles = veto_segments(ifo, config, [1], generate_segments)
    print "done"

    # remove cat 1 veto times
    vetoSegs = segmentsUtils.fromsegwizard(open(vetoFiles[1])).coalesce()
    sciSegs = sciSegs.__and__(vetoSegs.__invert__())

    if use_available_data:
      dfSegs = datafind_segments(ifo, config)
      analyzedSegs = sciSegs.__and__(dfSegs)
      missedSegs = sciSegs.__and__(dfSegs.__invert__())
      segmentsUtils.tosegwizard(file(missedFile,"w"), missedSegs)
      print "Writing " + ifo + " segments which cannot be analyzed to file " \
          + missedFile
      print "Not analyzing %d s, representing %.2f percent of time" %  \
         (missedSegs.__abs__(),
         100. * missedSegs.__abs__() / max(analyzedSegs.__abs__(), 0.1) )

    else: analyzedSegs = sciSegs

    segmentsUtils.tosegwizard(file(segFile,"w"), analyzedSegs)
    print "Writing " + ifo + " segments of total time " + \
        str(analyzedSegs.__abs__()) + "s to file: " + segFile
    print "done"

  if data_quality_vetoes: 
    print "Generating cat " + str(veto_categories) + " veto segments for " + ifo + "..."
    sys.stdout.flush()
  dqVetoes = veto_segments(ifo, config, veto_categories, data_quality_vetoes )
  if data_quality_vetoes: print "done"

  return tuple([segFile, dqVetoes])


##############################################################################
# Function to make sure we don't slide by more than the length of our data
def slide_sanity(config, playOnly = False):
  """
  check that the requested number of slides makes sense
  config    = config file
  playOnly  = only doing playground
  """
  # extract the num-slides
  try: numSlides = config.get("pipeline","num-slides")
  except: numSlides = "" 

  maxLength = None
  if playOnly: maxLength = 600
  elif config.has_option("input", "max-thinca-segment"):
    maxLength = config.getint("input", "max-thinca-segment")

  if maxLength and numSlides:
    ifos = [x[0:2].upper() for x in config.options("ifo-details") \
        if "data" in x]
    maxSlide = max([config.getint("thinca-slide", ifo.lower() + "-slide") \
        for ifo in ifos])
    if (maxLength/2/maxSlide - 1) < int(numSlides):
      numSlides = str(maxLength/2/maxSlide - 1)
      print "Setting number of slides to " + numSlides + \
          " to avoid double wrapping"

  return numSlides

##############################################################################
# Function to set up lalapps_inspiral_hipe
def hipe_setup(hipeDir, config, ifos, logPath, injSeed=None, dataFind = False, \
    tmpltBank = False, playOnly = False, vetoCat = None, vetoFiles = None, \
    dax = False, tmpltbankCache = None):
  """
  run lalapps_inspiral_hipe and add job to dag
  hipeDir   = directory in which to run inspiral hipe
  config    = config file
  logPath   = location where log files will be written
  injSeed   = injection file to use when running
  dataFind  = run the datafind step of the pipeline
  tmpltBank = run the template bank step (part of the datafind dag)
  vetoCat   = run this category of veto
  vetoFiles = dictionary of veto files
  tmpltbankCache = lal.Cache of template bank files
  """
  # remember what directory we are in to construct the pegasus exec dir
  ihopeDir = os.path.split(os.getcwd())[-1]

  # make the directory for running hipe
  mkdir(hipeDir)

  # create the hipe config parser, keep only relevant info
  hipecp = copy.deepcopy(config)
  if dataFind or tmpltBank: # Template generation and datafind share a number of options
    hipeSections = ["condor", "pipeline", "input", "datafind","data",\
        "ligo-data","inspiral","virgo-data", "condor-max-jobs", "calibration"]
    if tmpltBank: # Template generation needs some extra options that datafind doesn't
      hipeSections.extend(["calibration", "geo-data", "tmpltbank", \
          "tmpltbank-1", "tmpltbank-2", "h1-tmpltbank", "h2-tmpltbank", \
          "l1-tmpltbank", "v1-tmpltbank", "g1-tmpltbank"])
  elif vetoCat:
    hipeSections = ["condor", "pipeline", "input", "data", "ligo-data", \
        "tmpltbank", "inspiral", "thinca", "thinca-2", "datafind", "virgo-data", \
        "thinca-slide", "coire", "coire-1", "coire-2","coire-inj", "sire", \
        "sire-inj", "condor-max-jobs", "calibration"]
  else:
    hipeSections = ["condor", "pipeline", "input", "calibration", "datafind",\
        "ligo-data", "virgo-data", "geo-data", "data", "tmpltbank", \
        "tmpltbank-1", "tmpltbank-2", "h1-tmpltbank", "h2-tmpltbank", \
        "l1-tmpltbank", "v1-tmpltbank", "g1-tmpltbank", "no-veto-inspiral", \
        "veto-inspiral", "inspiral", "h1-inspiral", "h2-inspiral", \
        "l1-inspiral", "g1-inspiral", "v1-inspiral", "thinca", "thinca-1", \
        "thinca-2", "thinca-slide", "trigbank", "sire",  \
        "sire-inj", "coire", "coire-1", "coire-2", "coire-inj", \
        "cohbank", "trigbank-coherent", "chia", "inspiral-coherent", "condor-max-jobs"]

  for seg in hipecp.sections():
    if not seg in hipeSections: hipecp.remove_section(seg)

  hipecp.remove_option("condor","hipe")
  hipecp.remove_option("condor","plot")
  hipecp.remove_option("condor","follow")

  # add the inspinj section
  hipecp.add_section("inspinj")

  # set the data type
  if playOnly:
    hipecp.set("pipeline", "playground-data-mask", "playground_only")
  else:
    hipecp.set("pipeline", "playground-data-mask", "all_data")

  # set the user-tag to be the same as the directory name
  if hipecp.get("pipeline","user-tag"):
    usertag = hipecp.get("pipeline", "user-tag") + "_" + hipeDir.upper()
  else:
    usertag = hipeDir.upper()

  # template banks should not have a usertag so they can be 
  # passed between sub-dags more easily
  if hipeDir == "datafind":
    hipecp.remove_option("pipeline","user-tag")
  else:
    hipecp.set("tmpltbank","user-tag","")

  if vetoCat:
    # set the old usertag in inspiral and inspinj, 
    # so that we pick up the correct xml inputs
    sections = ["inspiral"]
    for section in ["inspiral","inspinj"]:
      hipecp.set(section, "user-tag",usertag)

    # set the correct pipeline usertag
    usertag += "_CAT_" + str(vetoCat) + "_VETO"

    for cat in range(2, vetoCat+1):
      section = "coire-2-cat-" + str(cat)
      if config.has_section(section):
        for opt, arg in config.items(section):
          hipecp.set("coire-2",opt,arg)

    # add the veto files in the thinca section
    for ifo in ifos:
      hipecp.set("thinca", ifo.lower() + "-veto-file", vetoFiles[ifo][vetoCat])
    hipecp.set("thinca", "do-veto", "")

  # set the usertag
  if hipeDir == "datafind":
    # we don't set a usertag for the datafind directory
    pass
  else:
    hipecp.set("pipeline", "user-tag",usertag)

  if injSeed:
    # copy over the arguments from the relevant injection section
    for (name,value) in config.items(hipeDir):
      hipecp.set("inspinj",name,value)
    if config.has_option("hipe-arguments","ringdown"):
      injType = config.get(hipeDir,"injection-type")
      hipecp.set("inspiral","injection-type",injType)
      hipecp.remove_option("inspinj","injection-type")
      if injType == "RINGDOWN":
        executable = "../executables/lalapps_rinj"
        hipecp.set("condor","inspinj",executable)
    hipecp.remove_section(hipeDir)
    hipecp.set("input","injection-seed",injSeed)
    hipecp.set("input", "num-slides", "")
    # set any extra inspiral arguments for the injection
    try:
      for item in config.items('-'.join([hipeDir,"inspiral"])):
        hipecp.set("inspiral",item[0],item[1])
    except ConfigParser.NoSectionError:
      pass
  else:
    # add the time slide to the ini file
    numSlides = slide_sanity(config, playOnly)
    hipecp.set("input","num-slides",numSlides)

  # return to the directory, write ini file and run hipe
  os.chdir(hipeDir)
  iniFile = "inspiral_hipe_"
  iniFile += hipeDir 
  if vetoCat: iniFile += "_cat" + str(vetoCat) + "_veto"
  iniFile += ".ini"

  hipecp.write(file(iniFile,"w"))

  print "Running hipe in directory " + hipeDir
  if dataFind or tmpltBank: print "Running datafind / template bank generation"
  elif injSeed: print "Injection seed: " + injSeed
  else: print "No injections, " + str(hipecp.get("input","num-slides")) + \
      " time slides"
  if vetoCat: print "Running the category " + str(vetoCat) + " vetoes"
  print

  # work out the hipe call:
  hipeCommand = config.get("condor","hipe")
  hipeCommand += " --log-path " + logPath
  hipeCommand += " --config-file " + iniFile
  if playOnly: hipeCommand += " --priority 10"
  for item in config.items("ifo-details"):
    hipeCommand += " --" + item[0] + " " + item[1]

  def test_and_add_hipe_arg(hipeCommand, hipe_arg):
    if config.has_option("hipe-arguments",hipe_arg):
      hipeCommand += "--" + hipe_arg + " " + \
        config.get("hipe-arguments",hipe_arg)
    return(hipeCommand)

  if dataFind or tmpltBank:
    if dataFind:
      for hipe_arg in ["datafind","ringdown"]:
        hipeCommand = test_and_add_hipe_arg(hipeCommand,hipe_arg)
    if tmpltBank:
      for hipe_arg in ["template-bank","ringdown"]:
        hipeCommand = test_and_add_hipe_arg(hipeCommand,hipe_arg)
  elif vetoCat:
    if config.has_option("hipe-arguments","ringdown"):
      hipe_args = ["coincidence", "ringdown","coire-coincidence",
        "summary-first-coinc-triggers","write-script"]
    else:
      hipe_args = ["second-coinc", "coire-second-coinc", 
        "summary-coinc-triggers", "sire-second-coinc", 
        "summary-single-ifo-triggers","write-script"]
    for hipe_arg in hipe_args:
      hipeCommand = test_and_add_hipe_arg(hipeCommand,hipe_arg)
  else:
    omit = ["datafind", "template-bank", "disable-dag-categories", "disable-dag-priorities"]
    for (opt, arg) in config.items("hipe-arguments"):
      if opt not in omit:
        hipeCommand += "--" + opt + " " + arg 

  hipeCommand = test_and_add_hipe_arg(hipeCommand,"disable-dag-categories")
  hipeCommand = test_and_add_hipe_arg(hipeCommand,"disable-dag-priorities")

  # run lalapps_inspiral_hipe
  make_external_call(hipeCommand)

  # link datafind
  if not dataFind and not tmpltBank and not vetoCat:
    try:
      os.rmdir("cache")
      os.symlink("../datafind/cache", "cache")
    except: pass

  # symlink in the template banks, and add them to the inspiral hipe cache
  if tmpltbankCache:
    symlinkedCache = symlink_tmpltbank(tmpltbankCache, hipeDir)

    inspiral_hipe_file = open(hipe_cache(ifos, usertag, \
        hipecp.getint("input", "gps-start-time"), \
        hipecp.getint("input", "gps-end-time")), "a")
    symlinkedCache.tofile(inspiral_hipe_file)
    inspiral_hipe_file.close()

  iniBase = iniFile.rstrip("ini")
  if hipeDir == "datafind":
    hipeDag = iniBase + "dag"  
    hipeDax = iniBase + "dax"  
  else:
    hipeDag = iniBase + usertag + ".dag"  
    hipeDax = iniBase + usertag + ".dax"  

  hipeJob = pipeline.CondorDAGManJob(hipeDag, hipeDir, hipeDax)
  hipeNode = pipeline.CondorDAGNode(hipeJob)

  # sweep up the .input files hipe generates
  hipeJob.add_pfn_cache(os.path.join( os.getcwd(), hipe_pfn_cache(
    'hipe_input_files.%s.cache' % usertag,'*%s-*input' % usertag)))

  if hipeDir == "datafind":
    # grab the segment files managed by hipe and put them in the df cache
    # since it is inherited by all the other sub-workflows
    hipeJob.add_pfn_cache(os.path.join( os.getcwd(), hipe_pfn_cache(
      'segment_files.cache', '../segments/*txt' )))
    hipeJob.set_pegasus_exec_dir(os.path.join(
      logPath, '/'.join(os.getcwd().split('/')[-2:])))
  else:
    hipeJob.set_pegasus_exec_dir(os.path.join(
      logPath, '/'.join(os.getcwd().split('/')[-2:]), usertag))
    hipeNode.set_user_tag(usertag)

  hipeNode.add_output_file( hipe_cache(ifos, usertag, \
      hipecp.getint("input", "gps-start-time"), \
      hipecp.getint("input", "gps-end-time")) )

  # return to the original directory
  os.chdir("..")

  return hipeNode

##############################################################################
# Function to remove meta option from lalapps_plot_hipe ini file
def remove_plot_meta_option(plotcp,patternType,plottingCode):
  programTag = patternType + '-program-tag'
  plotMeta = plottingCode + '-meta'
  plotcp.remove_option(plotMeta,programTag)
  patterns = (plotcp.get(plotMeta,'cache-patterns')).split(',')
  patterns.remove(patternType)
  newpatterns=','.join(patterns)
  plotcp.set(plotMeta,'cache-patterns',newpatterns)

##############################################################################
# Function to set up lalapps_plot_hipe
def plot_setup(plotDir, config, logPath, stage, injectionSuffix,
    zerolagSuffix, slideSuffix, bankSuffix, cacheFile, injdirType, tag = None, 
    ifos = None, cat = 3):
  """
  run lalapps_plot_hipe and add job to dag
  plotDir   = directory in which to run inspiral hipe
  config    = config file
  logPath   = location where log files will be written
  stage     = which stage to run (first, second or both)
  injectionSuffix = the string to restrict to for injections
  zerolagSuffix   = the string to restrict to for zero lag
  slideSuffix     = the string to restrict to for time slides
  bankSuffix      = the string to restrict to for bank plots
  cacheFile       = the input cache file for plotting
  tag             = extra tag for naming
  """
  # make the directory for running hipe
  mkdir(plotDir)

  plotcp = copy.deepcopy(config)

  # set details for the common section
  plotcp.add_section("common")
  plotcp.set("common","gps-start-time", plotcp.get("input","gps-start-time") )
  plotcp.set("common","gps-end-time", plotcp.get("input","gps-end-time") )
  plotcp.set("common","output-path", ".")
  plotcp.set("common","enable-output","")

  plotSections = ["common", "pipeline", "condor",\
      "plotinspiral", "plotinspiral-meta", \
      "plotthinca", "plotthinca-meta", \
      "plotnumtemplates", "plotnumtemplates-meta", \
      "plotinjnum", "plotinjnum-meta", \
      "plotethinca", "plotethinca-meta", \
      "plotinspmissed", "plotinspmissed-meta", \
      "plotinspinj", "plotinspinj-meta", \
      "plotsnrchi", "plotsnrchi-meta", \
      "plotinspfound", \
      "plotinspiralrange", "plotinspiralrange-meta", \
      "ploteffdistcut", "ploteffdistcut-meta", \
      "plotinspfound", "plotcoincmissed"]

  for seg in plotcp.sections():
    if not seg in plotSections: plotcp.remove_section(seg)

  plotcp.remove_option("condor","hipe")
  plotcp.remove_option("condor","plot")
  plotcp.remove_option("condor","follow")

  # XXX Can't yet run the plotting codes in standard universe
  if plotcp.get("condor","universe") == "standard":
    plotcp.set("condor","universe","vanilla")

  # set the various suffixes in pipeline
  plotcp.set("pipeline","injection-suffix",injectionSuffix)
  plotcp.set("pipeline","inj-suffix",injectionSuffix)
  plotcp.set("pipeline","found-suffix",injectionSuffix)
  plotcp.set("pipeline","missed-suffix",injectionSuffix)
  plotcp.set("pipeline","bank-suffix",bankSuffix)
  plotcp.set("pipeline","trigbank-suffix",bankSuffix)
  plotcp.set("pipeline","zerolag-suffix",zerolagSuffix)
  plotcp.set("pipeline","trig-suffix",zerolagSuffix)
  plotcp.set("pipeline","coinc-suffix",zerolagSuffix)
  plotcp.set("pipeline","slide-suffix",slideSuffix)
  
  numSlides = slide_sanity(config, ("PLAYGROUND" in slideSuffix )) 
  plotcp.set("pipeline","num-slides", numSlides)

  # Adding followup options to plotinspmissed
  analysisstart = plotcp.get("common","gps-start-time")
  analysisend = plotcp.get("common","gps-end-time")
  analysisduration = int(analysisend) - int(analysisstart)
  inspmissedVetoDir = "../segments"
  for ifo in ifos:
    if cat == 2:
      plotcp.set("plotinspmissed","followup-vetofile-" + ifo.lower(),
        inspmissedVetoDir + "/" + ifo + "-CATEGORY_" + str(cat) +
        "_VETO_SEGS-" + analysisstart + "-" + str(analysisduration) + ".txt")
    else:
      plotcp.set("plotinspmissed","followup-vetofile-" + ifo.lower(),
        inspmissedVetoDir + "/" + ifo + "-COMBINED_CAT_" + str(cat) + 
        "_VETO_SEGS-" + analysisstart + "-" + str(analysisduration) + ".txt")

  # Adding followup option to plotinspfound and plotinspmissed
  plotcp.set("plotinspfound","followup-tag",injdirType)
  plotcp.set("plotinspmissed","followup-tag",injdirType)

  # Remove options if no slide or zero lag files are available.
  if "NONE_AVAILABLE" in slideSuffix:
    if plotcp.has_option('plotsnrchi-meta','slide-program-tag'):
      remove_plot_meta_option(plotcp,'slide','plotsnrchi')
    if plotcp.has_option('ploteffdistcut-meta','slide-program-tag'):
      remove_plot_meta_option(plotcp,'slide','ploteffdistcut')
    if plotcp.has_option('plotethinca-meta','slide-program-tag'):
      remove_plot_meta_option(plotcp,'slide','plotethinca')
  if "NONE_AVAILABLE" in zerolagSuffix:
    if plotcp.has_option('plotsnrchi-meta','trig-program-tag'):
      remove_plot_meta_option(plotcp,'trig','plotsnrchi')

  # set the user-tag
  if plotcp.get("pipeline","user-tag"):
    usertag = plotcp.get("pipeline","user-tag")
    plotcp.set("pipeline","input-user-tag",usertag)
    usertag += plotDir.upper() 
  else:
    usertag = plotDir.upper()
    plotcp.set("pipeline","input-user-tag","")

  if tag: usertag += "_" + tag
  plotcp.set("pipeline","user-tag",usertag)
  
  plotcp.set("common","cache-file",cacheFile)

  # return to the directory, write ini file and run hipe
  os.chdir(plotDir)
  iniFile = "plot_hipe_"
  iniFile += plotDir 
  if tag: iniFile += "_" + tag.lower()
  iniFile += ".ini"

  plotcp.write(file(iniFile,"w"))

  print "Running plot hipe in directory " + plotDir
  print "Using zero lag sieve: " + zerolagSuffix 
  print "Using time slide sieve: " + slideSuffix  
  print "Using injection sieve: " + injectionSuffix 
  print "Using bank sieve: " + bankSuffix 
  print

  # work out the hipe call:
  plotCommand = config.get("condor","plot")
  plotCommand += " --log-path " + logPath
  plotCommand += " --config-file " + iniFile
  plotCommand += " --priority 10"
  for item in config.items("ifo-details"):
      plotCommand += " --" + item[0] + " " + item[1]

  for item in config.items("plot-arguments"):
      plotCommand += " --" + item[0] + " " + item[1]

  if stage == "first" or stage == "both":
    plotCommand += " --first-stage"
  if stage == "second" or stage == "both":
    plotCommand += " --second-stage"
 
  # run lalapps_inspiral_hipe
  make_external_call(plotCommand)

  # make hipe job/node
  plotDag = iniFile.rstrip("ini") + usertag + ".dag"
  plotJob = pipeline.CondorDAGManJob(plotDag, plotDir)
  plotNode = pipeline.CondorDAGNode(plotJob)
  plotNode.set_user_tag(usertag)

  # return to the original directory
  os.chdir("..")

  return plotNode

##############################################################################
# Function to set up and run pipedown

def pipedownSetup(dag,config,logPath,pipedownDir,\
                  cacheFile,parentNodes,playgroundOnly):
  """
  Set up and run pipedown
  dag      = the dag
  config   = the config object
  logPath  = Location where log files will be written
  pipedownDir = Directory to run pipedown in
  cacheFile = The input ihope cache file
  parentNodes = Name of parent dags to add
  playgroundOnly = Are we opening the box?
  """
  # Get necessary information from the config object
  gpsStart = config.get("input","gps-start-time")
  gpsEnd = config.get("input","gps-end-time")

  # Make directory

  mkdir(pipedownDir)
  os.chdir(pipedownDir)

  # Create the necessary ini file

  pipeCp = copy.deepcopy(config) 
  pipeCp.set("condor","universe","vanilla")

  # Create input section

  pipeCp.remove_section("input")
  pipeCp.add_section("input")
  pipeCp.set("input","ihope-segments-directory","../segments")

  # Write ini file to folder

  iniFile = pipedownDir
  iniFile += ".ini"
  
  pipeCp.write(file(iniFile,"w"))

  # Set up the command to run pipedown

  pipeCommand = config.get("condor","pipedown")
  pipeCommand += " --log-path " + logPath
  pipeCommand += " --config-file " + iniFile
  pipeCommand += " --gps-start-time " + gpsStart
  pipeCommand += " --gps-end-time " + gpsEnd
  pipeCommand += " --ihope-cache " + cacheFile
  if not playgroundOnly:
    pipeCommand += " --generate-all-data-plots"
    # Need to add playground command

  # run lalapps_pipedown
  make_external_call(pipeCommand)

  # make pipedown job/node
  pipeDag = iniFile.rstrip("ini") + "dag"
  pipeJob = pipeline.CondorDAGManJob(pipeDag, pipedownDir)
  pipeNode = pipeline.CondorDAGNode(pipeJob)
  dag.add_node(pipeNode)
  if parentNodes: 
    for thisDag in parentNodes:
      pipeNode.add_parent(thisDag)

  # return to the original directory
  os.chdir("..")

  return dag



##############################################################################
# Function to set up zero/slide plots:
def zeroSlidePlots(dag, plotDir, config, logPath, zerolagSuffix, slideSuffix, 
    cacheFile, doDagCategories, parentDags = None, vetoParentDags = None, 
    vetoCat = [2,3,4], ifos = []):
  """
  set up plots for zero lag and time slides
  dag       = the dag
  plotDir   = directory in to set up plots
  config    = config file
  logPath   = location where log files will be written
  zerolagSuffix   = the string to restrict to for zero lag
  slideSuffix     = the string to restrict to for time slides
  cacheFile       = the input cache file for plotting
  doCategories    = dag categories turned on
  parentDags      = the name of the parent dag to add
  vetoParentDags  = the name of the veto parent dag to add
  vetoCat         = veto category
  """
  # first stage
  plotcp = copy.deepcopy(config)
  plotcp.add_section("plot-arguments")
  plotcp.set("plot-arguments","plotinspiralrange","")
  plotcp.set("plot-arguments","plotnumtemplates","")
  plotcp.set("plot-arguments","plotinspiral","")
  plotcp.set("plot-arguments","plotthinca","")
  plotcp.set("plot-arguments","write-script","")
  # Add plotthinca scaling option for zero lag play full data slide plots
  if zerolagSuffix == "PLAYGROUND" and slideSuffix == "FULL_DATA":
    plotcp.set("plotthinca","zero-lag-playground","")

  plotNode = plot_setup(plotDir, plotcp, logPath, "first", \
      "", zerolagSuffix, slideSuffix, slideSuffix, cacheFile, "", ifos=ifos)
  if doDagCategories:
    plotNode.set_category('plotting')
  dag.add_node(plotNode)
  if parentDags:
    for thisDag in parentDags: 
      plotNode.add_parent(thisDag)

  # second stage (require DQ) 
  for cat in vetoCat:
    vetoString = "_CAT_" + str(cat) + "_VETO"
    plotcp = copy.deepcopy(config)
    plotcp.add_section("plot-arguments")
    plotcp.set("plot-arguments","plotinspiral","")
    plotcp.set("plot-arguments","plotthinca","")
    plotcp.set("plot-arguments","write-script","")
    if zerolagSuffix == "PLAYGROUND" and slideSuffix == "FULL_DATA":
      plotcp.set("plotthinca","zero-lag-playground","")
    plotVetoNode = plot_setup(plotDir, plotcp, logPath, "second", \
        "", zerolagSuffix + vetoString, slideSuffix + vetoString, \
        slideSuffix + vetoString, cacheFile, "", \
        tag=vetoString[1:], ifos=ifos, cat=cat)
    if doDagCategories:
      plotVetoNode.set_category('plotting')
    dag.add_node(plotVetoNode)
    if parentDags:
      for thisDag in parentDags: 
        plotVetoNode.add_parent(thisDag)
    if vetoParentDags:
      for thisDag in vetoParentDags:
        plotVetoNode.add_parent(thisDag[cat])
    if not doDagCategories:
      plotVetoNode.add_parent(plotNode)

  return dag

##############################################################################
# Function to set up inj/zero/slide plots:
def injZeroSlidePlots(dag, plotDir, config, logPath, injectionSuffix,
    zerolagSuffix, slideSuffix, cacheFile, doDagCategories, parentDags = None, 
    vetoParentDags = None, vetoCat = [2,3,4], ifos = []):
  """
  set up plots for injections, zero lag and time slides
  dag       = the dag
  plotDir   = directory in to set up plots
  config    = config file
  logPath   = location where log files will be written
  injectionSuffix = the string to restrict to for injections
  zerolagSuffix   = the string to restrict to for zero lag
  slideSuffix     = the string to restrict to for time slides
  cacheFile       = the input cache file for plotting
  doCategories    = dag categories turned on
  parentDags      = the name of the parent dag to add
  vetoParentDags  = the name of the veto parent dag to add
  vetoCat         = veto category
  """
  # first stage
  plotcp = copy.deepcopy(config)
  plotcp.add_section("plot-arguments")
  plotcp.set("plot-arguments","plotinspinj","")
  plotcp.set("plot-arguments","plotinjnum","")
  plotcp.set("plot-arguments","plotinspmissed","")
  plotcp.set("plot-arguments","ploteffdistcut","")
  plotcp.set("plot-arguments","write-script","")
  injPlotNode = plot_setup( plotDir, \
      plotcp, logPath, "first", injectionSuffix, zerolagSuffix, \
      slideSuffix, injectionSuffix, cacheFile, injectionSuffix, ifos=ifos )
  if doDagCategories:
    injPlotNode.set_category('plotting')
  dag.add_node(injPlotNode)

  if parentDags:
    for thisDag in parentDags:
      injPlotNode.add_parent(thisDag)

  # second stage
  for cat in vetoCat:
    vetoString = "_CAT_" + str(cat) + "_VETO"
    plotcp = copy.deepcopy(config)
    plotcp.add_section("plot-arguments")
    plotcp.set("plot-arguments","plotinspinj","")
    plotcp.set("plot-arguments","plotsnrchi","")
    plotcp.set("plot-arguments","plotethinca","")
    plotcp.set("plot-arguments","plotinjnum","")
    plotcp.set("plot-arguments","plotinspmissed","")
    plotcp.set("plot-arguments","plotinspfound","")
    plotcp.set("plot-arguments","plotcoincmissed","")
    plotcp.set("plot-arguments","ploteffdistcut","")
    plotcp.set("plot-arguments","write-script","")
    injPlotVetoNode = plot_setup( plotDir, \
        plotcp, logPath, "second", injectionSuffix + vetoString, \
        zerolagSuffix + vetoString, slideSuffix + vetoString, \
        injectionSuffix + vetoString, cacheFile, injectionSuffix, \
        tag=vetoString[1:], ifos=ifos, cat=cat)
    if doDagCategories:
      injPlotVetoNode.set_category('plotting')
    dag.add_node(injPlotVetoNode)
 
    if parentDags:
      for thisDag in parentDags:
        injPlotVetoNode.add_parent(thisDag)
    if vetoParentDags:
      for thisDag in vetoParentDags:
        injPlotVetoNode.add_parent(thisDag)

    if not doDagCategories:
      injPlotVetoNode.add_parent(injPlotNode)

  return dag



##############################################################################
# Function to set up lalapps_followup_pipe
def followup_setup(followupDir, config, opts, hipeDir):
  """
  run lalapps_followup_pipe and add job to dag
  followupDir = directory to output the followup
  config    = config file
  """

  # make the directory for followup pipe
  mkdir(followupDir)

  # create the followup config parser, keep only relevant info
  followupcp = copy.deepcopy(config)
  followupSections = ["condor", "hipe-cache", "triggers", "datafind", \
      "q-datafind", "qscan", "q-hoft-datafind", "qscan-hoft", \
      "plots", "output", "seg"]

  for seg in followupcp.sections():
    if not seg in followupSections: followupcp.remove_section(seg)

  followupcp.remove_option("condor","hipe")
  followupcp.remove_option("condor","follow")

  # XXX this should be replaced by getting the information from the hipe cache
  # set the cache paths
  followupcp.add_section("hipe-cache")
  followupcp.set("hipe-cache", "hipe-cache-path", "hipe_cache")
  followupcp.set("hipe-cache", "science-run", "S5")

  for path in ["tmpltbank-path", "trigbank-path", "first-inspiral-path", \
      "second-inspiral-path", "first-coinc-path", "second-coinc-path"]:
    followupcp.set("hipe-cache", path, "../" + hipeDir)

  # set the xml-glob
  followupcp.set("triggers", "xml-glob", "../" + hipeDir + "/*COIRE*H*xml")
  # to here XXX

  # correct paths to qscan config files
  for section in ["qscan", "qscan-hoft"]:
    for (opt, arg) in followupcp.items(section):
      if "config-file" in opt and arg[0] != "/":
        arg = "../../" + arg
        followupcp.set(section, opt, arg)

  # return to the directory, write ini file and run hipe
  os.chdir(followupDir)
  iniFile = "followup_pipe_" + followupDir + ".ini"
  followupcp.write(file(iniFile,"w"))

  # link datafind output from original hipe
  try: os.symlink("../datafind/cache", "hipe_cache")
  except: pass
  print "Running followup pipe in directory " + followupDir

  # work out the followup_pipe call:
  followupCommand = config.get("condor","follow")
  followupCommand += " --log-path " + opts.log_path
  followupCommand += " --config-file " + iniFile

  for item in config.items("followup-arguments"):
    followupCommand += " --" + item[0] + " " + item[1]

  # set up a fake followup dag -- the real one can't be generated until the
  # analysis is done
  followupDag = iniFile.rstrip("ini") + "dag"
  f = open(followupDag,"w")
  f.write("\n")
  f.close()

  # add job to dag
  followupJob = pipeline.CondorDAGManJob(followupDag, followupDir)
  followupNode = pipeline.CondorDAGNode(followupJob)


  # write the pre-script to run lalapps_followup_pipe at the appropriate time
  f = open(followupDag + ".pre","w")
  f.write("#! /bin/bash\n")
  f.write("cd followup\n")
  f.write(followupCommand)
  f.write("cd ..\n")
  f.close()
  os.chmod(followupDag + ".pre", 0744)
  followupNode.set_pre_script(followupDir + "/" + followupDag + ".pre")

  # return to the original directory
  os.chdir("..")

  return followupNode

##############################################################################
# plot_hipe helpers

def determine_sieve_patterns(cp, plot_name, ifotag, usertag=None):
    """
    From the given plot configuration file, determine the --*-pattern values
    for a given plot name.  Returns as a dictionary of command-line option-
    value pairs.
    
    Example ini file:
    [pipeline]
    trig-suffix = PLAYGROUND
    bank-suffix = PLAYGROUND
    missed-suffix = INJ*
    
    [plotinspiral-meta]
    cache-patterns = trig,bank,missed
    trig-program-tag = SIRE
    bank-program-tag = TRIGBANK
    missed-program-tag = SIRE
    
    Example invocation:
    >>> from lalapps.inspiralutils import determine_sieve_patterns
    >>> from ConfigParser import ConfigParser
    >>> cp = ConfigParser()
    >>> cp.read("plot_hipe.ini")
    >>> print determine_sieve_patterns(cp, "plotinspiral", "H1")
    {'bank-pattern': 'TRIGBANK_H1*_PLAYGROUND', 'missed-pattern':
     'SIRE_H1*_INJ*', 'trig-pattern': 'SIRE_H1*_PLAYGROUND'}
    
    """
    meta_name = plot_name + "-meta"
    usertag = cp.get("pipeline", "input-user-tag") or usertag
    suffixes = dict([(opt[:-len("-suffix")],val) for opt,val in cp.items("pipeline") if opt.endswith("-suffix")])

    patterns = {}
    for pattern_name in cp.get(meta_name, "cache-patterns").split(","):
        program_tag = cp.get(meta_name, pattern_name + "-program-tag")
        pattern = program_tag + "_" + ifotag + "*"
        if usertag is not None:
            pattern += "_" + usertag
        suffix = suffixes.get(pattern_name)
        if suffix is not None:
            pattern += "_" + suffix
        patterns[pattern_name + "-pattern"] = pattern
    return patterns

