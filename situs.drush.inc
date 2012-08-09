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

  /*
   * situs-build ...
   * situs-rebuild
   */

  $items['situs-rebuild'] = array(
    'description' => 'Build a site.',
    'arguments' => array(
      'site-alias' => 'Site alias to rebuild.',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,

  );

  return $items;
}

/**
 * Validate arguments to rebuild command.
 */
function drush_situs_rebuild_validate($alias = '') {
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
 *
 */
function drush_situs_rebuild($alias) {
  $site_record = drush_sitealias_get_record($alias);
  $root = $site_record['root'];

  $build_path = drush_tempdir() . '/build';

  // @todo run pre (re)build hooks
  // @todo document this.
  $result = drush_command_invoke_all('situs_pre_build', $site_record);
  if (!empty($result)) {
    return FALSE;
  }
  /* $hook = 'situs_pre_build'; */
  /* foreach (drush_command_implements($hook) as $module) { */
  /*   $function = $module . '_' . $hook; */
  /*   $result = call_user_func_array($function, array($site_record)); */

  /*   if (isset($result)) { */
  /*     die(); */
  /*     return FALSE; */
  /*   } */
  /* } */
  $settings = array(
    'working-copy' => TRUE,
  );
  $res = drush_invoke_process('@self', 'make', array($site_record['make-file'], $build_path), $settings, TRUE);

  if (!$res || $res['error_status'] != 0) {
    drush_set_error('BUILD_FAILED', dt('Drush Make failed.'));
  }

  // @todo backup the old around here.
  // @todo move site/ stuff to the new.
  $files = drush_scan_directory($root . '/sites', '/.*/', array('all', 'example.sites.php'), 0, FALSE, 'basename');
  foreach ($files as $name => $info) {
    if (is_dir($info->filename)) {
      drush_move_dir($info->filename, $build_path . '/sites/' . $name, TRUE);
    }
    else {
      drush_op('rename', $info->filename, $build_path . '/sites/' . $name);
    }
  }
  // Move site into place.
  drush_command_invoke_all('situs_post_build', $site_record, $build_path);
  drush_move_dir($build_path, $root, TRUE);

  // @todo run post (re)build hooks
}

function situs_drush_help_alter(&$command) {
  if ($command['command'] == 'situs-rebuild') {
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
