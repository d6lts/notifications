<?php
// $Id$
/**
 * @file
 *   Notifications for anonymous users
 */

// We will reuse some of the main Notifications form
require_once drupal_get_path('module', 'notifications') . '/notifications.pages.inc';

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
    'send_interval' => $send_interval,
    'send_method' => $send_method,
  ));
  $subscription->set_account($account);
  $subscription->add_field_args($fields, $values);
  // Check permissions and if allowed, ask for confirmation
  if (notifications_user_allowed('subscription', $account, $subscription)) {
    drupal_set_title(t('Confirm your subscription'));
    return drupal_get_form('notifications_anonymous_form_subscribe', $subscription);
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
function notifications_anonymous_form_subscribe($form_state, $subscription) {
  notifications_include('destination.inc');
  $form = notifications_form_subscribe_confirm($form_state, $subscription);
  if (!empty($form['#error'])) {
    return $form;
  }
  // Add a destination field so the user can fill in a destination address
  $form['send_method'] = notifications_destination_address_subform($account, $subscription->get_destination());
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
  list($method, $address, $uid) = notifications_destination_parse_submitted($form_state['values'], TRUE);

  if (!$uid && $method && $address && ($destination = Messaging_Destination::build($method, $address, 0))) {
    $form_state['values']['send_method'] = $method;
    $form_state['values']['destination'] = $destination;
    $form_state['values']['mdid'] = $destination->mdid;
  }
  elseif (empty($method) || empty($address)) {
    form_set_error('destination_address', t('You need exactly one destination address.'));
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
      list($method, $address, $uid) = notifications_destination_parse_submitted($form_state['values']);
      $destination = Messaging_Destination::create($method, $address, $uid);
      $subscription = Notifications_Subscription::build_submission($form_state);
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
  $output = '<p>' . t('If you want to unsubscribe or manage subscriptions for a destination, enter the address here and click on Submit.') . '</p>';
  $output .= '<p>' . t('You will get a message with a link that will allow you to manage your subscriptions.') . '</p>';
  $output .= drupal_get_form('notifications_anonymous_request_form');
  return $output;  
}

/**
 * Request form
 */
function notifications_anonymous_request_form() {
  notifications_include('destination.inc');
  global $user;

  $form = notifications_destination_address_subform($user);
  $form['manage'] = array('#type' => 'submit', '#value' => t('Edit my subscriptions'));
  $form['unsubscribe'] = array('#type' => 'submit', '#value' => t('Unsubscribe my address'));
  return $form;
}

/**
 * Request form submit
 */
function notifications_anonymous_request_form_submit($form, $form_state) {
  global $user;

  list($method, $address, $uid) = notifications_destination_parse_submitted($form_state['values']);
  if ($method && $address) {
    $destination = Messaging_Destination::build($method, $address, $uid);
    if ($destination && $destination->mdid && (!$destination->uid || $user->uid == $destination->uid)) {
      drupal_set_message(t('A message with instructions has been sent to your address.'));
      switch ($form_state['values']['op']) {
        case t('Edit my subscriptions'):
          notifications_anonymous_message('manage', $destination);
          return TRUE;
        case t('Unsubscribe my address'):
          notifications_anonymous_message('unsubscribe', $destination);
          return TRUE;
      }
    }
  }
  else {
    drupal_set_message(t('Invalid address.'), 'error');
  }
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
  return notifications_destination_subform($destination, $options);
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
  $anonymous_methods = array();
  foreach (messaging_method_info() as $method => $info) {
    if (!empty($info['anonymous'])) {
      $anonymous_methods[$method] = check_plain($info['name']);
    }
  }
  $form['notifications_anonymous_send_methods'] = array(
    '#title' => t('Allowed messaging methods'),
    '#type' => 'checkboxes',
    '#options' => $anonymous_methods,
    '#default_value' => variable_get('notifications_anonymous_send_methods', array('mail')),
    '#description' => t('Select send methods allowed for anonymous users. Only one of each type (I.e. only one mail method) is allowed.'),
  );
  $form['notifications_anonymous_send_intervals'] = array(
    '#title' => t('Allowed send intervals'),
    '#type' => 'checkboxes',
    '#options' => _notifications_send_intervals(),
    '#default_value' => variable_get('notifications_anonymous_send_intervals', array(0)),  
  );
  // We cannot use form filter because of inmediate send interval is always filtered out
  //$form['array_filter'] = array('#type' => 'value', '#value' => TRUE);
  $form = system_settings_form($form);
  $form['#validate'][] = 'notifications_anonymous_admin_settings_form_validate';
  return $form;
}

/**
 * Validate admin settings
 */
function notifications_anonymous_admin_settings_form_validate($form, &$form_state) {
  $send_methods = array_filter($form_state['values']['notifications_anonymous_send_methods']);
  $form_state['values']['notifications_anonymous_send_methods'] = $send_methods;
  // Check we have only one of each type
  $types = array();
  foreach ($send_methods as $method) {
    $type = messaging_method_info($method, 'address_type');
    if (in_array($type, $types)) {
      form_set_error('notifications_anonymous_send_methods', t('Only one method of each type is allowed.'));
      break;
    }
    $types[] = $type;
  }
}

/**
 * Destination form submitted
 */
function notifications_anonymous_destination_form_submit(&$form, &$form_state) {
  $account = $form_state['values']['account'];
  $method = $form_state['values']['method'];
  $address = $form_state['values']['address'];
  $destination = Messaging_Destination::get(array('method' => $method, 'address' => $address));
  if ($destination) {
    if ($account && $account->uid && $destination->uid == $account->uid) {
      // The address belongs to the current user, so go for it
      notifications_delete_destination($destination->mdid);
      drupal_set_message(t('All the subscriptions for that address have been deleted.'));
      return;
    }
    else {
      // Will ask for confirmation if enabled always or if it belongs to a user
      $confirm = variable_get('notifications_anonymous_unsubscribe_confirm', 1);
      notifications_anonymous_message('unsubscribe', $destination, $confirm || $destination->uid);
    }
  }
  // We print out a message without undisclosing any information about whether the address exists or not
  drupal_set_message(t('If your address is found on this system, a message will be sent with a link to delete all subscriptions.'));
}
