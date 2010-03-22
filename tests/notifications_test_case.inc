<?php
// $Id$

// Some utility functions need this one
module_load_include('cron.inc', 'notifications');
/**
 * @file
 * Base class for Notifications Tests
 */
class NotificationsTestCase extends DrupalWebTestCase {
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
   * Print helper for debugging
   */ 
  function printDebug($data, $title = 'DEBUG') {
    $string = is_array($data) || is_object($data) ? '<pre>' . print_r($data, TRUE) . '</pre>' : $data;
    $this->assertTrue(TRUE, $string, $title);
  }
  // Debug dump object with some formatting
  function printObject($object, $title = 'DEBUG Object', $max_depth = NULL) {
    $output = $this->formatTable($object, $max_depth);
    $this->printDebug($output, $title);
  }
  // Format object as table, recursive
  function formatTable($object, $max_depth = NULL) {
    if (isset($max_depth) && !$max_depth) {
      // Reached max depth, print out in debug format
      return '<pre>' . print_r($object, TRUE) . '</pre>';
    }
    foreach ($object as $key => $value) {
      $rows[] = array(
        $key,
        is_array($value) || is_object($value) ? $this->formatTable($value, isset($max_depth) ? $max_depth - 1 : NULL) : $value,
      );
    }
    if (!empty($rows)) {
      return theme_table(array(), $rows);
    }
    else {
      return 'No properties';
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
  // Dump table contents
  function dumpTable($table) {
    $result = db_query('SELECT * FROM {' . $table . '}');
    $output = '';
    while ($row = db_fetch_array($result)) {
      $rows[] = $row;
      if (empty($header)) {
        $header = array_keys($row);
      }
    }
    if (!empty($rows)) {
      // Need to call theme function directly
      $output .= theme_table($header, $rows);
    } else {
      $output .= ' No rows';
    }
    $this->printDebug($output, "Table dump: $table");
  }
  // Dump Notifications log
  function dumpLogs() {
    if ($logs = messaging_log_get()) {
      $rows = array();
      foreach ($logs as $log) {
        list($type, $string, $append, $objects) = _messaging_log_format($log);
        $rows[] = array($type, $string, $this->formatTable($append), $this->formatTable($objects));  
      }
      $this->printDebug(theme_table(array(), $rows), 'Notifications Log');
    }
  }
}