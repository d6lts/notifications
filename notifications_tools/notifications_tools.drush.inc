<?php
// $Id$

/**
 * @file
 *   Drush command to process notifications queue.
 *   
 * Process the queued notifications as a stand alone shell command instead of using cron
 *  
 *  drush --uri=http://www.example.com notifications process [rows] [messages]
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
function notifications_tools_drush_command() {
  $items['notifications process'] = array(
    'callback' => 'notifications_drush_queue_process',
    'description' => "Process queued notifications and send out messages.",
    'arguments' => array(
      'rows' => 'Optional maximum number of rows to process. Zero for no limit.',
      'messages' => 'Optional maximum number of messages to send. Zero for no limit.',
    ),
  );
  return $items;
}

/**
 * Implementation of hook_drush_help().
 */
function notifications_tools_drush_help($section) {
  switch ($section) {
    case 'drush:notifications process':
      return dt("Process queued notifications and send out messages.");
  }
}


/**
 * Drush command callback, process queue
 * 
 * @param $rows
 *   Maximum number of rows to process
 */
function notifications_drush_queue_process($rows = NULL, $messages = NULL) {
  // We need to run our drush hack included with messaging.
  messaging_include('drush_hack.inc');

  $queue = notifications_queue();
  // If passed a time limit we set it before, it will not be overridden by settings.
  if (isset($rows)) {
    $queue->process_limit('row', (int)$rows);
  }
  if (isset($messages)) {
    $queue->process_limit('message', (int)$messages);
  }
  $count = $queue->process_run();
  drush_print(dt("Processed @rows rows, sent @messages messages in @time ms.", notifications_process('results'))); 
}

