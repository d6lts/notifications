<?php
// $Id$

/**
 * @file
 *   Drush command to process notifications queue.
 *   
 * Process the queued notifications as a stand alone shell command instead of using cron
 *  
 *  drush --uri=http://www.example.com notifications_process [rowlimit]
 *  
 * Notes: 
 * - To properly generate links the base url must be set. You may need to set it manually in settings.php
 * - Before using this you will want to disable cron for Notifications, there's an option for that on Admin pages.
 * - The limits for queue processing (See Admin pages) still apply, though the row limit can be overriden with [rowlimit]
 * - The autoload hack is needed by autoload module, http://drupal.org/node/547736
 */

/**
 * Implementation of hook_drush_command().
 * 
 * @see drush_parse_command() for a list of recognized keys.
 *
 * @return
 *   An associative array describing your command(s).
 */
function notifications_drush_command() {
  $items['notifications_process'] = array(
    'callback' => 'notifications_drush_queue_process',
    'description' => "Process queued notifications and send out messages.",
    'arguments' => array(
      'rows' => 'Optional maximum number of rows to process. Zero for no limit. If not set the default from module settings will apply.',
      'messages' => 'Optional maximum number of messages to send. Zero for no limit. If not set the default from module settings will apply.',
    ),
  );
  return $items;
}

/**
 * Drush command callback, process queue
 * 
 * @param $rows
 *   Maximum number of rows to process
 */
function notifications_drush_queue_process($rows = NULL, $messages = NULL) {
  // We need to override autoload_class() so it works with Drush.
  // The prepend parameter is PHP 5.3 so we better unregister autoload_class() too
  spl_autoload_register('notifications_drush_autoload_class');
  spl_autoload_unregister('autoload_class');

  notifications_include('process.inc');
  // If passed a time limit we set it before, it will not be overridden by settings.
  if (isset($rows)) {
    notifications_process('limit', 'row', (int)$rows);
  }
  if (isset($messages)) {
    notifications_process('limit', 'message', (int)$messages);
  }
  $count = notifications_process_run(FALSE);
  drush_print(dt("Processed @rows rows, sent @messages messages in @time ms.", notifications_process('results'))); 
}

/**
 * Autoload hack, so autoload paths work with drush
 */
function notifications_drush_autoload_class($class) {
  static $lookup;

  if (!isset($lookup)) {
    $lookup = autoload_get_lookup();
  }

  if (!empty($lookup[$class]) && ($path = realpath($lookup[$class]))) {
    require $path;
  }
}
