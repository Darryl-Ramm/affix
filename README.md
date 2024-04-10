#  AFFIX

**affix**: Report AIFF and AIFF-C audio file sample rates, optionally correct incorrect sample rates. 

A UNIX style command line program that prints the sample rate of AIFF or AIFF-C files, and optionally sets a different sample rate. The program is intended to make it simple to repair problems where AIFF/AIFF-C files end up set to an incorrect sample rate. affix merely changes the sample rate value stored in the file AIFF/AIFF-C COMM chunk, it does not change the sample rate of the actual digital audio data in the SSND chunk. The only part of an AIFF/AIFF-C file modified by affix is the sample rate embedded in the COMM chunk.

affix is intended to complement macOS afinfo and afconvert. afinfo provides sample rate and other information but does not allow changing or correcting an incorrect sample rate. macOS afconvert is fairly  flexible, does not provide a way to correct an incorrect sample rate.

Usage: **affix [-vVdh] [-s sampleRate] aiff_file1 ... aiff_filen**

affix operates on one or more files with filenames provided on the command line.

**-v** option prints additional information for each file. Output for each file consist of a line of the following tab separated values:

fileName<br>number of audio channels<br>number of sample frames in the file<br>bits per sample<br>sample rate<br>file type: AIFF or AIFC (i.e. AIFF-C)<br>compression type from the compression string in the AIFF-C file

**-V** option prints the affix version and copyright information and exits.

**-d** option prints debug information that is only likely useful for developers with source. This option is partially hidden and not documented in the affix usage help.

**-h** option prints usage information and lists these options (except for **-d**).

**-s sampleRate** option resets the sample rate. sampleRate here is an integer even though internally AIFF/AIFF-C sample rates are floating point values. Only allowing integer values avoids accidental entering incorrect rates.

There is a bit more in this code than needed for just simply fixing sample rates, this could be a start of a more general AIFF/AIFC file checking program. This is a hybrid UNIX and CoreFoundation program and as such gets a little ugly/mixed up between those worlds.

affix does some basic checking that any AIFF/AIFF-C file is valid and tries to work with file even if they may have some problems. Since non-standard chunk types may be present in an AIFF/AIFF-C file affix will warn about any unknown chunk types on stderr, but will still process the file. 

See also macOS man pages for afinfo(1) and afconvert(1).

### License

Affix is licensed under the MIT open source license. See the license terms in main.c.

### Example Output

Example output showing 96 kHz and 8 kHz sample rates of files

```
$ affix *
M1F1-AlawC-AFsp.aif     96000
M1F1-float32C-AFsp.aif  8000
M1F1-float64C-AFsp.aif  8000
```

Example -v (verbose) output showing 

fileName<br>number of audio channels<br>number of sample frames in the file<br>bits per sample<br>sample rate<br>file type: AIFF or AIFC (AIFF Compressed)<br>compression type from the compression string in the AIFF-C file

```
$ affix -v *
M1F1-AlawC-AFsp.aif     2     23493   16    96000   AIFC    Alaw 2:1
M1F1-float32C-AFsp.aif  2     23493   32    8000    AIFC    IEEE 32-bit float
M1F1-float64C-AFsp.aif  2     23493   64    8000    AIFC    IEEE 64-bit float
M1F1-int12-AFsp.aif     2     23493   12    8000    AIFF    not compressed
M1F1-int12C-AFsp.aif    2     23493   12    8000    AIFC    not compressed
M1F1-int16-AFsp.aif     2     23493   16    8000    AIFF    not compressed
M1F1-int16C-AFsp.aif    2     23493   16    8000    AIFC    not compressed
M1F1-int16s-AFsp.aif    2     23493   16    8000    AIFC
M1F1-int24-AFsp.aif     2     23493   24    8000    AIFF    not compressed
M1F1-int24C-AFsp.aif    2     23493   24    8000    AIFC    not compressed
M1F1-int32-AFsp.aif     2     23493   32    8000    AIFF    not compressed
M1F1-int32C-AFsp.aif    2     23493   32    8000    AIFC    not compressed
M1F1-int8-AFsp.aif      2     23493   8     8000    AIFF    not compressed
M1F1-int8C-AFsp.aif     2     23493   8     8000    AIFC    not compressed
M1F1-mulawC-AFsp.aif    2     23493   16    8000    AIFC    (null)
```



###  AIFF and AIFC File Format Resources

-  McGill University: https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/AIFF/AIFF.html
-  Library of Congress: https://www.loc.gov/preservation/digital/formats/fdd/fdd000005.shtml
-  Paul Bourke: https://paulbourke.net/dataformats/audio

 The `<AIFF.h>` header file contains useful definitions and is incorporated in the code via `<CoreServices/CoreServices.h>`. The actual file is at .../CarbonCore.framework/Headers/AIFF.h

Or full path:

/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs
 /MacOSX.sdk/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks
 /CarbonCore.framework/Headers/AIFF.h

### Executable Versioning

The versioning information for the **-V** option is obtained from the Info.plist embedded in the executable.  The same information is also embedded in the executable as a what(1) SCCS compatible version string. The versioning is set using git tags and the set_build_number.sh shell script is run as a build pre-action script.

### Testing

As a part of testing affix we use a collection of AIFF/AIFF-C sample files (including the "perverse" files and the Stanford files) from McGill University here https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/AIFF/Samples.html. Download those into the aif_test_files directory in the affix Xcode directory if you want to use the same scripts.

The testing steps through that collection of AIFF/AIFF-C files and compares what affix and macOS afinfo return as sample rates. And then affix steps the though changing the sample rate and compare what it and afinfo read back as new sample rates. Since setting the sample rate is fairly simple the real value here is ensuring we handle all the different example files, including "perverse" files in that McGill sample set.

Given the lack of Xcode easy ability to test command line utilities, this testing is run from the test.sh shell script that can be invoked as a Xcode build post-action or run by hand.

affix will handle more perversely formatted AIFF/AIFF-C files than macOS affinfo which makes relying on affinfo for all testing problematic. In those cases where afinfo fails we then trust what affix reads for a file's sample rate and then check we affic can set a different sampel rate and read that back OK.

###  Packaging/Signing the Command Line Program

To distribute affix outside of the App Store the executable is signed and packaged in a .dmg disk image that is signed and then notarized with notarytool and the ticket is stapled to the .dmg. This is done in the build_dmg.sh build post-action shell script.

