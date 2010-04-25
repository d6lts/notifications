<?php
// $Id$

/**
 * Admin settings
 */
function notifications_settings_form() {
  $form['general'] = array(
      '#type' => 'fieldset',
      '#title' => t('General settings'),
      '#weight' => -10,
  );
  $form['general']['notifications_sendself'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Notify poster of own posts'),
    '#default_value' => variable_get('notifications_sendself', 0),
    '#description'   => t("Notifies a node poster about their own posts.  Useful principally during testing.  Default is OFF."),
  );
  $form['general']['notifications_send_immediate'] = array(
    '#title' => t('Immediate sending'),
    '#type' => 'checkbox',
    '#default_value' => variable_get('notifications_send_immediate', 0),
    '#description' => t('Notifications are usually queued to be sent on cron process later. Checking this option will cause immediate notifications to be sent right away, instead of being queued. This will produce more timely notifications for sites with a small number of users. Not recommended for sites with a large number of users. This may not work for AJAX forms and notifications will be queued anyway.'),
  );
  $form['general']['notifications_sender'] = array(
    '#title' => t('Notifications Sender'),
    '#type' => 'radios',
    '#options' => array(
      t('No one (All notifications will appear as coming from the web site)'),
      t('User name, site data (Only the user name will be used)'),
      t('Full user data (User name and available user information)'),
    ),
    '#default_value' => variable_get('notifications_sender', 0),
    '#description' => t('Use the site information as the sender for notification messages or use account data from the user causing the event. WARNING: Selecting the last option (Full user data) may disclose private information to subscribers like the user e-mail address.'),
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

  // Processing limits
  $limit = variable_get('notifications_process_limit', array('row' => 0, 'message' => 0, 'percent' => 0, 'time' => 0));
  $form['notifications_process_limit'] = array(
    '#type' => 'fieldset',
    '#title' => t('Limits for queue processing'),
    '#tree' => TRUE,
    '#description' => t('These are the limits for each run on queue processing. The process will stop when it first meets any of them. Set to 0 for no limit.'),
    '#collapsible' => TRUE, '#collapsed' => TRUE,
  );
  $form['notifications_process_on_cron'] = array(
    '#type' => 'checkbox',
    '#title' => t('Process on cron'),
    '#default_value' => variable_get('notifications_process_on_cron', TRUE),
    '#description' => t('If disabled you must use code elsewhere to actually send your e-mails such as a <a href="http://drupal.org/project/drush">drush</a> script.'),
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
 * Send intervals administration
 */
function notifications_send_intervals_form() {
  notifications_include('process.inc');
  // Collect information about digesting methods and print out some help
  // Collect information about building methods and print out some help
  $form['build'] = array(
    '#title' => t('Build formats'),
    '#type' => 'fieldset',
    '#description' => t('These are the building formats available for each interval.')
  );
  foreach (notifications_process_build_method() as $type => $method) {
    $build_methods[$type] = $method['name'];
    $rows[] = array($method['name'], $method['description']);    
  }
  $form['build']['info'] = array('#value' => theme('table', array(), $rows));
  
  // Build options. Need some complex formatting.
  $units = array(
    60 => t('Minutes'),
    60*60 => t('Hours'), 
    24*60*60 => t('Days'),
  );
  $form['intervals'] = array(
    '#type' => 'fieldset',
    '#title' => t('Send intervals'),
    '#tree' => TRUE,
    //'#theme' => 'notifications_send_intervals',
    '#description' => t('To delete an interval, set the time unit and the name empty. If the time is negative, notifications will never be sent.'),
  );
  $intervals = _notifications_send_intervals() + array('new1' => '', 'new2' => '');
  // We need to use different indexes (not time value) as negative ones won't work
  $index = 0;
  $index_name = $time_index = array();
  foreach ($intervals as $time => $name) {
    // Store index => name mapping
    $index_options[$index] = '';
    $time_index[$time] = $index;
    // Calculate value and unit
    $current = $number = 0; 
    if (!is_numeric($time)) {
      $number = $unit = '';
    }
    elseif ($time >= 0) {
      foreach (array_reverse(array_keys($units)) as $unit) {
        if ($time % $unit == 0) {
          $current = $unit;
          $number = $time / $unit;
          break;
        }
      }
    }
    else {
      // May be -1 for 'Never'
      $number = $time;
      $unit = '';
    }
    $form['intervals'][$index]['time'] = array(
      '#default_value' => $number,
      '#type' => 'textfield',
      '#size' => 2,
    );
    $form['intervals'][$index]['unit'] = array(
      '#type' => 'select',
      '#options' => array('' => '') + $units,
      '#default_value' => $unit,
    );
    $form['intervals'][$index]['name'] = array(
      '#type' => 'textfield',
      '#default_value' => $name,
      '#size' => 40,
    );
    $build = notifications_process_build_method($time);
    $form['intervals'][$index]['build'] = array(
      '#type' => 'select',
      '#disabled' => ($number < 0),
      '#default_value' => $build ? $build['type'] : 'simple',
      '#options' => $build_methods,
    );
    // Store also index -> time mapping
    $form['intervals'][$index]['key'] = array('#type' => 'value', '#value' => $time);
    $index++;
  }
  $form['default'] = array('#type' => 'radios',
    '#options' => $index_options,
    '#default_value' => $time_index[variable_get('notifications_default_send_interval', 0)],
  );
  // New row
  $form['submit'] = array('#type' => 'submit', '#value' => t('Update'));
  $form['reset'] = array('#type' => 'submit', '#value' => t('Reset to defaults'));

  return $form;
}

/**
 * Form validate for time intervals
 */
function notifications_send_intervals_form_validate($form, &$form_state) {
  // Check the default is a valid one, not one to be deleted
  $default = $form_state['values']['default'];
  if (empty($form_state['values']['intervals'][$default]['unit']) || empty($form_state['values']['intervals'][$default]['name'])) {
    form_set_error('default', t('Invalid default send interval.'));
  }
}

/**
 * Form submit for time intervals
 */
function notifications_send_intervals_form_submit($form, &$form_state) {
  $form_values = $form_state['values'];
  if ($form_values['op'] == t('Reset to defaults')) {
    variable_del('notifications_send_intervals');
    variable_del('notifications_digest_methods');
    variable_del('notifications_default_send_interval');
  }
  else {
    $intervals = $digest = array();
    foreach ($form_values['intervals'] as $index => $values) {
      if (is_numeric($values['time']) && $values['name']) {
        $unit = $values['unit'] ? (int)$values['unit'] : 1;
        $time = (int)$values['time'] * $unit;
        $intervals[$time] = $values['name'];
        $build_methods[$time] = $values['build'];
        if ($index == $form_values['default']) {
          variable_set('notifications_default_send_interval', $time);
        }
      }
    }
    ksort($intervals);
    variable_set('notifications_send_intervals', $intervals);
    variable_set('notifications_digest_methods', $build_methods);
  }
  drupal_set_message(t('The time intervals for your subscriptions have been updated'));
  // Update orphaned notifications with invalid send interval
  $valid = array_keys($intervals);
  $params = array_merge(array(variable_get('notifications_default_send_interval', 0)), $valid);
  db_query('UPDATE {notifications} SET send_interval = %d WHERE send_interval NOT IN (' . db_placeholders($valid) . ')', $params);
  if ($updated = db_affected_rows()) {
    drupal_set_message(format_plural($updated, 'Updated a subscription with invalid interval.', 'Updated @count subscriptions with invalid intervals.'));
  }
  // Refresh strings after update if translation enabled
  if (module_exists('i18nstrings')) {
    notifications_locale_refresh();
  }
}

/**
 * Build a table with send intervals
 */
function theme_notifications_send_intervals_form($element) {
  $output = ''; //drupal_render($element['digest']);
  $header = array(array('data' => t('Time'), 'colspan' => 2), t('Display name'), t('Format'), t('Default'));
  foreach (element_children($element['intervals']) as $key) {
    $rows[] = array(
      drupal_render($element['intervals'][$key]['time']),
      drupal_render($element['intervals'][$key]['unit']),
      drupal_render($element['intervals'][$key]['name']),
      drupal_render($element['intervals'][$key]['build']),
      drupal_render($element['default'][$key]),
    );  
  }
  $element['intervals']['#value'] = theme('table', $header, $rows);
  //$output .= theme('table', $header, $rows);
  $output .= drupal_render($element);
  return $output;
}

/**
 * Event configuration administration
 * 
 * Will allow to define which events trigger a notification and which ones not
 */
function notifications_admin_events_form() {
  $form = array();
  // Compile array from plug-ins and settings
  if ($events = notifications_event_types()) {
    $form['notifications_event_enabled']['#tree'] = TRUE;
    $form['notifications_event_template']['#tree'] = TRUE;
    // Group event types when they have a parent
    $tree = notifications_admin_events_build_tree($events);
    notifications_admin_events_tree_form($tree, $form, 0);
    $form = system_settings_form($form);
    $form['#theme'] = 'notifications_table_form';
    $form['#table_fields'] = array('name', 'notifications_event_enabled', 'notifications_event_template');
    $form['#table_header'] = array(t('Event type'), t('Enabled'), t('Template'));
    return $form;
  } else {
    $form['warning'] = array('#value' => t('You need to enable some plug-ins to provide notifications events.'));
    return $form;
  }  
}

/**
 * Build event tree
 */
function notifications_admin_events_build_tree($events) {
  $tree = array();
  foreach ($events as $key => &$event) {
    if (empty($event['parent'])) {
      $tree[$key] = &$event;
    }
    else {
      $events[$event['parent']]['children'][$key] = &$event;
    }
  }
  return $tree;  
}
function notifications_admin_events_tree_form($tree, &$form, $depth = 0) {
  foreach ($tree as $key => $event) {
    $form['name'][$key] = array('#value' => theme('indentation', $depth) . $event['description']);
    $form['notifications_event_enabled'][$key] = array(
      '#type' => 'checkbox',
      '#default_value' => notifications_event_enabled($key),
    );
    $form['notifications_event_template'][$key] = array(
      '#type' => 'select',
      '#default_value' => notifications_event_get_template($key),
      '#options' => notifications_template_list(),
    );
    if (!empty($event['children'])) {
      notifications_admin_events_tree_form($event['children'], $form, $depth +1);
    }
  }  
}
function notifications_template_list() {
  $list = &messaging_static(__FUNCTION__);
  if (!isset($list)) {
    foreach (notifications_get_templates() as $key => $template) {
      $list[$key] = $template['name'];
    }
  }
  return $list;
}
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
