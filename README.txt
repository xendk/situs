Situs site building command for Drush
=====================================

Situs is yet another site building command, trying to be as simple as
possible to use.

It relies on one assumption: That anything that should not be wiped in
a rebuild is located in sites/<sites> (excluding the sites/all
directory, which is often populated by make files).


Usage
=====

Situs needs three things in order to build: A site alias, a root path
and a make file. The make file is simply specified in the site alias,
like so:

$aliases['mysite'] = array(
  'root' => '/var/www/mysite,
  'make-file' => '/home/user/mysite.make',
);

Building and rebuilding is then done with the same command:

drush situs-build @mysite

Which will run the make file, move the sites in sites/ over and
replace the old root with the newly build.


Extending
=========

Situs is meant to be expanded upon, and the plan is to include a
selection of useful add-ons. Currently there's only the git-check
plugin (see below), but implementing a plugin is strait-forward:

Implement hook_drush_help_alter in order to add a switch to trigger
the plugin for a site. All plugins should add a switch for its
behaviour, either to enable it (preferred), or disable it.

Implement one or more of the hooks Situs provides, situs_pre_build and
situs_post_build, to do whatever, when the switch is specified.


Checking git checkouts for changes 
==================================

There is a plugin included, which both serves as an example, and is
quite useful in itself. By providing --git-check on the command line,
or 'git-check' => TRUE in the alias definition, it will check all git
repositories in the existing root for uncommitted changes or unpushed
commits, and abort the rebuild if any is found.


Ideas for plugins
=================

Some plugins that's planned (I wont complain if someone beats me to it
with a pull request).

Database sync: Triggers on the existence of a 'sync-from' option,
which is the name of another alias to do a sql-sync from. Has a
no-db-sync killswitch (as the sync-from option is shared with files
sync).

Files sync: The same as db sync, but for core-rsync.

Install modules: Installs additional modules using drush dl.

Enable modules: Enables modules that's either part of the build, but
not enabled per default, or downloaded by the previous plugin.

Drush command: Runs an arbitrary drush command. The previous two is
probably obsoleted by this.
