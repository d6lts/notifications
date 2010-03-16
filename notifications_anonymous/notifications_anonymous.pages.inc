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
  $subscription = (object)array(
    'uid' => $account->uid,
    'type' => $type,
    'fields' => notifications_field_args($fields, $values),
    'send_interval' => $send_interval ? $send_interval : notifications_user_setting('send_interval', $account),
    'send_method' => $send_method ? $send_method : notifications_user_setting('send_method', $account),
    'event_type' => notifications_subscription_types($type, 'event_type'),
  );      
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
  // Pass on simple values
  foreach (array('sid', 'uid', 'type', 'fields', 'event_type') as $field) {
    $form[$field] = array('#type' => 'value', '#value' => isset($subscription->$field) ? $subscription->$field : '');
  }
  // The subscription description will be added here
  $form['info'] = notifications_subscription_info_field($subscription); 
  // Additional parameters
  $form['send_interval'] = array(
    '#type' => 'select',
    '#title' => t('Send interval'),
    '#options' => notifications_send_intervals(),
    '#default_value' => $subscription->send_interval,
  );
  $form += _notifications_anonymous_address_subform();
  $form['confirm'] = array('#type' => 'submit', '#value' => t('Subscribe'));
  $form['cancel'] = array('#type' => 'submit', '#value' => t('Cancel'));
  return $form;
}

/**
 * Subscription form validation
 */
function notifications_anonymous_form_subscribe_validate($form, &$form_state) {
  // Get destination from form
  $destination = $form_state['values']['destination_address'];
  if (is_array($destination)) {
    $destination = array_filter($destination);
    if ($destination && count($destination) == 1) {
      $method = key($destination);
      $address = current($destination);
    }
  }
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
  $subscription = (object)$form_state['values'];
  switch ($form_state['values']['op']) {
    case t('Subscribe'):
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
 * Page callback
 */
function notifications_anonymous_overview_page() {

}

/**
 * Page callback
 */
function notifications_anonymous_destination_page($destination) {

}

/**
 * Page callback, unsubscribe
 */
function notifications_anonymous_unsubscribe_page($destination) {
  
}

/**
 * Admin settings form
 */
function notifications_anonymous_settings_form() {
  $form['notifications_anonymous_send_methods'] = array(
    '#title' => t('Allowed messaging methods'),
    '#type' => 'checkboxes',
    '#options' => messaging_method_list(),
    '#default_value' => variable_get('notifications_anonymous_send_methods', array()),
  );
  $form['array_filter'] = array('#type' => 'value', '#value' => TRUE);
  return system_settings_form($form);
}