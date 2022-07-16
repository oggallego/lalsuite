## Check SWIG Octave bindings for LALInference
## Author: Karl Wette, 2011--2014

page_screen_output(0);
crash_dumps_octave_core(0);

## check module load
disp("checking module load ...");
lalinference;
assert(exist("lalinference"));
lal;
assert(exist("lal"));
disp("PASSED module load");

# set error handlers
function set_nice_error_handlers()
  swig_set_nice_error_handlers();
endfunction
function set_default_error_handlers()
  lal;
  if swig_version >= 0x040002
    # see https://github.com/swig/swig/pull/1789
    swig_set_nasty_error_handlers();
  else
    swig_set_nice_error_handlers();
  endif
endfunction
set_default_error_handlers();

## check object parent tracking
disp("checking object parent tracking ...");
a = lalinference.new_swig_lalinference_test_parent_map_struct();
for i = 1:7
  b = a.s;
  c = lalinference.swig_lalinference_test_parent_map.s;
  lalinference.swig_lalinference_test_parent_map.s = lal.swig_lal_test_struct_const;
endfor
clear c;
clear b;
clear a;
clear ans;
LALCheckMemoryLeaks();
disp("PASSED object parent tracking");

## passed all tests!
disp("PASSED all tests");
if exist("swig_exit")
   swig_exit;
endif
