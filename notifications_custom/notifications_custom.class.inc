<?php
// $Id$
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */

/**
 * Message destination class
 */
class Notifications_Custom_Subscription extends Notifications_Subscription {
  // Unique custom subscription id
  public $csid;

  // Get name
  function get_name() {
    return check_plain(notifications_translate("custom:$this->type:name", $this->get_type('name')));
  }  
  // Get title
  function get_title() {
    return check_plain(notifications_translate("custom:$this->type:title", $this->get_type('title')));
  }  
  // Get description
  function get_description() {
    return check_plain(notifications_translate("custom:$this->type:title", $this->get_type('description')));
  }
  /**
   * Get default value for a possible checkbox
   */
  function get_default($user_registration = FALSE) {
    return $this->is_instance() || $user_registration && $this->get_type('default_value');
  }
  /**
   * Get instance of this one for a given user
   */
  function get_instance($account) {
    if ($instance = notifications_custom_get_subscription($this->type, $account->uid)) {
      return $instance;
    }
    else {
      // If not instance, returns itself, which is a template
      return $this;
    }
  }
  /**
   * Check visibility of this subscription type
   */
  function is_visible() {
    return user_access('administer users') || $this->get_type('visibility');
  }
  /**
   * Format as a form element, which may be a visible checkbox or a hidden field
   */
  function form_element($user_registration = FALSE) {
    return $this->is_visible() ? $this->form_checkbox($user_registration) : $this->form_value();
  }
  
  /**
   * Format as a form checkbox
   */
  function form_checkbox($user_registration = FALSE) {
    return array(
      '#type' => 'checkbox',
      '#title' => $this->get_name(),
      '#default_value' => $this->get_default($user_registration),
      '#description' => $this->get_description(),
    );    
  }
  /**
   * Format as a form value
   */
  function form_value() {
    return array(
      '#type' => 'value',
      '#value' => $this->type,
    );
  }
  
}