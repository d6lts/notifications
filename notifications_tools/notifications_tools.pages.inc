<?php
// $Id$

/**
 * @file
 * Notifications tools pages
 */

/**
 * Current subscriptions page
 */
function notifications_tools_status_page() {
  $output = '';
  
  // Subscriptions summary by type
  $header = array(t('Type'), t('Number'));
  $result = db_query("SELECT type, count(*) AS count FROM {notifications} GROUP BY type");
  $count = 0;
  $types = notifications_subscription_types();
  $rows = array();
  while ($stype = db_fetch_object($result)) {
    $rows[] = array(
      !empty($types[$stype->type]['title']) ? $types[$stype->type]['title'] : '<strong>' . $stype->type . '</strong>' , 
      $stype->count);
    $count += $stype->count;
  }
  $summary = theme('table', $header, $rows);
  $summary .= t('Total: %number', array('%number' => $count));
  $output .= theme('box', t('Subscriptions by type'), $summary);
  
  // Summary by sending method
  $header = array(t('Method'), t('Number'));
  $result = db_query("SELECT send_method, count(*) AS count FROM {notifications} GROUP BY send_method");
  $rows = array();
  while ($subs = db_fetch_object($result)) {
    $name = messaging_method_info($subs->send_method, 'title');
    $rows[] = array(
      $name ? $name : '<strong>' . $subs->send_method . '</strong>' , 
      $subs->count);
  }
  $summary = theme('table', $header, $rows);
  $output .= theme('box', t('Subscriptions by send method'), $summary); 
  
  // Queue summary 
  $output .= notifications_tools_queue_summary();
  return $output;  
}

/**
 * Admin queue management
 * 
 * @ TO DO Add confirmation before queue reset
 */
function notifications_tools_admin_queue($op = 'status', $param = NULL) {
  $output = '';
  $output .= drupal_get_form('notifications_tools_queue_operations_form');
  $output .= notifications_tools_queue_summary();

  // Display logs from last process
  if ($logs = messaging_log_get()) {
    $output .= theme('box', t('Process log'), messaging_log_format($logs));
  }
  return $output;
}

/**
 * Menu callback: subscriptions administration.
 */
function notifications_tools_manage_subscriptions($form_state) {
  module_load_include('manage.inc', 'notifications');
  if (isset($form_state['values']['operation']) && $form_state['values']['operation'] == 'delete') {
    return notifications_multiple_delete_confirm($form_state, array_filter($form_state['values']['subscriptions']));
  }
  $form = notifications_subscriptions_filter_form(TRUE);

  $form['#theme'] = 'notifications_subscriptions_filter_form';
  $form['admin']  = notifications_manage_subscriptions_form();

  return $form;
}

/**
 * Summary of queued notifications
 */
function notifications_tools_queue_summary() {
  $output = '';
  // Queue status
  $send_intervals = _notifications_send_intervals();
  $header = array(t('Send interval'), t('Number'));
  $count = 0;
  $result = db_query("SELECT send_interval, count(*) AS count FROM {notifications_queue} WHERE cron = 1 GROUP BY send_interval");    
  $rows = array();
  while ($stype = db_fetch_object($result)) {
    $rows[] = array($send_intervals[$stype->send_interval], $stype->count);
    $count += $stype->count;
  }
  $output .= theme('table', $header, $rows);
  $output .= t('Total: %number', array('%number' => $count));
  return theme('box', t('Notifications in queue'), $output);
}

/**
 * Form for queue operations
 */
function notifications_tools_queue_operations_form() {
  $form['operations'] = array(
    '#type' => 'fieldset',
    '#title' => t('Operations'),
    '#theme' => 'notifications_table_form',
    '#table_fields' => array('description', 'button'),
  );
  foreach (notifications_tools_queue_operations() as $op => $data) {
    $form['operations']['description'][$op] = array(
      '#value' => $data['description'],
    );
    $form['operations']['button'][$op] = array(
      '#type' => 'submit',
      '#value' => $data['label'],
    );
  }
  return $form; 
}

/**
 * Operations form submit, translate op into callback
 */
function notifications_tools_queue_operations_form_submit($form, $form_state) {
  include_once drupal_get_path('module', 'notifications') .'/notifications.cron.inc';
  // Enable messaging & notifications logs
  messaging_log_start();
  foreach (notifications_tools_queue_operations() as $op => $data) {
    if ($form_state['values']['op'] == $data['label']) {
      $callback = $data['callback'];
      $args = !empty($data['callback arguments']) ? $data['callback arguments'] : array();
      call_user_func_array($callback, $args);
      break;
    }  
  }
}

/**
 * List of queue operations
 */
function notifications_tools_queue_operations() {
  $operations = array(
    'run' => array(
      'label' => t('Run process'),
      'description' => t('Run normal queue processing, same as cron run.'),
      'callback' => 'notifications_process_run',
      'callback arguments' => array(FALSE),
    ),
    'immediate' => array(
      'label' => t('Process immediate'),
      'description' => t('Process only rows marked for immediate sending.'),
      'callback' => 'notifications_process_rows',
      'callback arguments' => array(array('cron' => 1, 'send_interval' => 0)),
    ),
    'clean' => array(
      'label' => t('Clean queue'),
      'description' => t('Delete all logs, processed rows and related data in queue.'),
      'callback' => 'notifications_tools_queue_process',
      'callback arguments' => array('clean queue'),
    ),
    'reset' => array(
      'label' => t('Reset queue'),
      'description' => t('Delete all notifications in queue.'),
      'callback' => 'notifications_tools_queue_process',
      'callback arguments' => array('reset queue'),
    ),
    'test' => array(
      'label' => t('Run test'),
      'description' => t('Test run queue processing, without updating nor sending messages.'),
      'callback' => 'notifications_tools_queue_process',
      'callback arguments' => array('test run'),
    ),
  );
  return $operations;
}

/**
 * Queue operations callback
 */
function notifications_tools_queue_process($op) {
  switch ($op) {
    case 'reset queue':
      db_query("DELETE FROM {notifications_queue}");
      db_query("DELETE FROM {notifications_event}");
      drupal_set_message(t('The queue has been reset.'));
      break;
    case 'test run':
      notifications_process('option', 'test', TRUE);
      notifications_process_run(FALSE);
      break;
    case 'clean queue':
      // Delete logs and clean up events
      notifications_queue_clean(array('cron' => 0));
      break;      
  }
}
