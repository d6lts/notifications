<?php
// $Id$

// Base class and some utility functions need this one
require_once drupal_get_path('module', 'messaging') . '/tests/messaging_test_case.inc';

/**
 * @file
 * Base class for Notifications Tests
 */
class NotificationsTestCase extends MessagingTestCase {
  /**
   * Set up some required modules
   */
  function setUp() {
    $modules = func_get_args();
    $modules = array_unique(array_merge(array('token', 'messaging_template', 'notifications'), $modules));
    call_user_func_array(array('parent', 'setUp'), $modules);
  }
  /**
   * Helper function. Simple row counting with conditions, uses query builder
   */
  function countQueued($params = NULL) {
    if ($params) {
      $query = notifications_queue_query($params);
      return db_result(db_query('SELECT COUNT(*) FROM {notifications_queue} WHERE ' . implode(' AND ', $query['where']), $query['args']));
    } else {
      return db_result(db_query('SELECT COUNT(*) FROM {notifications_queue}'));
    }
  }
  /**
   * Helper function to create a subscription
   */
  function contentCreateSubscription($user, $type, $fields, $confirm_texts = array(), $post = array()) {
    // Create a link for the subscription confirmation page
    $link = notifications_get_link('subscribe', array('uid' => $user->uid, 'type' => $type, 'fields' => $fields));
    $this->drupalGet($link['href'], $link['options']);
    $this->assertText(t('Confirm your subscription'), 'Subscriptions confirmation page is shown');
    foreach ($confirm_texts as $text) {
      $this->assertRaw($text);
    }
    // Submit the form and check confirmation page
    $this->drupalPost(NULL, $post, t('Subscribe'));
    $this->assertText(t('Your subscription has been created.')); 
  }
  // Enable all UI optional pages
  function enableUIPages($enable = TRUE) {    
    $settings = array_keys(notifications_subscription_types());
    variable_set('notifications_ui_types', $enable ? $settings : array());
    variable_set('notifications_ui_user_options', $enable ? array('page', 'create'): array());
    menu_rebuild();
  }
  // Enable content Subscriptions for all
  function enableSubscriptionTypes($enable = TRUE) {
    $settings = array_keys(notifications_subscription_types());
    variable_set('notifications_content_type', $enable ? $settings : array());
  }
}