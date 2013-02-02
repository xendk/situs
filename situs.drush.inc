<?php

/**
 * @file
 * Situs, site (re)building commands.
 */

/**
 * Implements hook_drush_command().
 */
function situs_drush_command() {
  // Build command options, including those defined by plugins.
  $options = array(
    'root' => 'The root of the site to build.',
    'make-file' => 'The make file to use for building.',
  );
  foreach (situs_plugins() as $info) {
    if (!empty($info['options'])) {
      $options += $info['options'];
    }
  }

  $items['situs-build'] = array(
    'description' => '(Re)build a site.',
    'aliases' => array('situs'),
    'arguments' => array(
      'site-alias' => 'Site alias to build, optional if specifying root and make-file options.',
    ),
    'options' => $options,
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  $items['situs-plugins'] = array(
    'description' => 'Show available Situs plugins.',
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
    case 'drush:situs-hooks':
      return dt('Lists the plugins available  for Situs.

These plugins may hook into the build process and do additional work when building a site.');
  }
}

/**
 * Pre_validate.
 *
 * If an alias is given as an argument, use it as the active alias.
 */
function drush_situs_build_pre_validate($alias = '') {
  $site = drush_get_context('DRUSH_TARGET_SITE_ALIAS');
  if (empty($site)) {
    $alias = drush_sitealias_get_record($alias);
    if (!empty($alias)) {
      drush_sitealias_set_alias_context($alias, '');
    }
  }
}

/**
 * Validate arguments to build command.
 */
function drush_situs_build_validate($alias = '') {
  if (!($root = drush_get_option('root', FALSE))) {
    return drush_set_error('NO_ROOT', dt('No root directory specified.'));
  }
  if (!is_dir(dirname($root))) {
    return drush_set_error('INVALID_ROOT_PARENT', dt("Root parent doesn't exist, or isn't a directory."));
  }
  if (!drush_get_option('make-file', FALSE)) {
    return drush_set_error('NO_MAKEFILE', dt('No make file specified.'));
  }
}

/**
 * Build command.
 */
function drush_situs_build($alias = '') {
  /* $site_record = drush_sitealias_get_record($alias); */
  $root = drush_get_option('root', FALSE);
  $makefile = drush_get_option('make-file', FALSE);

  $build_path = drush_tempdir() . '/build';

  drush_set_context('SITUS_ROOT_PATH', $root);
  drush_set_context('SITUS_BUILD_PATH', $build_path);

  // @todo document this.
  if (!situs_invoke('pre_build')) {
    return FALSE;
  }

  $settings = array(
    'working-copy' => TRUE,
  );
  $res = drush_invoke_process('@self', 'make', array($makefile, $build_path), $settings, TRUE);

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
  if (!situs_invoke('post_build', $build_path)) {
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
    return drush_set_error('CANNOT_MOVE_NEW_ROOT', dt('Cannot move new site into place. It is located at @path, please figure out what went wrong.'));
  }
  // @xxx Post move hooks?
  // Now delete the old dir.
  if (file_exists($root . '.situs-old')) {
    drush_delete_dir($root . '.situs-old', TRUE);
  }
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
 * List hooks command.
 */
function drush_situs_plugins() {
  foreach (situs_plugins() as $key => $info) {
    drush_print($info['name'] . ' (' . $key . ')');
    drush_print(wordwrap($info['description'], drush_get_context('DRUSH_COLUMNS', 80)));
    drush_print();

    // Sort command options.
    uksort($info['options'], '_drush_help_sort_command_options');

    // Print command sections help.
    foreach (array('options' => dt('Arguments')) as $key => $value) {
      if (!empty($info[$key])) {
        drush_print(dt($value) . ':');
        $rows = drush_format_help_section($info, $key);
        drush_print_table($rows, FALSE, array(40));
        unset($rows);
        drush_print();
      }
    }
  }
}

/**
 * Implements hook_situs_plugin().
 */
function situs_situs_plugin() {
  return array(
    'git-check' => array(
      'name' => 'Git check',
      'description' => 'Checks each git repository in the old site to ensure that there is no uncommitted changes or unpushed commits.',
      'options' => array(
        'git-check' => 'Check that all git repositories contains no uncommitted changes',
      ),
      'pre_build' => 'situs_git_pre_build',
    ),
    // @todo
    'drush' => array(
      'name' => 'Drush command',
      'description' => 'Runs drush commands.',
      'options' => array(
        // @todo Add in more documentation somewhere.
        'drush-pre-build' => 'Array of pre-build commands.',
        'drush-post-build' => 'Array of post-build commands.',
      ),
      'pre_build' => 'situs_drush_pre_build',
      'post_build' => 'situs_drush_post_build',
    ),
  );
}

/**
 * Get defined situs plugins.
 */
function situs_plugins() {
  static $plugins;
  if (!isset($plugins)) {
    $plugins = drush_command_invoke_all('situs_plugin');
    foreach ($plugins as $key => &$info) {
      $info += array(
        'name' => '',
        'description' => '',
        'options' => array(),
      );
    }
  }
  return $plugins;
}

function situs_default_plugin_order($hook) {
  static $orders = array(
    'pre_build' => '*,drush',
    'post_build' => '',
  );
  return $orders[$hook];
}

/**
 * Sorts an array of plugins according to a string spec.
 *
 * The spec is a comma separated list of names, with * functioning as a wildcard
 * for any unspecified plugin. For example will "*,drush" ensure that the drush
 * plugin is called last.
 */
function situs_order_plugins($plugins, $order) {
  $pre = $post = array();
  $seen_star = FALSE;
  $order = explode(',', $order);
  $plugins = array_flip($plugins);
  foreach ($order as $spec) {
    $spec = trim($spec);
    if (isset($plugins[$spec])) {
      unset($plugins[$spec]);
      if ($seen_star) {
        $post[] = $spec;
      }
      else {
        $pre[] = $spec;
      }
    }
    elseif ($spec == '*') {
      $seen_star = TRUE;
    }
  }
  return array_merge($pre, array_keys($plugins), $post);
}

/**
 * Invoke a situs hook, respecting plugin ordering..
 *
 * @param string $hook
 *   The hook to invoke.
 *
 * @returns bool
 *   Whether all hooks was successful.
 */
function situs_invoke($hook) {
  $args = func_get_args();
  array_shift($args);
  $all_plugins = situs_plugins();
  $plugins = array();
  foreach ($all_plugins as $key => $plugin) {
    if (!empty($plugin[$hook])) {
      $plugins[] = $key;
    }
  }
  $order = drush_get_option($hook . '_order', situs_default_plugin_order($hook));
  $plugins = situs_order_plugins($plugins, $order);

  $return = TRUE;
  foreach ($plugins as $plugin) {
    drush_log(dt('Calling @hook Situs @hook_name plugin.', array('@hook' => $plugin, '@hook_name' => $hook)), 'notice');
    $function = $all_plugins[$plugin][$hook];
    $result = call_user_func_array($function, $args);
    // Any hook returning non-true is considered a failure.
    if (isset($result)) {
      $return &= (bool) $result;
    }
  }
  return $return;
}

/**
 * Pre build callback.
 */
function situs_git_pre_build() {
  if (drush_get_option('git-check', FALSE)) {
    $success = TRUE;
    $root = drush_get_option('root', '');
    // If root is not a directory (i.e. on first time build) bail out with
    // success.
    if (!is_dir($root)) {
      return TRUE;
    }
    $git_checkouts = `find $root -type d -name .git`;
    $git_checkouts = array_filter(split("\n", $git_checkouts));
    foreach ($git_checkouts as $filename) {
      $dirname = dirname($filename);
      $uncommitted = `cd $dirname && git status --porcelain`;
      if (!empty($uncommitted)) {
        $success = drush_set_error('UNCOMMITTED_CHANGES', dt("You have uncommitted changes in @dir.", array('@dir' => strtr(dirname($filename), array($root => '')))));
        drush_log($uncommitted, 'notice');
      }
      $unpushed = `cd $dirname && git log --branches --not --remotes --simplify-by-decoration --decorate --oneline`;
      if (!empty($unpushed)) {
        $success = drush_set_error('UNPUSHED_CHANGES', dt("You have unpushed changes in @dir.", array('@dir' => strtr(dirname($filename), array($root => '')))));
        drush_log($unpushed, 'notice');
      }
    }
    return $success;
  }
}

/**
 *
 */
function situs_drush_pre_build() {
  if ($commands = drush_get_option('drush-pre-build', array())) {
    return situs_drush_run_commands($commands);
  }
}

/**
 *
 */
function situs_drush_post_build() {
  if ($commands = drush_get_option('drush-post-build', array())) {
    return situs_drush_run_commands($commands);
  }
}

function situs_drush_run_commands($commands) {
  foreach ($commands as $command) {
    // Support a simple command name.
    if (!is_array($command)) {
      $command = array($command);
    }
    $cmd = $command[0];
    $arguments = isset($command[1]) ? $command[1] : array();
    $options = isset($command[2]) ? $command[2] : array();
    drush_print_r(array($cmd, $arguments, $options));
    $res = drush_invoke_process('@self', $cmd, $arguments, $options, TRUE);

    if (!$res || $res['error_status'] != 0) {
      return drush_set_error('DRUSH_FAILED', dt('Drush command failed.'));
    }
  }
}
