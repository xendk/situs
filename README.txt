Situs site building command for Drush
=====================================

Situs is yet another site building command, trying to be as simple as
possible to use.

It relies on one assumption: That anything that should not be wiped in
a rebuild is located in sites/<sites> (excluding the sites/all
directory, which is often populated by make files).


Usage
=====

Situs needs two things in order to build: A root path and a make
file. In the simplest case, this can be specified on the command line:

drush situs-build --root=/var/www/site --make-file=./site.make

For easier rebuilding it is recommended to use a site alias. The make
file is simply specified in the site alias, like so:

$aliases['mysite'] = array(
  'root' => '/var/www/mysite,
  'make-file' => '/home/user/mysite.make',
);

As the make file is simply passed to Drush make, anything that Drush
make will accept should be usable, including remote make files.

Building and rebuilding is then done with the same command:

drush situs-build @mysite

Which will run the make file, move the sites in sites/ over and
replace the old root with the newly build.

Extra options
=============

(Can be specified in command line or alias)

settings-file
Can be used to specify a settings.php file to be copied into the sites/default
folder of the build.

no-version-control
If specified, it will not make a working copy with version control data.

Saving files
============

You might want to save some files through rebuilding, such as IDE
configuration files, TAG files or other local files. Situs attempts to
save the files specified by the --save-files option, but if the
directory of the specified file does not exist in the new build, it
will be quietly dropped.

To specify save files on a global basis you can add it to drushrc.php
in your .drush folder like so:

$command_specific = array(
  'situs' => array(
    'save-files' => 'TAGS,GTAGS,GPATH,GRTAGS,GSYMS',
  ),
);

Extending
=========

Situs can be extended with plugins that run before and/or after the
build process. Currently there's only the git-check plugin (see
below), but implementing a plugin is strait-forward:

Plugins should implement hook_situs_plugin() which should return an
array of plugins. This array is keyed on internal plugin name, and the
value is the plugin definition array.

The definition array can contain the following keys:

'name'
  The human readable name of the plugin. Used in help pages.
'description'
  A short description of what the plugin does.
'options'
  Command line options the plugin accepts. This is the same format as
  options in drush command definitions. Plugin should always define an
  option to enable (preferred) or disable it.
'pre_build'
  Callback for pre_build hook. Optional.
'post_build'
  Callback for post_build hook. Optional.

The callbacks will be called at the appropriate stages of the build
process.

Checking git checkouts for changes 
==================================

There is a plugin included, which both serves as an example, and is
quite useful in itself. By providing --git-check on the command line,
or 'git-check' => TRUE in the alias definition, it will check all git
repositories in the existing root for uncommitted changes or unpushed
commits, and abort the rebuild if any is found.

It is highly recommended to put it in the alias if using git for
development, as it provides a safety net against building over
uncommitted/pushed changes.

