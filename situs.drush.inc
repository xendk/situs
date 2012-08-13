<?php

/**
 * @file
 * Situs, site (re)building commands.
 */

/**
 * Implements hook_drush_command().
 */
function situs_drush_command() {
  $items = array();

  $items['situs-build'] = array(
    'description' => '(Re)build a site.',
    'arguments' => array(
      'site-alias' => 'Site alias to build.',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  return $items;
}

/**
 * Implements hook_drush_help().
 */
function situs_drush_help($section) {
  switch ($section) {
    case 'meta:situs:title':
      return dt('Situs');
    case 'meta:situs:summary':
      return dt('Simple site (re)building.');
    case 'drush:situs-build':
      return dt("Builds or rebuilds a site.

Requires a site alias with a 'make-file' and a 'root' specification. Builds the site from the make file, and moves existing sites over from the sites folder.

Plugins may provide further functionality through command options (see options below, which can also be specified in the alias).");
  }
}

/**
 * Validate arguments to build command.
 */
function drush_situs_build_validate($alias = '') {
  $site_record = drush_sitealias_get_record($alias);
  if (empty($alias)) {
    return drush_set_error('NO_SITEALIAS', dt('No site alias specified.'));
  }
  if (empty($site_record)) {
    return drush_set_error('INVALID_SITEALIAS', dt('Could not find site alias specified.'));
  }
  if (empty($site_record['root'])) {
    return drush_set_error('NO_ROOT', dt('No root directory specified.'));
  }
  if (!is_dir(dirname($site_record['root']))) {
    return drush_set_error('INVALID_ROOT_PARENT', dt("Root parent doesn't exist, or isn't a directory."));
  }
  if (empty($site_record['make-file'])) {
    return drush_set_error('NO_MAKEFILE', dt('No make file on alias.'));
  }
}

/**
 * Build command.
 */
function drush_situs_build($alias) {
  $site_record = drush_sitealias_get_record($alias);
  $root = $site_record['root'];

  $build_path = drush_tempdir() . '/build';

  drush_set_context('SITUS_ROOT_PATH', $root);
  drush_set_context('SITUS_BUILD_PATH', $build_path);

  // @todo document this.
  $result = drush_command_invoke_all('situs_pre_build', $site_record);
  if (!empty($result)) {
    return FALSE;
  }

  $settings = array(
    'working-copy' => TRUE,
    'no-gitinfofile' => TRUE,
  );
  $res = drush_invoke_process('@self', 'make', array($site_record['make-file'], $build_path), $settings, TRUE);

  if (!$res || $res['error_status'] != 0) {
    return drush_set_error('BUILD_FAILED', dt('Drush Make failed.'));
  }

  // @todo backup the old around here.
  // Move site/ stuff to the new.
  $files = drush_scan_directory($root . '/sites', '/.*/', array('all', 'example.sites.php'), 0, FALSE, 'basename');
  $moved = array();
  foreach ($files as $name => $info) {
    if (is_dir($info->filename)) {
      if (!drush_move_dir($info->filename, $build_path . '/sites/' . $name, TRUE)) {
        return drush_set_error('CANNOT_MOVE_SITE', dt('Could not move site @site.', array('@site' => $name)));
      }
      $moved[] = $name;
    }
    else {
      if (!drush_op('rename', $info->filename, $build_path . '/sites/' . $name)) {
        return drush_set_error('CANNOT_MOVE_FILE', dt('Could not move file @file.', array('@file' => $name)));

      }
      $moved[] = $name;
    }
    drush_set_context('SITUS_MOVED_SITES', $moved);
  }

  // @todo document this.
  $result = drush_command_invoke_all('situs_post_build', $site_record, $build_path);
  if (!empty($result)) {
    return FALSE;
  }
  // Move site into place.
  if (file_exists($root)) {
    if (!drush_move_dir($root, $root . '.situs-old', TRUE)) {
      return drush_set_error('CANNOT_MOVE_OLD_ROOT', dt('Cannot move old root out of way.'));
    }
    drush_set_context('SITUS_ROOT_MOVED', TRUE);
  }
  if (!drush_move_dir($build_path, $root, TRUE)) {
    return drush_set_error('CANNOT_MOVE_NEW_ROOT', dt('Cannot move new site into place. It is located at @path, please fi'));
  }
  // Now delete the old dir.
  drush_delete_dir($root . '.situs-old', TRUE);

  // @xxx Post move hooks?
}

/**
 * Rollback.
 */
function drush_situs_build_rollback() {
  $root = drush_get_context('SITUS_ROOT_PATH', '');
  $build_path = drush_get_context('SITUS_BUILD_PATH', '');
  if (drush_get_context('SITUS_ROOT_MOVED', FALSE)) {
    // Move it back.
    drush_move_dir($root . '.situs-old', $root, TRUE);
  }
  if ($sites = drush_get_context('SITUS_MOVED_SITES', array())) {
    foreach ($sites as $name) {
      if (is_dir($info->filename)) {
        drush_move_dir($build_path . '/sites/' . $name, $root . '/sites/' . $name, TRUE);
      }
      else {
        drush_op('rename', $build_path . '/sites/' . $name, $root . '/sites/' . $name);
      }
    }
  }
}

/**
 * Implements hook_drush_help_alter().
 *
 * Add in the options for git-check. This demonstrates how add on modules should
 * do it.
 */
function situs_drush_help_alter(&$command) {
  if ($command['command'] == 'situs-build') {
    $command['options']['git-check'] = 'Check that all git repositories contains no uncommitted changes';
  }
}


/**
 * Implements hook_situs_pre_build().
 */
function situs_situs_pre_build($site_alias) {
  if (drush_get_option('git-check', FALSE)) {
    $root = $site_alias['root'];
    $git_checkouts = `find $root -type d -name .git`;
    $git_checkouts = split("\n", $git_checkouts);
    foreach ($git_checkouts as $filename) {
      $uncommitted = `git --git-dir=$filename status --porcelain`;
      if (!empty($uncommitted)) {
        return drush_set_error('UNCOMMITTED_CHANGES', dt("You have uncommitted changes in @dir.", array('@dir' => strtr(dirname($filename), array($site_alias['root'] => '')))));
      }
      $unpushed = `git --git-dir=$filename log --branches --not --remotes --simplify-by-decoration --decorate --oneline`;
      if (!empty($unpushed)) {
        return drush_set_error('UNPUSHED_CHANGES', dt("You have unpushed changes in @dir.", array('@dir' => strtr(dirname($filename), array($site_alias['root'] => '')))));
      }
    }
  }
}
