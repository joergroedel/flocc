The Fast Lines Of Code Counter (flocc)
======================================

The Fast Lines Of Code Counter (flocc) is a tool which quickly traverses
a directory tree and counts the lines of code, comments and blank lines
of known source files.

The directory traversed can be on a file system or it can be a git-tree
object, referenced by a git revision. In git mode the revision does not
need to be checked out to a working tree.

As output is provides a summary of the numbers counted and numbers for
each programming or markup language found in the source tree. The output
can also be saved to a JSON file, which will contain very detailed
statistics for every file and directory.

To run the tool, just do

	$ cd ~/your/source/tree
	$ flocc .

For git mode, do:

	$ cd ~/your/source/tree
	$ flocc --git v1.0	# v1.0 would be a git-tag

The tool is in its early stages, but already useful. Please report any
bugs or feature requests to <jroedel@suse.de>.

Have a lot of fun!
