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
function notifications_tools_admin_queue($op = 'run') {
  $output = '';
  $output .= drupal_get_form('notifications_tools_queue_operations_form', $op);
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
  $output = '<p>' . t('Queued notifications by send interval and method.') . '</p>';
  // Queue status
  $send_intervals = _notifications_send_intervals();
  $send_methods = _notifications_send_methods();
  $header = array(t('Send interval'), t('Number'));
  $result = db_query("SELECT q.send_interval, d.method, count(*) AS count FROM {notifications_queue} q LEFT JOIN {messaging_destination} d ON q.mdid = d.mdid WHERE q.cron = 1 GROUP BY q.send_interval, d.method");    
  // Store by interval, method 
  $data = array();
  $count = 0;
  while ($item = db_fetch_object($result)) {
    $data[$item->send_interval][$item->method] = $item->count;
    $count += $item->count;
  }
  // Format the results in a table
  $rows = array();
  $header = array_merge(array(''), array_map('check_plain', array_values($send_methods)), array(t('Total')));
  $total_column = array();
  foreach ($send_intervals as $interval => $interval_name) {
    $total_row = 0;
    $row = array(array('header' => TRUE, 'data' => check_plain($interval_name)));
    foreach ($send_methods as $method => $method_name) {
      $value = isset($data[$interval][$method]) ? $data[$interval][$method] : 0;
      $row[] = $value;
      $total_row += $value;
      $total_column[$method] = isset($total_column[$method]) ? $total_column[$method] + $value : $value;
    }
    // Totals per row
    $row[] = $total_row;
    $rows[] = $row;
  }
  // Add totals per column
  $rows[] = array_merge(
    array(array('header' => TRUE, 'data' => t('Total'))),
    $total_column,
    array(array_sum($total_column))
  );
  $output .= theme('table', $header, $rows);
  $output .= t('Total: %number', array('%number' => $count));
  return theme('box', t('Notifications in queue'), $output);
}

/**
 * Form for queue operations
 */
function notifications_tools_queue_operations_form($form_state, $op = 'run') {
  $form['operation'] = array(
    '#type' => 'radios',
    '#title' => t('Operation'),
    '#options' => array(),
    '#default_value' => $op ? $op : 'run',
    //'#default_value' => isset($form_state['values']['mode']) ? $form_state['values']['mode'] : 'run',
  );
  foreach (notifications_tools_queue_operations() as $op => $data) {
    $form['operation']['#options'][$op] = $data['description'];
  }
  $form['process'] = array(
    '#type' => 'submit',
    '#value' => t('Process queue'),
  );
  return $form;
}

/**
 * Operations form submit, translate op into callback
 */
function notifications_tools_queue_operations_form_submit($form, &$form_state) {
  global $user;

  notifications_include('process.inc');
  // Enable messaging & notifications logs
  messaging_log_start();
  // Switch to anonymous user so this works with anonymous permissions
  session_save_session(FALSE);
  $save_user = $user;
  $user = drupal_anonymous_user();
  $operations = notifications_tools_queue_operations();
  if ($mode = $form_state['values']['operation']) {
    $process = $operations[$mode];
    $args = !empty($process['callback arguments']) ? $process['callback arguments'] : array();
    call_user_func_array($process['callback'], $args);
    drupal_set_message($process['description']);
    $form_state['redirect'] = 'admin/messaging/subscriptions/queue/' . $mode;
  } 
  // Switch back to current user
  $user = $save_user;
  session_save_session(TRUE);
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
      'label' => t('Test processing'),
      'description' => t('Test run queue processing, without updating nor sending messages.'),
      'callback' => 'notifications_tools_queue_process',
      'callback arguments' => array('test run'),
    ),
    'testsend' => array(
      'label' => t('Test sending'),
      'description' => t('Test run queue processing, without updating but actually sending messages.'),
      'callback' => 'notifications_tools_queue_process',
      'callback arguments' => array('test send'),
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
    case 'test send':
      notifications_process('option', 'keep', TRUE);
      notifications_process_run(FALSE);
      break;
    case 'clean queue':
      // Delete logs and clean up events
      notifications_queue_clean(array('cron' => 0));
      break;      
  }
}
