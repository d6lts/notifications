<?php
// $Id$

// Base class and some utility functions need this one
require_once drupal_get_path('module', 'messaging') . '/tests/messaging_testcase.inc';
module_load_include('cron.inc', 'notifications');
/**
 * @file
 * Base class for Notifications Tests
 */
class NotificationsTestCase extends MessagingTestCase {
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

  // Assert number of rows in table
  function assertTableRows($table, $target, $conditions = NULL, $message = NULL) {
    $sql = 'SELECT COUNT(*) FROM {' . $table . '}';
    $print = $where = array();
    if ($conditions) {
      foreach ($conditions as $field => $value) {
        $where[] = "$field = '%s'";
        $print[] = "$field=$value";
      }
    }
    if (!empty($where)) {
      $sql .= ' WHERE ' . implode(' AND ', $where);
    }
    $count = db_result(db_query($sql, $conditions));
    $message = $message ? $message : 'Right number of rows in table ' . $table .'[' . implode(', ', $print) . ']=' . $target;
    $message .= " ($count)";
    $this->assertEqual($count, $target, $message);
  }
  // Assert number of rows in table for a user
  function assertUserRows($table, $target, $uid, $message = NULL) {
    $message = $message ? $message : "There are $target rows in table $table for user $uid";
    $this->assertTableRows($table, $target, array('uid' => $uid), $message);
  }

}