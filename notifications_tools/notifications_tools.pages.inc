<?php
// $Id$

/**
 * @file
 * Notifications tools pages
 */


/**
 * Admin settings
 */
function notifications_tools_queue_settings() {
  $form['notifications_process_on_cron'] = array(
    '#type' => 'radios',
    '#title' => t('Process on cron'),
    '#default_value' => variable_get('notifications_process_on_cron', TRUE),
    '#options' => array(t('Disabled'), t('Enabled')),
    '#description' => t('If disabled you must use code elsewhere to actually send your e-mails such as a <a href="http://drupal.org/project/drush">drush</a> script.'),
  );
  
  // Logging settings  
  $period = array(0 => t('Disabled')) + drupal_map_assoc(array(3600, 10800, 21600, 32400, 43200, 86400, 172800, 259200, 604800, 1209600, 2419200, 4838400, 9676800), 'format_interval');
  $form['general']['notifications_log'] = array(
    '#title' => t('Logging'),
    '#type' => 'select',
    '#options' => $period,
    '#default_value' => variable_get('notifications_log', 0),
    '#description' => t('If enabled all notifications will be logged and kept for the specified time after they\'re processed. These logs are kept into the <i>notifications_queue</i> table in the database.'),
  );

  // Processing limits
  $limit = variable_get('notifications_process_limit', array('row' => 0, 'message' => 0, 'percent' => 0, 'time' => 0));
  $form['notifications_process_limit'] = array(
    '#type' => 'fieldset',
    '#title' => t('Limits for queue processing'),
    '#tree' => TRUE,
    '#description' => t('These are the limits for each run on queue processing. The process will stop when it first meets any of them. Set to 0 for no limit.'),
    '#collapsible' => TRUE,
  );

  $form['notifications_process_limit']['row'] = array(
    '#title' => t('Number of rows'),
    '#type' => 'textfield',
    '#size' => 10,
    '#default_value' => $limit['row'],
  );
  $form['notifications_process_limit']['message'] = array(
    '#title' => t('Number of messages sent'),
    '#type' => 'textfield',
    '#size' => 10,
    '#default_value' => $limit['message'],
  );
  $form['notifications_process_limit']['time'] = array(
    '#title' => t('Time (seconds)'),
    '#type' => 'textfield',
    '#size' => 10,
    '#default_value' => $limit['time'],
  );
  $form['notifications_process_limit']['percent'] = array(
    '#title' => t('Time (% of cron time)'),
    '#type' => 'textfield',
    '#size' => 10,
    '#default_value' => $limit['percent'],
    '#description' => t('Maximum percentage of cron time the process may use.'),
  );
  return system_settings_form($form);
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
 * Summary of queued notifications
 */
function notifications_tools_queue_summary() {
  $output = '<p>' . t('Queued notifications by send interval and method.') . '</p>';
  // Queue status
  $send_intervals = _notifications_send_intervals();
  $send_methods = _notifications_send_methods();
  $header = array(t('Send interval'), t('Number'));
  $count = notifications_queue()->queue_count(array('cron' => 1));
  $queue_status = notifications_queue()->queue_status(array('send_interval', 'send_method'));
  // Compile by interval, method 
  $data = array();
  foreach ($queue_status as $item) {
    $data[$item->send_interval][$item->send_method] = $item->count;
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
    $form_state['redirect'] = 'admin/messaging/notifications/queue/operations/' . $mode;
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
      'callback' => 'notifications_tools_queue_process',
      'callback arguments' => array('process run'),
    ),
    'immediate' => array(
      'label' => t('Process immediate'),
      'description' => t('Process only rows marked for immediate sending.'),
      'callback' => 'notifications_tools_queue_process',
      'callback arguments' => array('process rows', array('cron' => 1, 'send_interval' => 0)),
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
function notifications_tools_queue_process($op, $args = NULL) {
  switch ($op) {
    case 'process run':
      notifications_queue()->process_run();
      break;
    case 'process rows':
      notifications_queue()->process_rows($args);
      break;
    case 'reset queue':
      notifications_queue()->queue_reset();
      drupal_set_message(t('The queue has been reset.'));
      break;
    case 'test run':
      notifications_queue()->process_option('test', TRUE);
      notifications_queue()->process_run();
      break;
    case 'test send':
      notifications_queue()->process_option('keep', TRUE);
      notifications_queue()->process_run();
      break;
    case 'clean queue':
      // Delete logs and clean up events
      notifications_queue()->queue_clean(array('cron' => 0));
      break;      
  }
}

