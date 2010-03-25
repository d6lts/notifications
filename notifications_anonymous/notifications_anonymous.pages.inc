<?php
// $Id$
/**
 * @file
 *   Notifications for anonymous users
 */

/**
 * Menu callback add subscription
 * 
 * As it needs an address every time, no need for signed pages
 */
function notifications_anonymous_page_subscribe($type, $fields, $values, $send_interval = NULL, $send_method = NULL) {
  $account = drupal_anonymous_user();

  // Build subscriptions object  
  $subscription = Notifications_Subscription::build((object)array(
    'uid' => $account->uid,
    'type' => $type,
    'fields' => notifications_field_args($fields, $values),
    'send_interval' => $send_interval,
    'send_method' => $send_method,
  ));
  $subscription->set_account($account);

  if (notifications_user_allowed('subscription', $account, $subscription)) {
    // Ask for confirmation
    drupal_set_title(t('Confirm your subscription'));
    return drupal_get_form('notifications_anonymous_form_subscribe', $subscription, $account);
  }
  else {
    drupal_set_message(t('Subscription type or parameters not allowed'), 'error');
    drupal_goto();
  }
    
  drupal_access_denied();
}

/**
 * Form for subscription confirmation
 */
function notifications_anonymous_form_subscribe($form_state, $subscription, $account) {
  $account = $account ? $account : drupal_anonymous_user();
  notifications_include('destination.inc');
  // Pass on simple values
  foreach (array('sid', 'uid', 'type', 'fields', 'event_type') as $field) {
    $form[$field] = array('#type' => 'value', '#value' => isset($subscription->$field) ? $subscription->$field : '');
  }
  // The subscription description will be added here
  $form['info'] = notifications_subscription_info_field($subscription); 
  // Additional parameters
  if ($send_intervals = notifications_send_intervals($account)) {
    if (count($send_intervals) == 1) {
      $form['send_interval'] = array('#type' => 'value', '#value' => key($send_intervals));
    }
    else {
      $form['send_interval'] = array(
        '#type' => 'select',
        '#title' => t('Send interval'),
        '#options' => notifications_send_intervals($account),
        '#default_value' => $subscription->send_interval,
      );      
    }
  }
  else {
    drupal_set_message(t('No send intervals enabled.'), 'error');
    return $form;
  }

  $destination = !empty($subscription->mdid) ? Messaging_Destination::load($subscription->mdid) : NULL;
  $form += notifications_destination_address_subform($account, $destination);
  $form['confirm'] = array('#type' => 'submit', '#value' => t('Subscribe'));
  $form['cancel'] = array('#type' => 'submit', '#value' => t('Cancel'));
  return $form;
}

/**
 * Subscription form validation
 */
function notifications_anonymous_form_subscribe_validate($form, &$form_state) {
  notifications_include('destination.inc');
  // Get destination from form
  notifications_destination_parse_submitted($form_state['values'], TRUE);

  if (empty($method) || empty($address)) {
    form_set_error('destination_address', t('You need exactly one destination address.'));
  }
  elseif ($destination = Messaging_Destination::build($method, $address, 0)) {
    $form_state['values']['destination'] = $destination;
    $form_state['values']['mdid'] = $destination->mdid;
  }
  else {
    // @todo What should we do if the address is not valid?
    form_set_error('address', t('This is not a valid destination address.'));
  }
}

/**
 * Process form submission
 */
function notifications_anonymous_form_subscribe_submit($form, &$form_state) {
  switch ($form_state['values']['op']) {
    case t('Subscribe'):
      $destination = Messaging_Destination::create($method, $address, 0);
      $subscription = Notifications_Subscription::build((object)$form_state['values']);
      $subscription->set_destination($destination);
      notifications_save_subscription($subscription);
      drupal_set_message(t('Your subscription has been saved.'));
      //$form_state['redirect'] = 'notifications/subscription/' . $subscription->sid;
      break;
    case t('Cancel'):
      drupal_set_message(t('Your subscription was cancelled'));
      //$form_state['redirect'] = 'user/'. $subscription->uid .'/notifications';
      break;
  }  
}

/**
 * Page callback. Provide a form to request to edit a destination
 */
function notifications_anonymous_request_page() {
  return 'Not implemented';
}

/**
 * Page callback. Manage anonymous destinations.
 */
function notifications_anonymous_destination_page($destination, $op = 'unsubscribe') {
  notifications_include('destination.inc');
  // Just for administrators or signed requests
  if (!user_access('administer notifications') && !notifications_check_signature()) {
    drupal_access_denied();
    exit(0);
  }
  drupal_set_title(t('Subscriptions for @name', array('@name' => $destination->address_name())));
  $options = notifications_anonymous_manage_links('destination', $destination);
  switch ($op) {
    case 'edit':
      $output .= drupal_get_form('notifications_destination_edit_form', $destination, $options);
      break;
    case 'delete':
    case 'unsubscribe':
      $output .= drupal_get_form('notifications_destination_unsubscribe_form', $destination, $options);
      break;
    case 'manage':
      $output .= drupal_get_form('notifications_destination_manage_form', $destination, $options);
      break;
  }
  return $output;
}

/**
 * Subform for destination data
 */
function notifications_anonymous_destination_subform($destination) {
  $options = notifications_anonymous_manage_links('manage', $destination);
  return notitications_destination_subform($destination, $options);
}

/**
 * Page callback. Manage anonymous subscriptions.
 */
function notifications_anonymous_subscription_page($subscription, $op = 'unsubscribe') {
  global $user;
  notifications_include('destination.inc');
  // Just for administrators or signed requests
  if (!user_access('administer notifications') && !notifications_check_signature()) {
    drupal_access_denied();
    exit(0);
  }
  $options = notifications_anonymous_subscription_links($destination);
  $account = drupal_anonymous_user();
  switch ($op) {
    case 'unsubscribe':
      $output .= drupal_get_form('notifications_anonymous_unsubscribe_form', $subscription);
      break;
    case 'edit':
      $output .= drupal_get_form('notifications_anonymous_subscription_form', $subscription);
      break;
  }
  return $output;
}

/**
 * Admin settings form
 */
function notifications_anonymous_admin_settings_form() {
  $form['notifications_anonymous_send_methods'] = array(
    '#title' => t('Allowed messaging methods'),
    '#type' => 'checkboxes',
    '#options' => messaging_method_list(),
    '#default_value' => variable_get('notifications_anonymous_send_methods', array()),
  );
  $form['notifications_anonymous_send_intervals'] = array(
    '#title' => t('Allowed send intervals'),
    '#type' => 'checkboxes',
    '#options' => _notifications_send_intervals(),
    '#default_value' => variable_get('notifications_anonymous_send_intervals', array()),  
  );
  // We cannot use form filter because of inmediate send interval is always filtered out
  //$form['array_filter'] = array('#type' => 'value', '#value' => TRUE);
  return system_settings_form($form);
}