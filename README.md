## CUPS-File

"Smart" file backend for CUPS

## Background

Although CUPS has the `file://` back end which gives it the ability to send a print job to a file, it is considered only usable for diagnostic purposes.  The path to the file being created is hard-coded in `cupsd.conf` and cannot change depending on the user, the queue name, or other variables.  Since the file is written by `cupsd`, which runs as `root`, the use of the `file://` back end is discouraged.

CUPS-File` intended to provide a safe and intelligent way to save the output of print jobs to a filesystem.  It's features include:

* Files are written as the user who sumbitted the print job, with the resulting restrictions on where/how files can be saved
* The path to the file can be changed depending on seveal variables:

    * The name of the user who submitted the job
    * The title of the print job
    * The date and time the job was submitted
    * The name of the print queue to which the job was submitted

CUPS-File does nothing other than save the file created by the other CUPS filters.  The Generic PostScript, Generic PDF, or Generic PCL PPD files can be used to generate an appropriate file format -- Generic PDF being the obvious choice for those who want to convert/save documents to PDF.

## Usage

The path to the file will be encoded in the Device URI.  Something like:

    cups-file://home/@user@/@queue@/@date@.pdf

TODO: Work our the details and document them correctly here.

## Other Stuff

Copyright (C) 2019 Bryan Mason

"CUPS" is a trademark of [Apple, Inc.](http://www.apple.com/legal/intellectual-property/trademark/appletmlist.html)  The CUPS-File project has no affiliation with the CUPS project at [http://cups.org/](http://cups.org/) or Apple, Inc.
