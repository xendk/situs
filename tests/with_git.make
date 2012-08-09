; Make file for situs unit tests.
; define core version and drush make compatibility
core = 7.x
api = 2

projects[drupal][version] = 7.15

; Git repo
projects[devel][type] = module
projects[devel][download][type] = git
projects[devel][download][tag] = 7.x-1.3
