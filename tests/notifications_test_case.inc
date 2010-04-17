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
}