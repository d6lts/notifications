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
    'send_interval' => $send_interval,
    'send_method' => $send_method,
  ));
  $subscription->set_account($account);
  $subscription->add_field_args($fields, $values);
  // Check permissions and if allowed, ask for confirmation
  if (notifications_user_allowed('subscription', $account, $subscription)) {
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
  // Pass on whole subscription
  $form['subscription'] = array('#type' => 'value', '#value' => $subscription);
  // Fields to be saved on submit
  $form['subscription_fields'] = array('#type' => 'value', '#value' => array());
  
  // The subscription description will be added here
  $form['info'] = $subscription->form_info(); 
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
  list($method, $address, $uid) = notifications_destination_parse_submitted($form_state['values'], TRUE);

  if (!$uid && $method && $address && ($destination = Messaging_Destination::build($method, $address, 0))) {
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
  $form['notifications_anonymous_send_methods'] = array(
    '#title' => t('Allowed messaging methods'),
    '#type' => 'checkboxes',
    '#options' => messaging_method_list(),
    '#default_value' => variable_get('notifications_anonymous_send_methods', array('mail')),
  );
  $form['notifications_anonymous_send_intervals'] = array(
    '#title' => t('Allowed send intervals'),
    '#type' => 'checkboxes',
    '#options' => _notifications_send_intervals(),
    '#default_value' => variable_get('notifications_anonymous_send_intervals', array(0)),  
  );
  // We cannot use form filter because of inmediate send interval is always filtered out
  //$form['array_filter'] = array('#type' => 'value', '#value' => TRUE);
  return system_settings_form($form);
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
