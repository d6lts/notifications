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
  $form['sub_settings']['notifications_send_immediate'] = array(
    '#title' => t('Immediate sending'),
    '#type' => 'checkbox',
    '#size' => 10,
    '#default_value' => variable_get('notifications_send_immediate', 0),
    '#description' => t('Do not queue notifications for immediate sending. This will produce more timely notifications for sites with a small number of users.'),
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
  $limit = variable_get('notifications_process_limit', array('row' => 0, 'message' => 0, 'percent' => 0, 'time' => 0));
  $form['notifications_process_limit'] = array(
    '#type' => 'fieldset',
    '#title' => t('Limits for queue processing'),
    '#tree' => TRUE,
    '#description' => t('These are the limits for each cron run on queue processing. The process will stop when it first meets any of them. Set to 0 for no limit.'),
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
  $rows = array();
  
  // List types and count for each type
  foreach (notifications_subscription_types() as $type => $info) {
    $access = user_access('administer notifications') || !empty($info['access']) && user_access($info['access']);
    // If no access and no count, skip this type.
    // But if no access and there are some we show the type and number without link.
    if ($access || !empty($count[$type])) {
      $rows[] = array(
         $access ? l($info['title'], "user/$account->uid/notifications/$type") : $info['title'],
         isset($count[$type]) ? $count[$type] : 0,
      );
    }
  }
  if ($rows) {
    $output .= theme('table', $header, $rows);
  } else {
    $output .= t('No existing or allowed subscriptions');
  }
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
 * 
 * @ TO DO Add confirmation before queue reset
 */
function notifications_admin_queue($op = 'status', $param = NULL) {
  $base = 'admin/notifications/status/queue';
  $output = '';
  switch ($op) {
    case 'run':
      $out = notifications_admin_queue_process($param);
      if ($out) {
        $output .= theme('box', t('Output'), $out);
      }
      break;
    case 'reset':
      db_query("DELETE FROM {notifications_queue}");
      db_query("DELETE FROM {notifications_event}");
      drupal_set_message(t('The queue has been reset.'));
      drupal_goto($base);
      break;
    default:
  }
  // Add operations
  $list[] = l(t('Run queue process'), "$base/run");
  $list[] = l(t('Process immediate sending'), "$base/run/immediate");
  $list[] = l(t('Reset queue. Delete all notifications.'), "$base/reset");

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
    case 'immediate':
      notifications_process_rows(array('cron' => 1, 'send_interval' => 0));
      break;
    case 'debug':
      notifications_process('option', 'debug', TRUE);
      notifications_process('option', 'output', TRUE);
      notifications_process_run(FALSE);
      break;
    default:
      notifications_process_run(FALSE);
  }
  // Go for it, not cron run

  if ($logs = notifications_log()) {
    return theme('item_list', $logs);
  }
}

/**
 * Menu callback add subscription
 * 
 * Presents confirmation page or not depending on confirm parameter
 */
function notifications_page_subscribe($uid, $type, $fields, $values, $send_interval = NULL, $send_method = NULL) {
  global $user;

  // Access checking
  if ($uid && ($uid == $user->uid || user_access('administer notifications'))) {
    if (($account = user_load(array('uid' => $uid)))) {
      // Build subscriptions object
      $subscription = (object)array(
        'uid' => $uid,
        'type' => $type,
        'fields' => notifications_field_args($fields, $values),
        'send_interval' => $send_interval ? $send_interval : notifications_user_setting('send_interval', $account),
        'send_method' => $send_method ? $send_method : notifications_user_setting('send_method', $account),
        'event_type' => notifications_subscription_types($type, 'event_type'),
      );      
      if (notifications_user_allowed('subscription', $account, $subscription)) {
        // Display subscription information
        if (empty($_GET['confirm'])) {
          // Subscribe, no confirmation
          notifications_save_subscription($subscription);
          drupal_goto();
        } else {
          // Ask for confirmation
          drupal_set_title(t('Confirm your subscription'));
          return drupal_get_form('notifications_form_confirm', $subscription);
        }
      } else {
        drupal_set_message(t('Subscription type or parameters not allowed'), 'error');
        drupal_goto();
      }
    }
    
  }
  drupal_access_denied();
}

/**
 * Form for subscription confirmation
 */
function notifications_form_confirm($subscription) {

  // Pass on simple values
  foreach (array('sid', 'uid', 'type', 'fields', 'event_type') as $field) {
    $form[$field] = array('#type' => 'value', '#value' => $subscription->$field);
  }

  // The names will be added here
  notifications_module_invoke('names', $subscription);

  $form['info'] = array(
    '#type' => 'item',
    '#title' => t('!type subscription to', array('!type' => $subscription->type_name)),
    '#value' => theme('item_list', $subscription->names),
  );

  // Additional parameters
  $form['send_interval'] = array(
    '#type' => 'select',
    '#title' => t('Send interval'),
    '#options' => _notifications_send_intervals(),
    '#default_value' => $subscription->send_interval,
  );
  $form['send_method'] = array(
    '#type' => 'select',
    '#title' => t('Send method'),
    '#options' => _notifications_send_methods(),
    '#default_value' => $subscription->send_method,
  );

  $form['confirm'] = array('#type' => 'submit', '#value' => t('Subscribe'));
  $form['cancel'] = array('#type' => 'submit', '#value' => t('Cancel'));
  return $form;
}

/**
 * Process form submission
 */
function notifications_form_confirm_submit($form_id, $form_values) {
  $subscription = (object)$form_values;
  switch($form_values['op']) {
    case t('Subscribe'):
      notifications_save_subscription($subscription);
      drupal_set_message(t('Your subscription was activated.'));
      break;
    case t('Cancel'):
      drupal_set_message(t('Your subscription was cancelled'));
      break;
  }
  return 'user/'.$form_values['uid'].'/notifications';   
}

/**
 * Process arguments and return an array of field/value pairs
 */
function notifications_field_args($fields, $values) {
  $names = explode(',', $fields);
  $params = explode(',', $values);
  return array_combine($names, $params);  
}

/**
 * Menu callback add subscription
 * 
 * This just admits one field/value pair
 */
function notifications_page_unsubscribe($sid) {
  global $user;
  
  if (is_numeric($sid) && ($subscription = notifications_load_subscription($sid)) ) {
    if (user_access('administer notifications') ||
      ($user->uid && $user->uid == $subscription->uid) ||
      (!empty($_GET['signature']) && ($_GET['signature'] == _notifications_signature(array('unsubscribe', $sid))))
    ) {
      notifications_delete_subscription($sid);
      drupal_set_message(t('Your subscription has been removed.'));
      drupal_goto();
      return;
    }
  }
  drupal_access_denied();
}

/**
 * Generic subscriptions content form
 * 
 * Builds a form for a user to manage its own subscriptions with
 * some generic parameters
 * 
 * Currently it only manages simple condition fields
 * @param $account 
 *   User account
 * @param $type
 *   Subscription type
 * @param $subscriptions
 *   Current subscriptions of this type. If null they'll be loaded
 * @param $list
 *   Array with available subscriptions indexed by field value
 * @param $defaults
 *   Default value for subscriptions
 * @param $options
 *   Optional, array of aditional options for the form
 */
function notifications_user_form($account, $type, $subscriptions, $list, $defaults, $options = array()) {
  // Complete defaults
  $info = notifications_subscription_types($type);
  $field = $info['fields'][0];
  $field_title = !empty($options['title']) ? $options['title'] : '';
  if (is_null($subscriptions)) {
    // Fetch subscriptions with given parameters
    $subscriptions = notifications_get_subscriptions(array('type' => $type, 'event_type' => $info['event_type'], 'uid' => $account->uid), TRUE, 'value');
  }
  $defaults += array(
    'sid' => 0,
    'type' => $type,
    'event_type' => $info['event_type'],
  );
  $defaults += _notifications_subscription_defaults($account);
  $form['defaults'] = array('#type' => 'value', '#value' => $defaults);  
  $form['account'] = array('#type' => 'value', '#value' => $account);
  $form['current'] = array('#type' => 'value', '#value' => $subscriptions);
  $form['subscription_fields'] = array('#type' => 'value', '#value' => array());
  $form['subscriptions'] = array(
    '#tree' => TRUE,
    '#theme' => 'notifications_form_table',
    '#header' => array('', $field_title, t('Send interval'), t('Send method'))
  );  
  foreach ($list as $key => $title) {
    $rowdefaults = isset($subscriptions[$key]) ? (array)($subscriptions[$key]) : $defaults;
    $rowdefaults += $rowdefaults;
    $form['subscriptions']['checkbox'][$key] = array(
      '#type' => 'checkbox',
      '#default_value' => $rowdefaults['sid'],
    );
    $form['subscriptions']['title'][$key] = array(
      '#value' => $title,
    );
    $form['subscriptions']['send_interval'][$key] = array(
      '#type' => 'select',
      '#options' => _notifications_send_intervals(),
      '#default_value' => $rowdefaults['send_interval'],
    );
    $form['subscriptions']['send_method'][$key] = array(
      '#type' => 'select',
      '#options' => _notifications_send_methods(),
      '#default_value' => $rowdefaults['send_method'],
    );
    // Pass on the fields for processing
    $form['subscription_fields']['#value'][$key] = array($field => $key);
  
  }
  $form['submit'] = array('#type' => 'submit', '#value' => t('Save'));

  return $form; 
}

/**
 * Process generic form submission
 */
function notifications_user_form_submit($form_id, $form_values) {
  $account = $form_values['account'];
  $current = $form_values['current'];
  $defaults = $form_values['defaults'];
  $defaults += array('uid' => $account->uid);
  $fields = $form_values['subscription_fields'];
  $values = $form_values['subscriptions'];
  $check = 'checkbox';

  foreach ($values[$check] as $index => $value) {
    $subscription = NULL;
    if ($value) {
      // Checked, save only if new or changed
      if (!isset($current[$index])) {
        $subscription = $defaults;
      } elseif ($current[$index]->send_interval != $values['send_interval'][$index] || $current[$index]->send_method != $values['send_method'][$index]) {
        $subscription = (array)($current[$index]);        
      }
      // Complete and save
      if ($subscription) {
        $subscription['send_interval'] = $values['send_interval'][$index];
        $subscription['send_method'] = $values['send_method'][$index];
        $subscription['fields'] = $fields[$index];
        notifications_save_subscription($subscription);
      }
    } elseif(isset($current[$index])) {
      notifications_delete_subscription($current[$index]->sid);
    }
  }
}
