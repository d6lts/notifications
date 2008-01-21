<?php
// $Id$

/**
 * Admin settings
 */
function notifications_settings_form() {
  $form['sub_settings'] = array(
      '#type' => 'fieldset',
      '#title' => t('General settings'),
      '#weight' => -10,
  );
  $form['sub_settings']['notifications_sendself'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Notify poster of own posts'),
    '#default_value' => variable_get('notifications_sendself', 0),
    '#description'   => t("Notifies a node poster about their own posts.  Useful principally during testing.  Default is OFF."),
  );
  $form['sub_settings']['notifications_watchgood'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Display watchdog entries for successful mailings'),
    '#default_value' => variable_get('notifications_watchgood', 1),
    '#description'   => t('Inserts notification of successful mailings in the watchdog log.  Default is ON.'),
  );
  $form['sub_settings']['notifications_testpost'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Test held posts prior to sending'),
    '#default_value' => variable_get('notifications_testpost', 0),
    '#description'   => t('Tests to see if a post about to be sent by cron is still active.  Adds a small amount of overhead.  Default is OFF.'),
  );
  $form['sub_settings']['notifications_autoset'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Set all users to "autosubscribe" by default'),
    '#default_value' => variable_get('notifications_autoset', 0),
    '#description'   => t('Sets each users "autosubscribe" profile option. Default is OFF.'),
  );
  // Default options
  $form['defaults'] = array(
      '#type' => 'fieldset',
      '#title' => t('Default settings'),
  );
  $form['defaults']['notifications_default_send_interval'] = array(
    '#type' => 'select',
    '#title' => t('Default send interval'),
    '#options' => _notifications_send_intervals(),
    '#default_value' => variable_get('notifications_default_send_interval', 0),
  );
  $form['defaults']['notifications_default_send_method'] = array(
    '#type' => 'select',
    '#title' => t('Default send method'),
    '#options' => _notifications_send_methods(),
    '#default_value' => variable_get('notifications_default_send_method', ''),
  );

  // Processing limits
  $limit = variable_get('notifications_process_limit', array('row' => 0, 'message' => 0, 'step' => 0, 'time' => 0));
  $form['notifications_process_limit'] = array(
    '#type' => 'fieldset',
    '#title' => t('Limits for queue processing'),
    '#tree' => TRUE,
    '#description' => t('These are the limits for each cron run on queue processing. Set to 0 for no limit.'),
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
  $form['notifications_process_limit']['step'] = array(
    '#title' => t('Process steps'),
    '#type' => 'textfield',
    '#size' => 10,
    '#default_value' => $limit['step'],
    '#description' => t('One process step is counted for each user, send method and send interval'),
  );
  return system_settings_form($form);
}


/* ******************************************************* */
/*  user screens: display, edit functions */
/* ******************************************************* */

/**
 * Theme subscriptions list
 */
function theme_notifications_form_table($element) {
  $output = '';
  if ($fields = element_children($element)) {
    $header = $element['#header'];
    $rows = array();
    // The first element determines the number of columns
    foreach (element_children($element[$fields[key($fields)]]) as $index) {
      $row = array();
      foreach ($fields as $key) {
        $row[] = isset($element[$key][$index]) ? drupal_render($element[$key][$index]) : '';
      }
      $rows[] = $row;
    }
    $output .= theme('table', $header, $rows);
  }
  $output .= drupal_render($element);
  return $output;
}

/**
 * Menu callback. Overview page for user subscriptions.
 */
function notifications_page_user_overview($account) {
  // Build summary
  $count = array();  
  // Query all subscriptions for this user
  $result = db_query("SELECT s.type, count(s.sid) as number FROM {notifications} s WHERE s.uid = %d  GROUP BY s.type", $account->uid);
  while ($subs = db_fetch_object($result)) {
    $count[$subs->type] =  $subs->number;
  }

  $header = array(t('Type'), t('Number'));
  foreach (notifications_subscription_types() as $type => $info) {
    $rows[] = array(
        l($info['title'], "user/$account->uid/notifications/$type"),
        isset($count[$type]) ? $count[$type] : 0,
    );
  }
  $output .= theme('table', $header, $rows);
  return $output;
}

/** Administration pages **/

/**
 * Admin overview page
 */
function notifications_admin_overview_page() {
  $menu = menu_get_item(NULL, 'admin/notifications');
  $content = system_admin_menu_block($menu);
  $output = theme('admin_block_content', $content);
  return $output;
}

/**
 * Current subscriptions page
 */
function notifications_admin_status_page() {
  $output = '';
  // Subscriptions summary
  $header = array(t('Type'), t('Number'));
  $result = db_query("SELECT type, count(*) AS count FROM {notifications} GROUP BY type");
  $count = 0;
  while ($stype = db_fetch_object($result)) {
    $rows[] = array($stype->type, $stype->count);
    $count += $stype->count;
  }
  $summary = theme('table', $header, $rows);
  $summary .= t('Total: %number', array('%number' => $count));
  $output .= theme('box', t('Current subscriptions'), $summary);
  $output .= notifications_admin_queue_summary();
  return $output;  
}

/**
 * Summary of queued notifications
 */
function notifications_admin_queue_summary() {
  $output = '';
  // Queue status
  $send_intervals = _notifications_send_intervals();
  $header = array(t('Send interval'), t('Number'));
  $count = 0;
  $result = db_query("SELECT send_interval, count(*) AS count FROM {notifications_queue} GROUP BY send_interval");    
  while ($stype = db_fetch_object($result)) {
    $rows[] = array($send_intervals[$stype->send_interval], $stype->count);
    $count += $stype->count;
  }
  $output .= theme('table', $header, $rows);
  $output .= t('Total: %number', array('%number' => $count));
  return theme('box', t('Messages in queue'), $output);
}

/**
 * Admin queue management
 */
function notifications_admin_queue($op = 'status', $param = NULL) {
  $output = '';
  switch ($op) {
    case 'run':
      $out = notifications_admin_queue_process($param);
      if ($out) {
        $output .= theme('box', t('Output'), $out);
      }
    default:
  }
  // Add operations
  $base = 'admin/notifications/status/queue';
  $list[] = l(t('Run queue process'), "$base/run");
  $list[] = l(t('Run queue process on debug mode (rows not deleted)'), "$base/run/debug");
  $output .= theme('box', t('Operations'), theme('item_list', $list));
  // Summary
  $output .= notifications_admin_queue_summary();
  return $output;
}

/**
 * Admin manual queue processing
 */
function notifications_admin_queue_process($param) {
  include_once drupal_get_path('module', 'notifications') .'/notifications.cron.inc';
  // Set some running parameters
  switch  ($param) {
    case 'debug':
      notifications_process('option', 'debug', TRUE);
      notifications_process('option', 'output', TRUE);
      break;
  }
  // Go for it
  notifications_process_run();
  if ($logs = notifications_log()) {
    return theme('item_list', $logs);
  }
}